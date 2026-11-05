#pragma once
#include "BoardHardware.h"
#include <math.h>

// Uses acceleration in g; rejects shocks and requires a stable portrait pose.
class MotionGestures final {
public:
    explicit MotionGestures(BoardHardware& board):board_(board){}
    struct Event {bool wake=false;int rotation=-1;};
    Event update(bool enabled,uint8_t currentRotation) {
        Event event;const uint32_t now=millis();
        if(!enabled){reference_=false;candidate_=-1;basePose_=-1;flatSince_=0;return event;}
        if(now-lastSample_<80)return event;lastSample_=now;
        float x,y,z;
        if(!board_.readAcceleration(&x,&y,&z) || !isfinite(x) || !isfinite(y) || !isfinite(z))return event;
        const float norm=sqrtf(x*x+y*y+z*z);
        if(norm<0.75f || norm>1.25f){candidate_=-1;return event;}
        if(!reference_ || now-lastValid_>1500){fx_=x;fy_=y;fz_=z;flat_=false;reference_=true;}
        lastValid_=now;fx_+=(x-fx_)*0.22f;fy_+=(y-fy_)*0.22f;fz_+=(z-fz_)*0.22f;
        // A face-up rest followed by a deliberate lift, independent of Wi-Fi/time.
        if(fabsf(fz_)>0.86f){if(!flatSince_)flatSince_=now;if(now-flatSince_>=800)flat_=true;}
        else flatSince_=0;
        if(flat_ && fabsf(fz_)<0.65f && now-lastWake_>4000){event.wake=true;lastWake_=now;flat_=false;}
        int pose=-1;
        // Match the sensor-to-canvas convention already used by EyesAnimation.
        if(fabsf(fy_)>0.72f && fabsf(fx_)<0.40f && fabsf(fz_)<0.65f)pose=fy_>0?0:2;
        if(pose!=candidate_){candidate_=pose;candidateAt_=now;}
        if(pose>=0 && now-candidateAt_>=1200) {
            // Learn mounting orientation from the first stable pose rather than guessing axis polarity.
            if(basePose_<0){basePose_=pose;baseRotation_=currentRotation & 2;}
            const int desired=pose==basePose_?baseRotation_:(baseRotation_^2);
            if(desired!=currentRotation)event.rotation=desired;
        }
        return event;
    }
private:
    BoardHardware& board_;bool reference_=false,flat_=false;
    float fx_=0,fy_=0,fz_=0;int candidate_=-1,basePose_=-1,baseRotation_=0;
    uint32_t lastSample_=0,lastValid_=0,flatSince_=0,lastWake_=0,candidateAt_=0;
};
