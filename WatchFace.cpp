#include "WatchFace.h"
#include "ScreenLayout.h"
#include "UiPainter.h"
#include <math.h>

void WatchFace::centered(Arduino_Canvas& canvas,const String& text,int x,int y,uint8_t size,uint16_t color) {
    UiPainter ui(canvas);
    const auto font=size<=1?UiPainter::Font::Small:size==2?UiPainter::Font::Body:UiPainter::Font::Title;
    ui.text(text,x,y,font,color,canvas.width()-72,UiPainter::Align::Center);
}
void WatchFace::hand(Arduino_Canvas& canvas,int cx,int cy,float radians,float length,float width,uint16_t color) {
    const float dx=sinf(radians),dy=-cosf(radians);
    const int tx=cx+int(dx*length),ty=cy+int(dy*length);
    const int lx=cx+int(-dy*width),ly=cy+int(dx*width);
    const int rx=cx-int(-dy*width),ry=cy-int(dx*width);
    canvas.fillTriangle(lx,ly,rx,ry,tx,ty,0x0000);
    canvas.fillTriangle(cx+int(-dy*max(1.0f,width-2)),cy+int(dx*max(1.0f,width-2)),
        cx-int(-dy*max(1.0f,width-2)),cy-int(dx*max(1.0f,width-2)),tx,ty,color);
}
void WatchFace::draw(Arduino_Canvas& canvas,const String& timestamp,const BatterySnapshot& battery,
                     bool online,bool weatherValid,float temperature,int32_t timezoneHours,bool daylightSaving,const String& timeSource,uint8_t style,uint16_t accent) {
    constexpr float Pi=3.14159265359f;
    const uint16_t Cyan=accent;
    constexpr uint16_t Silver=0xBDF7,Dim=0x2148,Face=0x0842;
    int day=0,month=0,year=0,hour=0,minute=0,second=0;
    const bool valid=timestamp.length()==19 && sscanf(timestamp.c_str(),"%d/%d/%d %d:%d:%d",&day,&month,&year,&hour,&minute,&second)==6 &&
        month>=1 && month<=12 && day>=1 && day<=31 && hour>=0 && hour<24 && minute>=0 && minute<60 && second>=0 && second<60;
    canvas.fillScreen(0); canvas.setTextWrap(false);
    const int contentHeight=ScreenLayout::footerTop(canvas.height())-ScreenLayout::StatusHeight;
    const int cx=canvas.width()/2,cy=ScreenLayout::StatusHeight+contentHeight/2;
    const int radius=min((int(canvas.width())-24)/2,(contentHeight-12)/2);
    if(style==2) {
        UiPainter ui(canvas);
        const int top=ScreenLayout::StatusHeight+12;
        const int available=contentHeight-24;
        const bool compact=contentHeight<350;
        ui.text("TEMPO LOCAL",cx,top,UiPainter::Font::Small,Silver,canvas.width()-40,UiPainter::Align::Center);
        ui.text(valid?timestamp.substring(11,16):String("--:--"),cx,top+available*13/100,compact?UiPainter::Font::Large:UiPainter::Font::Hero,Cyan,canvas.width()-32,UiPainter::Align::Center);
        ui.text(valid?timestamp.substring(17,19):String("--"),cx,top+available*39/100,UiPainter::Font::Large,Silver,canvas.width()-40,UiPainter::Align::Center);
        ui.meter(28,top+available*60/100,canvas.width()-56,4,valid?second*100.0f/59:0,Cyan);
        ui.text(valid?timestamp.substring(0,10):String("Sem hora valida"),cx,top+available*67/100,UiPainter::Font::Body,UiPainter::Ink,canvas.width()-40,UiPainter::Align::Center);
        ui.text(timeSource+"  /  UTC"+String(timezoneHours+(daylightSaving?1:0)),cx,top+available*79/100,UiPainter::Font::Small,Cyan,canvas.width()-40,UiPainter::Align::Center);
        ui.text(battery.valid && battery.connected && battery.percentValid?String(battery.percent)+"%  "+(battery.charging?"CARREGANDO":"BATERIA"):String("BATERIA --"),cx,top+available*91/100,UiPainter::Font::Small,Silver,canvas.width()-40,UiPainter::Align::Center);
        return;
    }
    if(style==1) {
        for(int i=0;i<60;++i) {
            const float angle=i*Pi/30;const int outer=radius-8,inner=outer-(i%5==0?12:3);
            canvas.drawLine(cx+int(sinf(angle)*inner),cy-int(cosf(angle)*inner),cx+int(sinf(angle)*outer),cy-int(cosf(angle)*outer),i%5==0?Silver:Dim);
        }
        centered(canvas,valid?timestamp.substring(0,10):String("SEM HORA"),cx,cy-radius/2,1,Silver);
        centered(canvas,timeSource,cx,cy+radius/2,1,Cyan);
        if(valid) {
            hand(canvas,cx,cy,(hour%12+minute/60.0f)*Pi/6,radius*0.47f,6,Silver);
            hand(canvas,cx,cy,(minute+second/60.0f)*Pi/30,radius*0.75f,4,Cyan);
            const float a=second*Pi/30;
            canvas.drawLine(cx,cy,cx+int(sinf(a)*(radius-20)),cy-int(cosf(a)*(radius-20)),UiPainter::Amber);
        }
        canvas.fillCircle(cx,cy,5,Cyan);
        centered(canvas,battery.valid && battery.connected && battery.percentValid?String(battery.percent)+"%":String("--%"),cx,cy+radius*3/4,1,Silver);
        return;
    }
    canvas.fillCircle(cx,cy,radius,0x18E3);
    canvas.fillCircle(cx,cy,radius-6,Face);
    canvas.drawCircle(cx,cy,radius,Silver);
    canvas.drawCircle(cx,cy,radius-8,Dim);
    canvas.drawCircle(cx,cy,radius-20,Dim);
    // Subtle dial texture clipped mathematically to the inner circle.
    for(int y=-radius+24;y<radius-24;y+=8) {
        const int half=int(sqrtf(float((radius-24)*(radius-24)-y*y)));
        canvas.drawFastHLine(cx-half,cy+y,half*2,0x1063);
    }
    for(int tick=0;tick<60;++tick) {
        const float angle=tick*Pi/30;
        const bool major=tick%5==0;
        const int outer=radius-10,inner=outer-(major?13:5);
        canvas.drawLine(cx+int(sinf(angle)*inner),cy-int(cosf(angle)*inner),
            cx+int(sinf(angle)*outer),cy-int(cosf(angle)*outer),major?Cyan:Dim);
        if(major)canvas.drawLine(cx+int(sinf(angle)*inner)+1,cy-int(cosf(angle)*inner),
            cx+int(sinf(angle)*outer)+1,cy-int(cosf(angle)*outer),Cyan);
    }
    const int numbers=radius-34;
    centered(canvas,"12",cx,cy-numbers-7,2,Silver);
    centered(canvas,"3",cx+numbers,cy-7,2,Silver);
    centered(canvas,"6",cx,cy+numbers-7,2,Silver);
    centered(canvas,"9",cx-numbers,cy-7,2,Silver);
    // Battery segment ring uses the PMIC fuel-gauge percentage; absent data stays dim.
    for(int i=0;i<40;++i) {
        const float angle=(125.0f+i*110.0f/39)*Pi/180;
        const uint16_t color=battery.valid && battery.percentValid && i*100<40*battery.percent ? Cyan : Dim;
        canvas.drawLine(cx+int(sinf(angle)*(radius-4)),cy-int(cosf(angle)*(radius-4)),
            cx+int(sinf(angle)*(radius-1)),cy-int(cosf(angle)*(radius-1)),color);
    }
    static constexpr const char* months[]={"JAN","FEV","MAR","ABR","MAI","JUN","JUL","AGO","SET","OUT","NOV","DEZ"};
    char date[20];
    if(valid)snprintf(date,sizeof(date),"%02d %s",day,months[month-1]); else snprintf(date,sizeof(date),"SEM HORA");
    const int plaqueY=cy-radius*52/100;
    canvas.fillRoundRect(cx-radius*56/100,plaqueY-5,radius*112/100,45,8,0x0864);
    canvas.drawRoundRect(cx-radius*56/100,plaqueY-5,radius*112/100,45,8,Dim);
    centered(canvas,date,cx,plaqueY,2,0xFFFF);
    const int zone=timezoneHours+(daylightSaving?1:0);
    centered(canvas,String("UTC")+(zone>=0?"+":"")+String(zone)+"  "+timeSource,cx,plaqueY+23,1,Cyan);
    char digital[12]; if(valid)snprintf(digital,sizeof(digital),"%02d:%02d:%02d",hour,minute,second);else snprintf(digital,sizeof(digital),"--:--:--");
    const int digitalY=cy+radius*16/100;
    canvas.fillRoundRect(cx-radius*64/100,digitalY-7,radius*128/100,38,7,0x0023);
    canvas.drawRoundRect(cx-radius*64/100,digitalY-7,radius*128/100,38,7,Dim);
    centered(canvas,digital,cx,digitalY,3,Cyan);
    String charge=battery.valid && battery.connected ?
        String(battery.percentValid?String(battery.percent)+"%":String("--%"))+
        (battery.charging?" CARGA":battery.discharging?" BATERIA":" EM ESPERA") :
        String(battery.valid && battery.usb?"USB / SEM BATERIA":"BATERIA --");
    centered(canvas,charge,cx,cy+radius*45/100,1,Silver);
    String weather=weatherValid?String(temperature,1)+" C  PORTO ALEGRE":String("CLIMA AGUARDANDO");
    centered(canvas,weather,cx,cy+radius*61/100,1,Cyan);
    if(valid) {
        hand(canvas,cx,cy,(hour%12+minute/60.0f+second/3600.0f)*Pi/6,radius*0.48f,7,0x9FFF);
        hand(canvas,cx,cy,(minute+second/60.0f)*Pi/30,radius*0.76f,5,Cyan);
        const float angle=second*Pi/30;
        canvas.drawLine(cx-int(sinf(angle)*18),cy+int(cosf(angle)*18),
            cx+int(sinf(angle)*(radius-25)),cy-int(cosf(angle)*(radius-25)),0xFFE0);
        canvas.fillCircle(cx+int(sinf(angle)*(radius-25)),cy-int(cosf(angle)*(radius-25)),2,0xFFE0);
    }
    canvas.fillCircle(cx,cy,7,0x18E3); canvas.drawCircle(cx,cy,7,Cyan);canvas.fillCircle(cx,cy,2,0xFFFF);
}
