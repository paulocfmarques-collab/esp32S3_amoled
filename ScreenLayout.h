#pragma once
#include <stdint.h>

// Native display geometry, in rotated logical screen coordinates.
class ScreenLayout final {
public:
    static constexpr uint8_t AnalogPage = 10;
    static constexpr uint8_t BatteryPage = 11, SettingsPage = 12, LastPage = SettingsPage;
    static constexpr uint8_t VisiblePages = 11;
    static constexpr int Margin = 12;
    static constexpr int StatusHeight = 32;
    static constexpr int HeaderHeight = 84;
    static constexpr int FooterHeight = 16;
    static int footerTop(int height) { return height - FooterHeight; }
};
