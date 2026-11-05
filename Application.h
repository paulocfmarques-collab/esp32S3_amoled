#pragma once
#include <Arduino.h>
#if !defined(CONFIG_IDF_TARGET_ESP32S3) || CONFIG_FREERTOS_UNICORE
#error "Selecione ESP32-S3 com dois nucleos habilitados"
#endif
#include "BoardHardware.h"
#include "BoardPower.h"
#include "BoardRtc.h"
#include "TouchInput.h"
#include "RGBLed.h"
#include "DisplayUtil.h"
#include "ESP32Gateway.h"
#include "NTPUtil.h"
#include "SDUtil.h"
#include "CommandProcessor.h"
#include "OTAManager.h"
#include "ClimaManager.h"
#include "SerialConsole.h"
#include "MotionGestures.h"
#include "BatteryHistory.h"

// Raiz da aplicação: todos os serviços são membros com tempo de vida comum.
class Application final {
public:
    void begin();
private:
    BoardHardware board_;
    BoardPower power_{board_};
    BoardRtc rtc_{board_};
    MotionGestures motion_{board_};
    BatteryHistory batteryHistory_;
    uint32_t lastClockRead_=0;
    String clockTimestamp_;
    ClimaManager climate_;
    TouchInput touch_{board_};
    DisplayUtil display{board_, touch_, climate_};
    RGBLed rgbLed{display};
    SDUtil sd;
    ESP32Gateway gateway{display, sd, rgbLed, climate_};
    NTPUtil ntp{rtc_};
    CommandProcessor commandProcessor{display, gateway, ntp, rgbLed, sd, climate_, power_};
    SerialConsole serialConsole;
    OTAManager ota_{display, climate_};
    TaskHandle_t task_ = nullptr;
    bool otaInicializadoCompleto = false, sdDisponivel = false;
    uint32_t tempoUltimoComando = 0;
    enum { PAGE_CLOCK=0, PAGE_NTP_1, PAGE_NTP_2, PAGE_NET_1, PAGE_NET_2,
        PAGE_SYS, PAGE_WIFI, PAGE_SD, PAGE_MONITOR, PAGE_FORECAST,
        PAGE_ANALOG=ScreenLayout::AnalogPage, PAGE_BATTERY, PAGE_SETTINGS, PAGE_COUNT };
    uint8_t currentPage = 0;
    uint32_t lastFrame = 0, consoleUntil = 0;
    bool consoleVisible = false;
    bool screensaverActive = false;
    int lastButtonReading = HIGH;
    int buttonState = HIGH;
    uint32_t lastDebounceTime = 0;
    uint32_t buttonPressedAt = 0;
    uint32_t lastNavigationAt = 0;
    bool returnToClockArmed = false;
    uint8_t holdStage = 0;
    int8_t lastLedState = -1;
    bool touchWakeOnly_ = false;
    uint8_t adjacentPage(uint8_t page, int direction = 1) const;
    void handleTouch();
    void shutdown();
    void tick();
    static void run(void* context);
};
