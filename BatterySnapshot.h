#pragma once
#include <Arduino.h>
#include <math.h>

struct BatterySnapshot {
    bool available=false, valid=false, connected=false;
    bool charging=false, discharging=false, usb=false;
    bool percentValid=false, voltageValid=false;
    uint8_t percent=0;
    uint16_t millivolts=0;
    float dischargePercentPerHour=NAN; // Observed SOC percentage points/hour, not current.
    uint32_t observedSeconds=0;
    uint32_t updatedAt=0;
};
