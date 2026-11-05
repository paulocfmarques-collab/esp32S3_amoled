#pragma once
#include "BatterySnapshot.h"
#include "SDUtil.h"

// Fixed-size session ring: 144 five-minute slots, including missing readings.
class BatteryHistory final {
public:
    struct Sample {int16_t percent=-1;uint16_t millivolts=0;bool charging=false;};
    static constexpr int Capacity=144;
    bool update(const BatterySnapshot& battery) {
        const uint32_t now=millis();if(started_ && now-lastAt_<300000)return false;
        started_=true;lastAt_=now;
        Sample value;
        if(battery.valid && battery.connected && battery.percentValid && now-battery.updatedAt<15000) {
            value.percent=battery.percent;value.millivolts=battery.voltageValid?battery.millivolts:0;value.charging=battery.charging;
        }
        values_[head_]=value;head_=(head_+1)%Capacity;if(count_<Capacity)++count_;return true;
    }
    int count() const{return count_;}
    const Sample& at(int index) const {return values_[(head_+Capacity-count_+index)%Capacity];}
    // Bounded CSV archive; the on-screen ring intentionally starts again after reboot.
    void archive(SDUtil& sd,const String& localTime,const BatterySnapshot& b) {
        constexpr uint64_t limit=128*1024;
        if(sd.getFileSize("/battery_history.csv")>=limit) {
            if(sd.exists("/battery_history.previous.csv") && !sd.removeFile("/battery_history.previous.csv"))return;
            if(!sd.renameFile("/battery_history.csv","/battery_history.previous.csv"))return;
        }
        if(!sd.exists("/battery_history.csv") && !sd.writeText("/battery_history.csv","local_time,uptime_s,percent,millivolts,charging,usb\n"))return;
        const auto& v=at(count_-1);
        const String row=localTime+","+String(millis()/1000)+","+(v.percent<0?String(""):String(v.percent))+","+
            (v.millivolts?String(v.millivolts):String(""))+","+String(v.charging?1:0)+","+String(b.valid && b.usb?1:0)+"\n";
        if(!sd.appendText("/battery_history.csv",row))Serial.println("[BATTERY] Falha ao arquivar historico no SD");
    }
private:
    Sample values_[Capacity]{};int head_=0,count_=0;bool started_=false;uint32_t lastAt_=0;
};
