#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <ImuDrv.hpp>

// SensorLib 0.5.x API. Do not include the deprecated root SensorQMI8658.hpp:
// it declares a different class layout with the same name as the compiled driver.
class BoardImu final {
public:
    bool begin(TwoWire& wire, int sda, int scl) {
        Serial.println("[IMU] QMI8658: SensorLib ImuDrv API");
        ready_ = sensor_.begin(wire, QMI8658_L_SLAVE_ADDRESS, sda, scl);
        if (!ready_) return false;
        ready_ = sensor_.configAccel(SensorQMI8658::AccelRange::FS_4G,
            896.8f, SensorQMI8658::LpfMode::MODE_0) && sensor_.enableAccel();
        return ready_;
    }

    bool readAcceleration(float& x, float& y, float& z) {
        if (!ready_ || !sensor_.isDataReady(static_cast<uint8_t>(ImuBase::DataReadyMask::ACCEL)))
            return false;
        AccelerometerData sample{};
        if (!sensor_.readAccel(sample)) return false;
        // Modern SensorLib reports m/s². The existing animation expects g.
        constexpr float gravity = 9.80665f;
        x = sample.mps2.x / gravity;
        y = sample.mps2.y / gravity;
        z = sample.mps2.z / gravity;
        return true;
    }

private:
    SensorQMI8658 sensor_;
    bool ready_ = false;
};
