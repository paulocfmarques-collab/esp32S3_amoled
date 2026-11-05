#include "NTPUtil.h"
#include "ClockCalendar.h"
#include <Preferences.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <esp_timer.h>

const char* NTPUtil::sourceName(Source source) {
    switch(source) {
    case Source::Ntp:return "NTP";
    case Source::Rtc:return "RTC";
    case Source::Manual:return "MANUAL";
    case Source::Internal:return "INTERNO";
    default:return "SEM HORA";
    }
}
void NTPUtil::onNtpSync(struct timeval* value) {
    NTPUtil* owner=callbackOwner_.load(std::memory_order_acquire);
    if(!owner || !value || !ClockCalendar::validNetworkEpoch(value->tv_sec))return;
    const int64_t monotonic=esp_timer_get_time();
    portENTER_CRITICAL(&owner->callbackMux_);
    owner->notifiedUnixUs_=int64_t(value->tv_sec)*1000000+value->tv_usec;
    owner->notifiedAtUs_=monotonic;owner->pendingSync_=true;
    portEXIT_CRITICAL(&owner->callbackMux_);
    // No I2C, display, Preferences or heap work on the SNTP callback task.
}
bool NTPUtil::setSystemUs(int64_t epochUs) {
    struct timeval value{};value.tv_sec=static_cast<time_t>(epochUs/1000000);
    value.tv_usec=static_cast<suseconds_t>(epochUs%1000000);
    return settimeofday(&value,nullptr)==0;
}
void NTPUtil::carregarConfiguracoes() {
    Preferences prefs;
    if(prefs.begin("ntp_cfg",true)) {
        fusoHora=prefs.getInt("fuso",-3);dstAtivo=prefs.getBool("dst",false);prefs.end();
    }
    if(fusoHora<-12 || fusoHora>14)fusoHora=-3;
}
void NTPUtil::aplicarConfiguracaoNTP() {
    configTime(fusoHora*3600,dstAtivo?3600:0,"a.st1.ntp.br","pool.ntp.org","time.nist.gov");
    sntp_set_sync_interval(NtpIntervalMs);
    Serial.printf("[NTP] Consulta assincrona; UTC%+d DST=%s\n",int(fusoHora),dstAtivo?"ON":"OFF");
}
bool NTPUtil::refreshRtc() {
    lastRtcPollMs_=millis();
    rtcValid_=rtc_.readUtc(rtcEpoch_);
    if(rtcValid_)rtcReadAtUs_=esp_timer_get_time();
    return rtcValid_;
}
bool NTPUtil::initNTP() {
    if(initialized_)return isSincronizado();
    carregarConfiguracoes();rtc_.begin();
    if(refreshRtc()) {
        systemValid_=setSystemUs(int64_t(rtcEpoch_)*1000000);
        source_=Source::Rtc;
        Serial.println("[RTC] Hora UTC restaurada; aguardando resposta NTP");
    } else Serial.printf("[RTC] Hora indisponivel: %s\n",rtc_.error());
    lastOnline_=WiFi.status()==WL_CONNECTED;connectionStartedUs_=esp_timer_get_time();
    callbackOwner_.store(this,std::memory_order_release);
    sntp_set_time_sync_notification_cb(onNtpSync);
    initialized_=true;aplicarConfiguracaoNTP();update();
    return isSincronizado();
}
void NTPUtil::reconcileRtc(time_t authority) {
    lastRtcCheckMs_=millis();
    if(rtcValid_) {
        const time_t current=rtcEpoch_+static_cast<time_t>((esp_timer_get_time()-rtcReadAtUs_)/1000000);
        lastDriftSeconds_=static_cast<int64_t>(difftime(authority,current));driftKnown_=true;
        // RTC precision is one second. Ignore a one-second read/tick boundary.
        if(lastDriftSeconds_>=-1 && lastDriftSeconds_<=1)return;
    } else driftKnown_=false;
    if(rtc_.writeUtc(authority)) {
        ++rtcWrites_;refreshRtc();
        Serial.printf("[RTC] Atualizado pela hora %s; gravacoes=%u\n",manual_?"manual":"NTP",unsigned(rtcWrites_));
    } else {
        rtcValid_=false;
        Serial.printf("[RTC] Falha ao sincronizar: %s\n",rtc_.error());
    }
}
void NTPUtil::update() {
    if(!initialized_)return;
    const bool online=WiFi.status()==WL_CONNECTED;
    if(online!=lastOnline_) {
        lastOnline_=online;ntpSeen_=false;
        if(online) {connectionStartedUs_=esp_timer_get_time();aplicarConfiguracaoNTP();}
    }
    bool event=false;int64_t unixUs=0,eventAt=0;
    portENTER_CRITICAL(&callbackMux_);
    if(pendingSync_) {event=true;unixUs=notifiedUnixUs_;eventAt=notifiedAtUs_;pendingSync_=false;}
    portEXIT_CRITICAL(&callbackMux_);
    if(event) {
        systemValid_=setSystemUs(unixUs+esp_timer_get_time()-eventAt);
        ntpSeen_=systemValid_;lastNtpAtUs_=eventAt;manual_=false;
        Serial.println("[NTP] Resposta recebida e hora confirmada");
    }
    if(event || !lastRtcPollMs_ || millis()-lastRtcPollMs_>=1000)refreshRtc();
    const int64_t now=esp_timer_get_time();
    const bool fresh=online && ntpSeen_ && lastNtpAtUs_>=connectionStartedUs_ && now-lastNtpAtUs_<=NtpFreshUs;
    if((event && systemValid_) || ((fresh || manual_) && (!lastRtcCheckMs_ || millis()-lastRtcCheckMs_>=60000)))
        reconcileRtc(time(nullptr));
    const Source next=fresh?Source::Ntp:manual_ && systemValid_?Source::Manual:
        rtcValid_?Source::Rtc:systemValid_?Source::Internal:Source::None;
    if(next!=source_) {
        if(next==Source::Rtc) {
            const time_t utc=rtcEpoch_+static_cast<time_t>((esp_timer_get_time()-rtcReadAtUs_)/1000000);
            systemValid_=setSystemUs(int64_t(utc)*1000000);
        }
        source_=next;Serial.printf("[TIME] Fonte: %s\n",sourceName(source_));
    }
}
bool NTPUtil::currentUtc(time_t& epoch) {
    update();
    if(source_==Source::Rtc) {
        epoch=rtcEpoch_+static_cast<time_t>((esp_timer_get_time()-rtcReadAtUs_)/1000000);return true;
    }
    if(source_==Source::Ntp || source_==Source::Manual || source_==Source::Internal) {
        epoch=time(nullptr);return systemValid_;
    }
    return false;
}
void NTPUtil::getDateTime(String& dateTime,uint32_t timeoutMs) {
    (void)timeoutMs;time_t utc=0;
    if(!currentUtc(utc)){dateTime="Hora indisponivel";return;}
    // Explicit display conversion keeps the RTC in UTC and avoids applying DST twice.
    const time_t local=utc+fusoHora*3600+(dstAtivo?3600:0);struct tm info{};
    if(!gmtime_r(&local,&info)){dateTime="Hora indisponivel";return;}
    char value[24];strftime(value,sizeof(value),"%d/%m/%Y %H:%M:%S",&info);dateTime=value;
}
String NTPUtil::getDateTime(uint32_t timeoutMs) {String value;getDateTime(value,timeoutMs);return value;}
bool NTPUtil::isSincronizado() const {return source_==Source::Ntp;}
const char* NTPUtil::sourceLabel() const {return sourceName(source_);}
String NTPUtil::getSomenteHora() {
    const String value=getDateTime();return value.length()==19?value.substring(11):String("--:--:--");
}
void NTPUtil::atualizarConfiguracao(int32_t novoFuso,bool novoDst) {
    if(novoFuso<-12 || novoFuso>14)return;
    fusoHora=novoFuso;dstAtivo=novoDst;
    Preferences prefs;
    if(prefs.begin("ntp_cfg",false)) {prefs.putInt("fuso",fusoHora);prefs.putBool("dst",dstAtivo);prefs.end();}
    aplicarConfiguracaoNTP(); // Does not shift the RTC calendar.
}
bool NTPUtil::ajustarDataHora(int ano,int mes,int dia,int hora,int minuto,int segundo) {
    time_t local=0;
    if(!ClockCalendar::toUtc(ano,mes,dia,hora,minuto,segundo,local))return false;
    const time_t utc=local-fusoHora*3600-(dstAtivo?3600:0);
    if(!setSystemUs(int64_t(utc)*1000000))return false;
    systemValid_=true;manual_=true;ntpSeen_=false;
    portENTER_CRITICAL(&callbackMux_);pendingSync_=false;portEXIT_CRITICAL(&callbackMux_);
    rtcValid_=rtc_.writeUtc(utc);
    if(rtcValid_) {++rtcWrites_;refreshRtc();}
    else Serial.printf("[RTC] Hora manual nao persistida: %s\n",rtc_.error());
    source_=Source::Manual;lastRtcCheckMs_=millis();
    aplicarConfiguracaoNTP();return true;
}
String NTPUtil::statusReport() {
    const String displayed=getDateTime();
    String report="===== HORA / NTP / RTC =====\nFonte: "+String(sourceName(source_))+"\n";
    report+="Data/Hora: "+displayed+"\n";
    report+="NTP: "+String(source_==Source::Ntp?"resposta recente confirmada":"sem resposta recente; usando alternativa")+"\n";
    report+="RTC: "+String(rtc_.available()?rtcValid_?"valido":"hora invalida":"indisponivel")+"\n";
    report+="RTC detalhe: "+String(rtc_.error())+"\n";
    if(lastNtpAtUs_)report+="Ultimo NTP ha: "+String((esp_timer_get_time()-lastNtpAtUs_)/1000000)+" s\n";
    if(driftKnown_)report+="Ultima diferenca NTP/manual - RTC: "+String(lastDriftSeconds_)+" s\n";
    report+="RTC gravacoes verificadas: "+String(rtcWrites_)+"\n";
    report+="RTC armazena UTC; fuso exibido UTC"+String(fusoHora>=0?"+":"")+String(fusoHora)+(dstAtivo?" + DST":"")+"\n";
    return report;
}
