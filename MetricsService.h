#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <Preferences.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class SDUtil;
class ClimaManager;

class MetricsService
{
public:
    explicit MetricsService(ClimaManager& climate) : climate_(climate) {}
    struct Settings
    {
        uint32_t version;
        char host[254];
        uint16_t port, interval;
    };

    void begin(WebServer& panel, SDUtil& storage);
    void process();
    // Updated by the gateway on the same application task before serving scrapes.
    void setUdpStatus(bool listenerUp, uint64_t received, uint64_t replies, uint64_t sendErrors)
    {
        udp = {listenerUp, received, replies, sendErrors};
    }
    bool resetCounters();
    static bool validHost(const String& host);
    static bool parseNumber(const String& text, uint16_t minimum, uint16_t maximum, uint16_t& value);

private:
    ClimaManager& climate_;
    struct UdpStatus {
        bool listenerUp = false;
        uint64_t received = 0, replies = 0, sendErrors = 0;
    } udp;
    Settings settings{};
    struct BootRecord { uint32_t version, boots, watchdogs; } boots{};
    WebServer* panel = nullptr;
    SDUtil* storage = nullptr;
    bool initialized = false;
    WebServer exporter{9120};
    String device;
    bool countersOk = false, pending = false;
    uint16_t activePort = 9120;
    uint32_t savedAt = 0, imageBytes = 0, partitionBytes = 0;
    std::atomic<uint32_t> disconnects{0};
    uint64_t requests = 0, errors = 0;
    double responseMs = 0;
    float cpuUsage = NAN;
#if configUSE_TRACE_FACILITY && configGENERATE_RUN_TIME_STATS
    TaskStatus_t tasks[64];
    configRUN_TIME_COUNTER_TYPE previousIdle[portNUM_PROCESSORS]{};
    int64_t previousTime = 0;
    uint32_t sampledAt = 0;
    bool baseline = false;
#endif

    void load();
    bool save(const String& host, uint16_t port, uint16_t interval);
    void start();
    void show(int status = 200, const String& notice = "");
    void handleSave();
    void sampleCpu();
    void sendMetrics(WebServer& web);
    String exposition() const;
    String yaml() const;
    static String escapeLabel(String value);
    static String escapeHtml(String value);
};
