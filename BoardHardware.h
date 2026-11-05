#pragma once
#include "BoardConfig.h"
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include "BoardIoExpander.h"
#include "BoardImu.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Dono do barramento I2C e dos periféricos da placa.
class BoardHardware final {
public:
    class Lock final {
        SemaphoreHandle_t mutex_;
    public:
        explicit Lock(BoardHardware& board) : mutex_(board.i2cMutex_) {
            configASSERT(mutex_); xSemaphoreTakeRecursive(mutex_, portMAX_DELAY);
        }
        ~Lock() { xSemaphoreGiveRecursive(mutex_); }
        Lock(const Lock&) = delete;
        Lock& operator=(const Lock&) = delete;
    };
    bool begin();
    bool i2cReady() const { return i2cReady_; }
    Arduino_GFX& screen() { return screen_; }
    void setBrightness(uint8_t level) { if (displayReady_) screen_.setBrightness(level); }
    bool displayReady() const { return displayReady_; }
    bool readAcceleration(float* x, float* y, float* z);
    bool readRegisters(uint8_t address, uint8_t reg, uint8_t* bytes, size_t count);
    bool writeRegister(uint8_t address, uint8_t reg, uint8_t value);
    bool writeRegisters(uint8_t address,uint8_t reg,const uint8_t* data,size_t count);
private:
    SemaphoreHandle_t i2cMutex_ = nullptr;
    BoardIoExpander expander_{Wire};
    BoardImu imu_;
    bool displayReady_ = false, imuReady_ = false, i2cReady_ = false;
    Arduino_ESP32QSPI bus_{BoardConfig::LcdCs, BoardConfig::LcdClock,
        BoardConfig::LcdD0, BoardConfig::LcdD1, BoardConfig::LcdD2, BoardConfig::LcdD3};
#if AMOLED_BOARD_REVISION == 1
    Arduino_SH8601 screen_{&bus_, GFX_NOT_DEFINED, 0, BoardConfig::Width, BoardConfig::Height};
#else
    Arduino_CO5300 screen_{&bus_, GFX_NOT_DEFINED, 0, BoardConfig::Width, BoardConfig::Height, 16, 0, 0, 0};
#endif
};
