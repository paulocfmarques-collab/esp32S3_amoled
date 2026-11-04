#pragma once
#include "BoardHardware.h"
#include <freertos/queue.h>
#include <freertos/task.h>
#include <atomic>

// Só envia eventos por fila. Tela, rede e SD pertencem à tarefa da aplicação.
class TouchInput final {
public:
    enum class Gesture : uint8_t { Down, Tap, Next, Previous, Home };
    explicit TouchInput(BoardHardware& board) : board_(board) {}
    bool begin();
    bool poll(Gesture& gesture) { return queue_ && xQueueReceive(queue_, &gesture, 0) == pdTRUE; }
    void setRotation(uint8_t rotation) { rotation_.store(rotation & 3); }
    bool available() const { return ready_; }
private:
    BoardHardware& board_;
    QueueHandle_t queue_ = nullptr;
    TaskHandle_t task_ = nullptr;
    bool ready_ = false;
    std::atomic<uint8_t> rotation_{0};
    static void run(void* context);
    void sample();
    void send(Gesture gesture) { xQueueSend(queue_, &gesture, 0); }
    bool pressed_ = false;
    int startX_ = 0, startY_ = 0, lastX_ = 0, lastY_ = 0;
    uint32_t pressedAt_ = 0, lastContactAt_ = 0;
};
