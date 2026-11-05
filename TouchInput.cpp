#include "TouchInput.h"
#include "ScreenLayout.h"

bool TouchInput::begin() {
    if (task_) return true;
    queue_ = xQueueCreate(12, sizeof(Event));
    if (!queue_) return false;
    pinMode(BoardConfig::TouchInterrupt, INPUT_PULLUP);
    // Mesmos registros dos drivers Arduino_FT3x68 / Arduino_CST816x da Waveshare.
#if AMOLED_BOARD_REVISION == 1
    ready_ = board_.writeRegister(BoardConfig::TouchAddress, 0xA5, 0x00); // active
#else
    ready_ = board_.writeRegister(BoardConfig::TouchAddress, 0xFA, 0x40); // periodic
#endif
    if (!ready_) { vQueueDelete(queue_); queue_ = nullptr; return false; }
    if (xTaskCreatePinnedToCore(run, "touch", 3072, this, 1, &task_, BoardConfig::InputCore) != pdPASS) {
        vQueueDelete(queue_); queue_ = nullptr; ready_ = false; return false;
    }
    return true;
}
void TouchInput::run(void* context) {
    auto& self = *static_cast<TouchInput*>(context);
    Serial.printf("[CPU] Touch no core %d\n", xPortGetCoreID());
    for (;;) { self.sample(); vTaskDelay(pdMS_TO_TICKS(20)); }
}
void TouchInput::sample() {
    if (paused_.load()) { pressed_ = false;contact_.store(false); return; }
    uint8_t data[5];
    const uint32_t now = millis();
    const bool valid = board_.readRegisters(BoardConfig::TouchAddress, 0x02, data, sizeof(data));
    // Não transforma uma falha de I2C em toque/gesto fantasma.
    if (!valid) {
        if (pressed_ && now - lastContactAt_ > 300) {pressed_ = false;contact_.store(false);}
        return;
    }
    if ((data[0] & 0x0F) > 0) {
        int x = ((data[1] & 0x0F) << 8) | data[2];
        int y = ((data[3] & 0x0F) << 8) | data[4];
        if (x >= BoardConfig::Width || y >= BoardConfig::Height) return;
        int rx = x, ry = y;
        switch (rotation_.load()) {
        case 1: rx = y; ry = BoardConfig::Width - 1 - x; break;
        case 2: rx = BoardConfig::Width - 1 - x; ry = BoardConfig::Height - 1 - y; break;
        case 3: rx = BoardConfig::Height - 1 - y; ry = x; break;
        }
        lastX_ = rx; lastY_ = ry; lastContactAt_ = now;
        if (!pressed_) {
            pressed_ = true;contact_.store(true); startX_ = rx; startY_ = ry; pressedAt_ = now; send(Gesture::Down);
        }
    } else if (pressed_) {
        pressed_ = false;
        const int dx = lastX_ - startX_, dy = lastY_ - startY_;
        if (abs(dy) >= 50 && abs(dy) > abs(dx)) send(dy < 0 ? Gesture::Next : Gesture::Previous);
        else if (abs(dx) >= 50 && abs(dx) > abs(dy)) send(dx < 0 ? Gesture::Next : Gesture::Previous);
        else if (abs(dx) < 30 && abs(dy) < 30 && now - pressedAt_ >= 800) send(Gesture::Home);
        else if (abs(dx) < 30 && abs(dy) < 30 && now - pressedAt_ >= 30) {
            send(Gesture::Tap); // Coordinates target explicit settings cards / monitor chart.
        }
        contact_.store(false); // Publish release event before allowing canvas rotation.
    }
}
