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
    struct Event { Gesture gesture; int16_t x,y; };
    bool poll(Event& event) { return queue_ && xQueueReceive(queue_, &event, 0) == pdTRUE; }
    void setRotation(uint8_t rotation) { rotation_.store(rotation & 3); }
    bool available() const { return ready_; }
    bool contactActive() const {return contact_.load();}
    void pause(bool paused) { paused_.store(paused); }
private:
    BoardHardware& board_;
    QueueHandle_t queue_ = nullptr;
    TaskHandle_t task_ = nullptr;
    bool ready_ = false;
    std::atomic<uint8_t> rotation_{0};
    std::atomic<bool> paused_{false},contact_{false};
    static void run(void* context);
    void sample();
    void send(Gesture gesture) { const Event event{gesture,int16_t(lastX_),int16_t(lastY_)};xQueueSend(queue_, &event, 0); }
    bool pressed_ = false;
    int startX_ = 0, startY_ = 0, lastX_ = 0, lastY_ = 0;
    uint32_t pressedAt_ = 0, lastContactAt_ = 0;
};
