#pragma once
#include <Arduino_GFX_Library.h>
#include "BatterySnapshot.h"

// Native vector dial: no bitmap asset and no extra graphics dependency.
class WatchFace final {
public:
    void draw(Arduino_Canvas& canvas, const String& timestamp, const BatterySnapshot& battery,
              bool online, bool weatherValid, float temperature, int32_t timezoneHours, bool daylightSaving,const String& timeSource,uint8_t style=0,uint16_t accent=0x07FF);
private:
    static void centered(Arduino_Canvas& canvas,const String& text,int x,int y,uint8_t size,uint16_t color);
    static void hand(Arduino_Canvas& canvas,int cx,int cy,float radians,float length,float width,uint16_t color);
};
