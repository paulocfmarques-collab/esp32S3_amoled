#pragma once
#include "BoardHardware.h"
#include <time.h>

// PCF85063A at 7-bit address 0x51. The stored calendar is UTC, years 2000–2099.
class BoardRtc final {
public:
    explicit BoardRtc(BoardHardware& board) : board_(board) {}
    bool begin();
    bool readUtc(time_t& epoch);
    bool writeUtc(time_t epoch);
    bool available() const { return present_; }
    const char* error() const { return error_; }
private:
    BoardHardware& board_;
    static constexpr uint8_t Address=0x51;
    bool present_=false;
    const char* error_="RTC ainda nao consultado";
    static int fromBcd(uint8_t value) {
        return (value & 0x0F)>9 || (value>>4)>9?-1:(value>>4)*10+(value & 0x0F);
    }
    static uint8_t toBcd(int value) { return uint8_t((value/10)<<4) | uint8_t(value%10); }
};
