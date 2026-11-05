#pragma once
#include <Arduino.h>

// 1 = SH8601 + FT3168; 2 = CO5300 + CST820. Confira a etiqueta da placa.
#ifndef AMOLED_BOARD_REVISION
#define AMOLED_BOARD_REVISION 2
#endif
#if AMOLED_BOARD_REVISION != 1 && AMOLED_BOARD_REVISION != 2
#error "AMOLED_BOARD_REVISION deve ser 1 ou 2"
#endif

class BoardConfig final {
public:
    // Temporary startup diagnostic: draws directly, without the PSRAM canvas.
    static constexpr bool DisplayStartupTest = false;
    static constexpr int Width = 368, Height = 448;
    static constexpr int LcdCs = 12, LcdClock = 11;
    static constexpr int LcdD0 = 4, LcdD1 = 5, LcdD2 = 6, LcdD3 = 7;
    static constexpr int Sda = 15, Scl = 14, TouchInterrupt = 21;
    static constexpr int SdClock = 2, SdCommand = 1, SdData = 3;
    static constexpr int Button = 0; // BOOT. PWR permanece sob controle do AXP2101.
    static constexpr int ApplicationCore = 1, InputCore = 0;
    static constexpr uint8_t ExpanderAddress = 0x20;
    static constexpr uint8_t TouchAddress = AMOLED_BOARD_REVISION == 1 ? 0x38 : 0x15;
    static constexpr const char* Hostname = "AMOLED-Gateway";
    static constexpr const char* AccessPoint = "AMOLED_CONFIG";
};
