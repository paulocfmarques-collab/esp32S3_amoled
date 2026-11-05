#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "ping/ping_sock.h"

// Callbacks only publish results; display and statistics stay on the Arduino loop.
class NetworkMonitor
{
  public:
    bool connected = false, initialized = false, hasResult = false, replied = false;
    uint32_t drops = 0, attempts = 0, failures = 0, rtt = 0, lastDown = 0, measuredAt = 0,
             received = 0, minimum = 0, maximum = 0;
    uint64_t totalRtt = 0;
    uint64_t offlineMs = 0;
    String events[3];
    int history[24];
    uint8_t head = 0;
    struct Trend {int16_t rssi=-127;int32_t latency=-2;};
    static constexpr int TrendCapacity=120;
    int trendCount=0,trendHead=0;
    Trend trends[TrendCapacity]{};
    const Trend& trendAt(int i) const {return trends[(trendHead+TrendCapacity-trendCount+i)%TrendCapacity];}
    void begin();
    void update();

    uint64_t offlineSeconds() const
    {
        return (offlineMs + (!connected ? uint32_t(millis() - offlineAt) : 0)) / 1000;
    }

  private:
    uint32_t trendAt_=0;bool trendStarted_=false;
    uint32_t offlineAt = 0, tickAt = 0, nextAt = 0, epoch = 0, sessionEpoch = 0;
    esp_ping_handle_t session = nullptr;
    IPAddress target;
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    bool done = false, success = false;
    uint32_t elapsed = 0;
    static void onSuccess(esp_ping_handle_t, void*);
    static void onEnd(esp_ping_handle_t, void*);
    void event(const String& text);
};
