#pragma once
#include <Arduino.h>
#include <Wire.h>

// TCA9554 register map: output 0x01, configuration 0x03.
// Datasheet: https://www.ti.com/lit/ds/symlink/tca9554.pdf
// The owner must serialize access to the shared I2C bus.
class BoardIoExpander final {
public:
    explicit BoardIoExpander(TwoWire& wire) : wire_(wire) {}

    bool begin(uint8_t address) {
        address_ = address;
        ready_ = readRegister(OutputRegister, output_) &&
                 readRegister(ConfigurationRegister, configuration_);
        return ready_;
    }

    bool readState(uint8_t& output, uint8_t& configuration) {
        return ready_ && readRegister(OutputRegister, output) &&
               readRegister(ConfigurationRegister, configuration);
    }

    bool configureOutputs(uint8_t mask, uint8_t values) {
        if (!writeMasked(mask, values)) return false;
        // Load the output latch before enabling outputs to avoid a reset pulse.
        const uint8_t configuration = configuration_ & uint8_t(~mask);
        if (!writeRegister(ConfigurationRegister, configuration)) {
            ready_ = false; return false;
        }
        configuration_ = configuration;
        return true;
    }

    bool writeMasked(uint8_t mask, uint8_t values) {
        if (!ready_) return false;
        const uint8_t output = (output_ & uint8_t(~mask)) | (values & mask);
        if (!writeRegister(OutputRegister, output)) {
            ready_ = false; return false;
        }
        output_ = output;
        return true;
    }

private:
    static constexpr uint8_t OutputRegister = 0x01;
    static constexpr uint8_t ConfigurationRegister = 0x03;
    TwoWire& wire_;
    uint8_t address_ = 0, output_ = 0xFF, configuration_ = 0xFF;
    bool ready_ = false;

    bool readRegister(uint8_t reg, uint8_t& value) {
        wire_.beginTransmission(address_); wire_.write(reg);
        if (wire_.endTransmission(false) != 0) return false;
        if (wire_.requestFrom(address_, size_t(1)) != 1) {
            while (wire_.available()) wire_.read();
            return false;
        }
        value = wire_.read();
        return true;
    }

    bool writeRegister(uint8_t reg, uint8_t value) {
        wire_.beginTransmission(address_); wire_.write(reg); wire_.write(value);
        return wire_.endTransmission() == 0;
    }
};
