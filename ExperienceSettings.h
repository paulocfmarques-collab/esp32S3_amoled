#pragma once
#include <Arduino.h>
#include <Preferences.h>

// Small versioned preference record; histories never wear the NVS flash.
class ExperienceSettings final {
public:
    uint8_t face=0, accent=0, nightStart=22, nightEnd=7, nightBrightness=12;
    bool nightEnabled=true, motionEnabled=true;
    void begin() {
        Preferences p;if(!p.begin("experience",true))return;
        face=p.getUChar("face",0)%3;accent=p.getUChar("accent",0)%4;
        nightEnabled=p.getBool("night",true);motionEnabled=p.getBool("motion",true);
        nightStart=p.getUChar("start",22)%24;nightEnd=p.getUChar("end",7)%24;
        nightBrightness=constrain(p.getUChar("level",12),1,30);p.end();
    }
    bool save() const {
        Preferences p;if(!p.begin("experience",false))return false;
        p.putUChar("face",face);p.putUChar("accent",accent);p.putBool("night",nightEnabled);
        p.putBool("motion",motionEnabled);p.putUChar("start",nightStart);p.putUChar("end",nightEnd);p.putUChar("level",nightBrightness);
        const bool ok=p.getUChar("face",255)==face && p.getUChar("accent",255)==accent &&
            p.getBool("night",!nightEnabled)==nightEnabled && p.getBool("motion",!motionEnabled)==motionEnabled &&
            p.getUChar("start",255)==nightStart && p.getUChar("end",255)==nightEnd && p.getUChar("level",255)==nightBrightness;
        p.end();return ok;
    }
    bool nightAt(int hour) const {
        if(!nightEnabled || hour<0 || hour>23 || nightStart==nightEnd)return false;
        return nightStart<nightEnd?hour>=nightStart && hour<nightEnd:hour>=nightStart || hour<nightEnd;
    }
    const char* faceName() const {return face==0?"Instrumental":face==1?"Minimalista":"Digital";}
    uint16_t color() const {static constexpr uint16_t palette[]={0x07FF,0x4F70,0xFDC0,0xC39F};return palette[accent%4];}
};
