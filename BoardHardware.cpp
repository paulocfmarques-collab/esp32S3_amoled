#include "BoardHardware.h"

bool BoardHardware::begin() {
    if (i2cMutex_) return displayReady_;
    i2cMutex_ = xSemaphoreCreateRecursiveMutex();
    if (!i2cMutex_) { Serial.println("[BOARD] Falha ao criar mutex I2C"); return false; }
    Serial.printf("[BOARD] Config V%d; PSRAM total=%u livre=%u bytes\n",
        AMOLED_BOARD_REVISION, unsigned(ESP.getPsramSize()), unsigned(ESP.getFreePsram()));
    if (!Wire.begin(BoardConfig::Sda, BoardConfig::Scl)) {
        Serial.println("[BOARD] Falha ao iniciar I2C SDA15/SCL14"); return false;
    }
    Wire.setClock(400000); Wire.setTimeOut(30);
    Lock lock(*this);
    if (!expander_.begin(BoardConfig::ExpanderAddress)) {
        Serial.println("[BOARD] XCA9554 indisponivel; confira alimentacao");
        return false;
    }
    // Official board sequence, with checked writes and unrelated pins preserved.
    if (!expander_.configureOutputs(0x07, 0x00)) {
        Serial.println("[BOARD] Falha ao configurar reset/power");
        return false;
    }
    delay(20);
    if (!expander_.writeMasked(0x07, 0x07) ||
        !expander_.configureOutputs(0x80, 0x80)) { // SD DAT3/CS held high
        Serial.println("[BOARD] Falha ao liberar reset/configurar SD");
        return false;
    }
    uint8_t output = 0, configuration = 0;
    if (!expander_.readState(output, configuration)) {
        Serial.println("[BOARD] Falha ao reler registros do expansor"); return false;
    }
    Serial.printf("[BOARD] Expander OUT=0x%02X CFG=0x%02X\n", output, configuration);
    if ((output & 0x87) != 0x87 || (configuration & 0x87) != 0) {
        Serial.println("[BOARD] Reset/power/SD nao confirmados no expansor"); return false;
    }
    delay(200);
    auto probe = [](uint8_t address) {
        Wire.beginTransmission(address); return Wire.endTransmission() == 0;
    };
    const bool touchV1 = probe(0x38), touchV2 = probe(0x15);
    Serial.printf("[BOARD] Touch ACK: V1/0x38=%s V2/0x15=%s\n",
        touchV1 ? "SIM" : "NAO", touchV2 ? "SIM" : "NAO");
    if ((AMOLED_BOARD_REVISION == 1 && !touchV1 && touchV2) ||
        (AMOLED_BOARD_REVISION == 2 && !touchV2 && touchV1)) {
        Serial.println("[BOARD] AVISO: touch responde no endereco da outra revisao; confira V1/V2");
    }
    Serial.println("[LCD] Iniciando driver QSPI...");
    displayReady_ = screen_.begin();
    // begin() checks driver/bus initialization; it does not verify visible pixels.
    Serial.printf("[LCD] Driver begin=%s (nao confirma imagem no painel)\n", displayReady_ ? "OK" : "FALHA");
    if (displayReady_) {
        screen_.setBrightness(255);
        if (BoardConfig::DisplayStartupTest) {
            Serial.println("[LCD] Teste direto SEM PSRAM: vermelho, verde, azul");
            const uint16_t colors[] = {0xF800, 0x07E0, 0x001F};
            for (uint16_t color : colors) { screen_.fillScreen(color); delay(300); }
        }
        screen_.fillScreen(0);
        screen_.setTextColor(0xFFFF, 0); screen_.setTextSize(2);
        screen_.setCursor(24, 80); screen_.println("AMOLED boot");
        screen_.setCursor(24, 110); screen_.println("Preparing framebuffer...");
    }
    Serial.println("[BOARD] Inicializando QMI8658...");
    imuReady_ = imu_.begin(Wire, BoardConfig::Sda, BoardConfig::Scl);
    Serial.printf("[BOARD] V%d LCD=%s QMI8658=%s\n", AMOLED_BOARD_REVISION,
        displayReady_ ? "OK" : "falha", imuReady_ ? "OK" : "indisponivel");
    return displayReady_;
}
bool BoardHardware::readAcceleration(float* x, float* y, float* z) {
    Lock lock(*this);
    return imuReady_ && imu_.readAcceleration(*x, *y, *z);
}
bool BoardHardware::readRegisters(uint8_t address, uint8_t reg, uint8_t* bytes, size_t count) {
    Lock lock(*this);
    Wire.beginTransmission(address); Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(address, count) != count) {
        while (Wire.available()) Wire.read();
        return false;
    }
    for (size_t i = 0; i < count; ++i) bytes[i] = Wire.read();
    return true;
}
bool BoardHardware::writeRegister(uint8_t address, uint8_t reg, uint8_t value) {
    Lock lock(*this);
    Wire.beginTransmission(address); Wire.write(reg); Wire.write(value);
    return Wire.endTransmission() == 0;
}
