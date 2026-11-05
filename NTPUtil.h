#ifndef NTP_UTIL_H
#define NTP_UTIL_H
#include <Arduino.h>
#include <atomic>
#include <time.h>
#include <sys/time.h>
#include "BoardRtc.h"

// One time authority for UI and commands. RTC storage is UTC; timezone affects display only.
class NTPUtil final {
public:
    explicit NTPUtil(BoardRtc& rtc) : rtc_(rtc) {}
    bool initNTP();
    void update();
    void getDateTime(String& dateTime,uint32_t timeoutMs=0);
    String getDateTime(uint32_t timeoutMs=0);
    bool isSincronizado() const; // True only after an actual recent SNTP response on this connection.
    String getSomenteHora();
    bool ajustarDataHora(int ano,int mes,int dia,int hora,int minuto,int segundo);
    int32_t getFusoHora() const { return fusoHora; }
    bool isDstAtivo() const { return dstAtivo; }
    void atualizarConfiguracao(int32_t novoFuso,bool novoDst);
    void carregarConfiguracoes();
    const char* sourceLabel() const;
    String statusReport();
private:
    enum class Source : uint8_t { None,Ntp,Rtc,Manual,Internal };
    BoardRtc& rtc_;
    int32_t fusoHora=-3;
    bool dstAtivo=false,initialized_=false,lastOnline_=false,rtcValid_=false;
    bool systemValid_=false,manual_=false,ntpSeen_=false;
    Source source_=Source::None;
    time_t rtcEpoch_=0;
    int64_t rtcReadAtUs_=0,lastNtpAtUs_=0,connectionStartedUs_=0;
    uint32_t lastRtcPollMs_=0,lastRtcCheckMs_=0,rtcWrites_=0;
    bool driftKnown_=false;
    int64_t lastDriftSeconds_=0;
    static constexpr uint32_t NtpIntervalMs=300000;
    static constexpr int64_t NtpFreshUs=600LL*1000000;
    // The C callback has no context argument. This bridge only publishes a timestamp.
    inline static std::atomic<NTPUtil*> callbackOwner_{nullptr};
    portMUX_TYPE callbackMux_=portMUX_INITIALIZER_UNLOCKED;
    bool pendingSync_=false;
    int64_t notifiedUnixUs_=0,notifiedAtUs_=0;
    static void onNtpSync(struct timeval* value);
    static const char* sourceName(Source source);
    void aplicarConfiguracaoNTP();
    bool refreshRtc();
    void reconcileRtc(time_t authority);
    bool currentUtc(time_t& epoch);
    static bool setSystemUs(int64_t epochUs);
};
#endif
