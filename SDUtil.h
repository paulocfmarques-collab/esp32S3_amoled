#ifndef SDUTIL_H
#define SDUTIL_H

#include <Arduino.h>
#include <SD_MMC.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <atomic>

struct SdMetricsSnapshot
{
    bool mounted = false, valid = false;
    uint64_t capacity = 0, total = 0, used = 0;
    bool logValid[3]{};
    uint64_t logBytes[3]{};
    int64_t updatedUs = 0;
    uint32_t appends = 0, appendErrors = 0;
};

class SDUtil
{
public:
    SDUtil() : mutex_(xSemaphoreCreateRecursiveMutex()) {}
    SDUtil(const SDUtil&) = delete;
    SDUtil& operator=(const SDUtil&) = delete;
    bool begin();
    bool salvarCrash(const String& relatorio, String& caminho, String& erro);
    bool test();
    String getCardType();
    uint64_t getCardSizeBytes();
    uint64_t getUsedBytes(bool* valid = nullptr);
    uint64_t getUsedBytesCooperative(bool* valid = nullptr);
    void processMetrics();
    SdMetricsSnapshot metricsSnapshot();
    inline static constexpr const char* METRIC_LOG_PATHS[] =
        {"/log.txt", "/network_history.log", "/network_history.previous.log"};

    bool exists(String path);
    bool writeText(String path, String text);
    bool appendText(String path, String text);
    String readText(String path);
    bool removeFile(String path);
    bool createDir(String path);
    String listFiles(String path = "/");
    bool writeJson(String path, String jsonContent);
    String readJson(String path);
    uint64_t getFileSize(String path);
    String readTail(String path, size_t maxBytes);
    // Retorna um status HTTP e limita a leitura para o portal de logs.
    int readLogTail(const char* path, String& content, bool& truncated);
    bool removeDir(String path);
    bool renameFile(const String& from,const String& to);

private:
    SemaphoreHandle_t mutex_;
    bool mounted_ = false;
    SdMetricsSnapshot metrics_{};
    uint32_t metricsAt_ = 0;
    class Lock final {
        SemaphoreHandle_t mutex_;
    public:
        explicit Lock(SDUtil& sd) : mutex_(sd.mutex_) {
            configASSERT(mutex_); xSemaphoreTakeRecursive(mutex_, portMAX_DELAY);
        }
        ~Lock() { xSemaphoreGiveRecursive(mutex_); }
        Lock(const Lock&) = delete;
        Lock& operator=(const Lock&) = delete;
    };
    static bool validPath(const String& path);
    bool write(const String& path, const String& text, bool append);
};
#endif
