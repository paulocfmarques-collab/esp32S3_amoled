#ifndef DISPLAY_UTIL_H
#define DISPLAY_UTIL_H

#include <Arduino.h>
#include "BoardHardware.h"
#include "TouchInput.h"
#include "EyesAnimation.h"

#include <vector>
#include "ClimaManager.h"
#include <esp_heap_caps.h>

class DisplayBuffer final : public Arduino_Canvas {
public:
    DisplayBuffer(Arduino_GFX& output) : Arduino_Canvas(BoardConfig::Width, BoardConfig::Height, &output) {}
    bool begin(int32_t = GFX_SKIP_OUTPUT_BEGIN) override {
        if (!_framebuffer) _framebuffer = static_cast<uint16_t*>(heap_caps_aligned_alloc(16,
            BoardConfig::Width * BoardConfig::Height * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        return _framebuffer && Arduino_Canvas::begin(GFX_SKIP_OUTPUT_BEGIN);
    }
};
#include "NetworkMonitor.h"

class DisplayUtil {
private:
    BoardHardware& board_;
    TouchInput& touch_;
    DisplayBuffer frame_;
    ClimaManager& climate_;
    uint8_t rotation_ = 0;
    uint16_t statusColor_ = 0x07FF;
    EyesAnimation eyes_{board_};
    void footer();
    void centered(const String& value, int y, uint8_t size, uint16_t color);
    bool ready_ = false;
    uint16_t appliedBrightness_ = 256;
    void page(const String& title, const String& body);
    const NetworkMonitor* monitor_=nullptr;
    String connecting_;
    uint8_t brightness_=80;
    uint32_t lastActivity_=0;
    int requestedPage_=-1;

    std::vector<String> lines;
    uint16_t textColor;
    uint16_t backgroundColor;
    uint8_t textSize;
    
    uint32_t fimAnimacaoSD = 0;

public:
    DisplayUtil(BoardHardware& board, TouchInput& touch, ClimaManager& climate);
    bool ready() const { return ready_; }
    void setStatusColor(uint16_t color) { statusColor_ = color; }
    void begin();
    void setMonitor(const NetworkMonitor* m){monitor_=m;}
    void setConnecting(const String& name){connecting_=name;}
    void setBacklight(uint8_t percent);
    void wake();
    void updatePower();
    bool idleFor(uint32_t duration) const { return millis() - lastActivity_ >= duration; }
    uint8_t brightness()const{return brightness_;}
    void selectPage(uint8_t page){requestedPage_=page;wake();}
    void nextPage(){requestedPage_=-2;wake();}
    int takePageRequest(){int r=requestedPage_;requestedPage_=-1;return r;}
    void clearConsole(){clear();}
    void showMonitorPage();
    void showForecastPage();
    void showOtaProgress(uint8_t percent, const String& status);
    void clear();
    void setTextColor(uint16_t color);
    void setBackgroundColor(uint16_t color);
    void setTextSize(uint8_t size);
    void setRotation(uint8_t rotation);
    
    void showClock(String dateTime);
    void showStatusPage(bool sdReady, bool ntpSynchronized, const String& ntpDateTime,
                        int32_t timezoneOffset, bool daylightSaving, uint8_t part = 0);
    void showNetworkPage(uint8_t part = 0);
    void showSdLoading(uint8_t frame);
    void showSystemPage();
    void showSavedWifiPage(const String ssids[5], int connectedSlot, uint8_t nextSlot);
    void showHoldMessage(const String& line1, const String& line2, uint16_t color);
    void showSdPage(bool sdReady, const String& cardType, uint64_t totalBytes,
                    uint64_t usedBytes);
    void desenharOlhosScreensaver();
    void dispararAnimacaoGravacaoSD(); 
    
    void print(String text);
    void println(String text);
    void addLine(String line);
    void redraw();
    

};

#endif
