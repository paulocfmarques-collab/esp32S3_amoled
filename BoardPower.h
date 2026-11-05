#pragma once
#include "BoardHardware.h"
#include "BatterySnapshot.h"

// Minimal AXP2101 power-key control. No changes to charger current or rail voltages.
// Register definitions cross-checked against XPowersLib's AXP2101 driver.
class BoardPower final {
public:
    explicit BoardPower(BoardHardware& board) : board_(board) {}
    bool begin();
    bool takeShutdownRequest();
    bool shutdown();
    bool available() const { return ready_; }
    void updateBattery(bool force=false);
    BatterySnapshot batterySnapshot() const { return battery_; }
    String batteryReport();
private:
    BoardHardware& board_;
    static constexpr uint8_t Address = 0x34;
    static constexpr uint8_t KeyShort = 0x08; // INTSTS2/INTEN2, bit 3
    bool ready_ = false, telemetryReady_ = false;
    BatterySnapshot battery_{};
    uint32_t lastBatteryAt_=0;
    int64_t dischargeStartedUs_=0;
    uint8_t dischargeStartedPercent_=0;
    bool dischargeTracking_=false;
    uint32_t startedAt_ = 0, lastPoll_ = 0;
    bool read(uint8_t reg, uint8_t& value) { return board_.readRegisters(Address, reg, &value, 1); }
    bool update(uint8_t reg, uint8_t mask, uint8_t value);
};
