#include "Application.h"

void Application::begin() {
  Serial.begin(115200);
  delay(100); 
  pinMode(BoardConfig::Button, INPUT_PULLUP);
  climate_.begin(); // Reserve the persistent weather worker before network allocations.

  const bool boardReady = board_.begin();
  if (boardReady) power_.begin();
  display.begin();
  if (boardReady && !touch_.begin()) Serial.println("[TOUCH] Indisponivel; use BOOT ou tela_next");
  sdDisponivel = commandProcessor.begin();
  display.clear();
  display.println("Sistema Inicializando...");
  delay(200); 

  delay(100);

  rgbLed.begin();
  gateway.begin();
  ntp.initNTP();
  tempoUltimoComando = millis();
  if (xTaskCreatePinnedToCore(run, "application", 16384, this, 1, &task_, BoardConfig::ApplicationCore) != pdPASS) {
    Serial.println("[FATAL] Falha ao criar tarefa no core 1");
    abort();
  }
  Serial.println("[Serial] Console pronto: 115200 baud, termine com Enter. Digite help.");

}
uint8_t Application::adjacentPage(uint8_t page, int direction) const {
    do { page = (int(page) + PAGE_COUNT + direction) % PAGE_COUNT; }
    while (page == PAGE_NTP_2 || page == PAGE_NET_2);
    return page;
}
void Application::handleTouch() {
    TouchInput::Event event;
    while (touch_.poll(event)) {
        const auto gesture=event.gesture;
        if (gesture == TouchInput::Gesture::Down) {
            touchWakeOnly_ = (currentPage != PAGE_ANALOG && display.idleFor(60000)) || display.brightness() == 0;
            if (display.brightness() == 0) display.setBacklight(80);
            display.wake(); tempoUltimoComando = millis(); lastNavigationAt = millis();
            continue;
        }
        if (touchWakeOnly_) { touchWakeOnly_ = false; continue; }
        switch (gesture) {
        case TouchInput::Gesture::Home: currentPage = (currentPage==PAGE_CLOCK || currentPage==PAGE_ANALOG)?PAGE_SETTINGS:PAGE_ANALOG; break;
        case TouchInput::Gesture::Previous: currentPage = adjacentPage(currentPage,-1); break;
        case TouchInput::Gesture::Tap:
            if(currentPage==PAGE_SETTINGS)display.settingsTap(event.x,event.y);
            else if(currentPage==PAGE_MONITOR)display.cycleMonitorView();
            else continue;
            break;
        case TouchInput::Gesture::Next:
            currentPage = adjacentPage(currentPage); break;
        default: continue;
        }
        display.wake(); lastNavigationAt = millis();
        returnToClockArmed = currentPage != PAGE_CLOCK && currentPage != PAGE_ANALOG && currentPage != PAGE_SETTINGS;
        consoleVisible = false; lastFrame = millis()-1000;
    }
}
void Application::tick() {
  if (power_.takeShutdownRequest()) { shutdown(); return; }
  ntp.update();
  display.setTimeSource(ntp.sourceLabel());
  power_.updateBattery();
  const auto battery=power_.batterySnapshot();
  display.setBatterySnapshot(battery);
  if(!lastClockRead_ || millis()-lastClockRead_>=1000) {
    lastClockRead_=millis();clockTimestamp_=ntp.getDateTime(0);
    display.updateLocalHour(clockTimestamp_);
  }
  if(batteryHistory_.update(battery) && commandProcessor.sdReady())
    batteryHistory_.archive(sd,clockTimestamp_,battery);
  const auto movement=motion_.update(display.settings().motionEnabled,display.rotation());
  if(movement.wake && display.brightness()>0) {
    display.wake();tempoUltimoComando=millis();lastFrame=millis()-1000;
  }
  String comando;

  // Handle the OTA invitation and transfer before HTTP rendering and peripherals.
  if (WiFi.status() == WL_CONNECTED) {
    if (!otaInicializadoCompleto) {
      ota_.begin(BoardConfig::Hostname);
      otaInicializadoCompleto = true;
    }
    ota_.handle();
  }

  handleTouch();
  if(movement.rotation>=0 && buttonState!=LOW && !consoleVisible && !touch_.contactActive()) {
    display.setRotation(uint8_t(movement.rotation));lastFrame=millis()-1000;
  }

  gateway.handleClient();
  commandProcessor.update();  

  int buttonReading = digitalRead(BoardConfig::Button);
  if (buttonReading != lastButtonReading)
  {
    lastDebounceTime = millis();
  }
  if (millis() - lastDebounceTime >= 50 && buttonReading != buttonState)
  {
    buttonState = buttonReading;
    if (buttonState == LOW)
    {
      display.wake();
      buttonPressedAt = millis();
      lastNavigationAt = buttonPressedAt;
      holdStage = 0;
    }
    else if (holdStage == 0)
    {
      Serial.printf("[STATUS] Core=%d WiFi=%s Heap=%u\n", xPortGetCoreID(), WiFi.status() == WL_CONNECTED ? "OK" : "offline", ESP.getFreeHeap());
      currentPage = adjacentPage(currentPage);
      lastNavigationAt = millis();
      returnToClockArmed = currentPage != PAGE_CLOCK && currentPage != PAGE_ANALOG && currentPage != PAGE_SETTINGS;
      consoleVisible = false;
      lastFrame = millis() - 1000;
      tempoUltimoComando = millis();
    }
    else if (holdStage == 1)
    {
      display.showHoldMessage("REINICIANDO", "", 0x07FF);
      delay(500);
      ESP.restart();
    }
    else
    {
      display.showHoldMessage("WI-FI APAGADO", "Reiniciando...", 0xF800);
      delay(500);
      gateway.clearConfig();
    }
  }
  lastButtonReading = buttonReading;

  if (buttonState == LOW)
  {
    lastNavigationAt = millis();
    uint32_t held = millis() - buttonPressedAt;
    if (held >= 10000 && holdStage < 2)
    {
      holdStage = 2;
      rgbLed.red();
      display.showHoldMessage("APAGAR WI-FI", "Solte p/ confirmar", 0xF800);
    }
    else if (held >= 3000 && holdStage < 1)
    {
      holdStage = 1;
      rgbLed.blue();
      display.showHoldMessage("REINICIAR", "Solte (ou segure 10s)", 0x07FF);
    }
    tempoUltimoComando = millis();
  }
  else if (commandProcessor.ledBusy())
  {
    lastLedState = -1;
  }
  else
  {
    int8_t ledState = WiFi.status() == WL_CONNECTED ? 0 : ((WiFi.getMode() & WIFI_AP) ? 1 : 2);
    if (ledState != lastLedState)
    {
      lastLedState = ledState;
      if (ledState == 0) rgbLed.green();
      else if (ledState == 1) rgbLed.yellow();
      else rgbLed.red();
    }
  }

  if (otaInicializadoCompleto && WiFi.status() == WL_CONNECTED)
  {
    climate_.atualizar();
  }

  // Comandos UDP e serial usam o mesmo processamento.
  const bool serialCommand = serialConsole.poll(Serial, comando);
  if (serialCommand || gateway.receiveCommand(comando))
  {
    display.wake();
    commandProcessor.executeCommand(comando, serialCommand);
    sdDisponivel = commandProcessor.sdReady();
    consoleVisible = true; consoleUntil = millis() + 10000;
    tempoUltimoComando = millis();
  }

  int request = display.takePageRequest();
  if (request != -1) {
    currentPage = request == -2 ? adjacentPage(currentPage) : request % PAGE_COUNT;
    lastNavigationAt = millis();
    returnToClockArmed = currentPage != PAGE_CLOCK && currentPage != PAGE_ANALOG && currentPage != PAGE_SETTINGS;
    consoleVisible = false; lastFrame = millis() - 1000;
  }
  if (returnToClockArmed && currentPage != PAGE_ANALOG && buttonState != LOW && millis() - lastNavigationAt >= 20000UL) {
    currentPage = PAGE_CLOCK;
    returnToClockArmed = false;
    consoleVisible = false;
    lastFrame = millis() - 1000;
  }
  if (consoleVisible && int32_t(millis() - consoleUntil) >= 0) consoleVisible = false;
  static constexpr uint8_t positions[PAGE_COUNT] = {0,1,1,2,2,3,4,5,6,7,8,9,10};
  display.setPageIndex(positions[currentPage]);
  display.setContinuousClock(currentPage==PAGE_ANALOG);
  display.updatePower();
  const bool idle = currentPage != PAGE_ANALOG && buttonState != LOW && !consoleVisible && display.idleFor(60000);
  if (idle) {
    screensaverActive = true;
    display.desenharOlhosScreensaver();
  } else if (screensaverActive) {
    screensaverActive = false;
    lastFrame = millis() - 1000;
    if (buttonState == LOW) display.showClock(ntp.getDateTime(10));
  }
  if (!idle && buttonState != LOW && !consoleVisible && millis() - lastFrame >= 1000) {
    lastFrame = millis();
    const String clockTime=ntp.getDateTime(0);
    display.setTimeSource(ntp.sourceLabel());
    switch (currentPage) {
      case PAGE_CLOCK: display.showClock(clockTime); break;
      case PAGE_ANALOG: display.showAnalogClock(clockTime,power_.batterySnapshot(),ntp.getFusoHora(),ntp.isDstAtivo()); break;
      case PAGE_NTP_1: case PAGE_NTP_2:
        display.showStatusPage(sdDisponivel, ntp.isSincronizado(), clockTime, ntp.getFusoHora(), ntp.isDstAtivo()); break;
      case PAGE_NET_1: case PAGE_NET_2: display.showNetworkPage(); break;
      case PAGE_SYS: display.showSystemPage(); break;
      case PAGE_WIFI: {
        String ssids[5]; int slot = -1;
        for (int i=0;i<5;++i) { ssids[i]=gateway.savedSsid(i); if(WiFi.status()==WL_CONNECTED && ssids[i]==WiFi.SSID())slot=i; }
        display.showSavedWifiPage(ssids,slot,gateway.nextSlot()); break;
      }
      case PAGE_SD: {
        const auto card = sd.metricsSnapshot();
        display.showSdPage(sdDisponivel,sd.getCardType(),card.valid?card.total:card.capacity,card.used); break;
      }
      case PAGE_MONITOR: display.showMonitorPage(); break;
      case PAGE_FORECAST: display.showForecastPage(); break;
      case PAGE_BATTERY: display.showBatteryHistory(batteryHistory_); break;
      case PAGE_SETTINGS: display.showSettingsPage(); break;
    }
  }
  vTaskDelay(pdMS_TO_TICKS(5));

}
void Application::shutdown() {
    Serial.println("[POWER] Encerrando SD e solicitando desligamento ao AXP2101");
    climate_.pauseForOta(true);
    touch_.pause(true);
    display.showPowerOff();
    sd.end(); // Wait for in-flight file operations, then unmount before the PMU cuts power.
    sdDisponivel=false;
    vTaskDelay(pdMS_TO_TICKS(250));
    display.setBacklight(0);
    const bool accepted=power_.shutdown();
    vTaskDelay(pdMS_TO_TICKS(1000));
    // If execution continues, the PMU did not remove the MCU supply (e.g. external power).
    Serial.printf("[POWER] CPU ainda ativa; comando=%s. Segure PWR por 6s.\n",accepted?"ACK":"falha I2C");
    sdDisponivel=commandProcessor.begin();
    touch_.pause(false);
    climate_.pauseForOta(false);
    display.setBacklight(80);
    display.showHoldMessage("ENERGIA AINDA ATIVA","Segure PWR por 6s",0xFFE0);
    consoleVisible=true; consoleUntil=millis()+5000;
}
void Application::run(void* context) {
    auto& self = *static_cast<Application*>(context);
    Serial.printf("[CPU] Aplicacao no core %d; touch e clima no core 0\n", xPortGetCoreID());
    for (;;) self.tick();
}
