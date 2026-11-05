#include "BoardPower.h"
#include <esp_timer.h>

bool BoardPower::update(uint8_t reg, uint8_t mask, uint8_t value) {
    BoardHardware::Lock lock(board_);
    uint8_t previous=0;
    return read(reg,previous) && board_.writeRegister(Address,reg,(previous & uint8_t(~mask)) | (value & mask));
}
bool BoardPower::begin() {
    BoardHardware::Lock lock(board_);
    uint8_t id=0;
    if (!read(0x03,id) || id!=0x4A) {
        Serial.printf("[POWER] AXP2101 nao encontrado (ID=0x%02X)\n",id); return false;
    }
    // PEKEY: power on after 1 s; hardware emergency power-off after 6 s.
    // 0x22: enable long-press shutdown, select power-off rather than restart.
    // 0x10 bit 2: allow PWRON to shut down the PMIC.
    ready_ = update(0x27,0x0F,0x06) && update(0x22,0x03,0x02) &&
             update(0x10,0x04,0x04) &&
             board_.writeRegister(Address,0x49,KeyShort) && update(0x41,KeyShort,KeyShort);
    // Enable battery-presence detection and the battery-voltage ADC only.
    // Charging current, target voltage and fuel-gauge calibration remain untouched.
    telemetryReady_=update(0x68,0x01,0x01) && update(0x30,0x01,0x01);
    battery_.available=telemetryReady_;
    Serial.println(telemetryReady_?"[BATTERY] Telemetria habilitada":"[BATTERY] Falha ao habilitar telemetria");
    startedAt_=millis();
    Serial.println(ready_ ? "[POWER] PWR curto: desligar; 1s: ligar; 6s: emergencia" : "[POWER] Falha ao configurar PWR");
    return ready_;
}
bool BoardPower::takeShutdownRequest() {
    if (!ready_ || millis()-lastPoll_<50) return false;
    lastPoll_=millis();
    BoardHardware::Lock lock(board_);
    uint8_t status=0;
    if (!read(0x49,status) || !(status & KeyShort)) return false;
    // W1C only the event we consumed; do not clear unrelated PMU events.
    if (!board_.writeRegister(Address,0x49,KeyShort)) return false;
    return millis()-startedAt_>=1500; // Discard a short event from the wake-up press.
}
bool BoardPower::shutdown() {
    return ready_ && update(0x10,0x01,0x01);
}

void BoardPower::updateBattery(bool force) {
    if (!telemetryReady_ || (!force && lastBatteryAt_ && millis()-lastBatteryAt_<5000)) return;
    lastBatteryAt_=millis();
    BatterySnapshot next{}; next.available=true; next.updatedAt=lastBatteryAt_;
    BoardHardware::Lock lock(board_);
    uint8_t status[2]{};
    if (!board_.readRegisters(Address,0x00,status,sizeof(status))) {
        battery_=next; dischargeTracking_=false; return;
    }
    next.valid=true;
    next.connected=(status[0] & 0x08)!=0;
    next.usb=(status[0] & 0x20)!=0 && (status[1] & 0x08)==0;
    const uint8_t mode=(status[1]>>5) & 0x03;
    next.charging=next.connected && mode==1;
    next.discharging=next.connected && mode==2;
    if (next.connected) {
        uint8_t percent=0, voltage[2]{};
        if (read(0xA4,percent) && percent<=100) { next.percent=percent; next.percentValid=true; }
        if (board_.readRegisters(Address,0x34,voltage,sizeof(voltage))) {
            next.millivolts=(uint16_t(voltage[0] & 0x1F)<<8) | voltage[1];
            next.voltageValid=next.millivolts>=2000 && next.millivolts<=5000;
        }
    }
    // Observe fuel-gauge change only during uninterrupted battery discharge.
    if (next.percentValid && next.discharging && !next.usb) {
        const int64_t now=esp_timer_get_time();
        if (!dischargeTracking_ || next.percent>dischargeStartedPercent_) {
            dischargeTracking_=true; dischargeStartedUs_=now; dischargeStartedPercent_=next.percent;
        }
        const int64_t elapsed=now-dischargeStartedUs_;
        next.observedSeconds=uint32_t(elapsed/1000000);
        if (elapsed>=600LL*1000000 && next.percent<dischargeStartedPercent_)
            next.dischargePercentPerHour=(dischargeStartedPercent_-next.percent)*3600000000.0/elapsed;
    } else dischargeTracking_=false;
    battery_=next;
}
String BoardPower::batteryReport() {
    updateBattery(true);
    const auto b=batterySnapshot();
    if (!b.available || !b.valid) return "Bateria: telemetria indisponivel\n";
    String report="===== BATERIA =====\nUSB: "+String(b.usb?"conectado":"desconectado")+"\n";
    if (!b.connected) return report+"Bateria nao detectada\n";
    report+="Estado: "+String(b.charging?"carregando":b.discharging?"descarregando":"em espera")+"\n";
    if (b.percentValid) {
        report+="Carga restante: "+String(b.percent)+"% (estimativa AXP2101)\n";
        report+="Capacidade usada desde 100%: "+String(100-b.percent)+"%\n";
    } else report+="Carga restante: indisponivel\n";
    if (b.voltageValid) report+="Tensao: "+String(b.millivolts/1000.0f,3)+" V\n";
    if (isfinite(b.dischargePercentPerHour))
        report+="Consumo estimado: "+String(b.dischargePercentPerHour,1)+" pontos percentuais/h\n";
    else report+="Consumo estimado: aguardando 10 min e queda de carga sem USB\n";
    report+="Janela de descarga: "+String(b.observedSeconds/60)+" min\n";
    report+="Corrente/potencia instantaneas (mA/mW): nao medidas\n";
    return report;
}
