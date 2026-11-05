#pragma once
#include <Arduino_GFX_Library.h>
#include "BatterySnapshot.h"

class UiPainter final {
public:
    enum class Font : uint8_t { Small, Body, Title, Large, Hero };
    enum class Align : uint8_t { Left, Center, Right };
    enum class Icon : uint8_t { Clock, Wifi, Chip, Storage, Chart, Cloud, Sun, Console, Power, Sync, Battery };
    static constexpr uint16_t Ink=0xEF7F,Muted=0x8CF3,Cyan=0x07FF,Green=0x4F70,Amber=0xFDC0,Red=0xF986;
    static constexpr uint16_t Card=0x0823,Edge=0x1927,Track=0x18E5;
    explicit UiPainter(Arduino_Canvas& canvas) : canvas_(canvas) {}
    void text(String value,int x,int top,Font font,uint16_t color,int maxWidth=0,Align align=Align::Left);
    int textWidth(const String& value,Font font);
    void card(int x,int y,int w,int h,uint16_t accent=Cyan);
    void icon(Icon type,int x,int y,int size,uint16_t color);
    void meter(int x,int y,int width,int height,float percent,uint16_t color);
    void statusBar(uint8_t page,uint8_t pages,const BatterySnapshot& battery,bool pinned);
private:
    Arduino_Canvas& canvas_;
    void select(Font font);
    void resetFont();
    static String ascii(String text);
};
