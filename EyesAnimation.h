#pragma once
#include "BoardHardware.h"

class EyesAnimation final {
public:
    explicit EyesAnimation(BoardHardware& board) : board_(board) {}
    bool draw(Arduino_Canvas& canvas, uint8_t rotation);
private:
    BoardHardware& board_;
    enum Expression : uint8_t { HAPPY, CURIOUS, SURPRISED, SLEEPY };
    uint32_t lastFrame = 0, nextLook = 0, nextBlink = 0, blinkAt = 0;
    uint32_t lastAccelAt = 0;
    float lookX = 0, lookY = 0;
    float targetX = 0, targetY = 0;
    float neutralX = 0, neutralY = 0, filteredX = 0, filteredY = 0;
    bool referenceReady = false;
    bool blinking = false;
    uint8_t expression = HAPPY;
    uint32_t nextExpressionAt = 0, lastSurpriseAt = 0;
    float topLids[2] = {}, bottomLids[2] = {};
};
