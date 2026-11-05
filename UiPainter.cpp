#include "UiPainter.h"
#include "ScreenLayout.h"
#include "UiFonts/FreeSans9pt7b.h"
#include "UiFonts/FreeSans12pt7b.h"
#include "UiFonts/FreeSansBold18pt7b.h"
#include "UiFonts/FreeSansBold24pt7b.h"
#include <WiFi.h>
#include <math.h>

void UiPainter::select(Font font) {
    const GFXfont* face=&FreeSans9pt7b;
    if(font==Font::Body)face=&FreeSans12pt7b;
    if(font==Font::Title)face=&FreeSansBold18pt7b;
    if(font==Font::Large || font==Font::Hero)face=&FreeSansBold24pt7b;
    canvas_.setFont(face); canvas_.setTextSize(font==Font::Hero?2:1); canvas_.setTextWrap(false);
}
void UiPainter::resetFont() { canvas_.setFont(static_cast<const GFXfont*>(nullptr)); canvas_.setTextSize(1); }
String UiPainter::ascii(String value) {
    const char* from[]={"á","à","ã","â","ä","Á","À","Ã","Â","é","ê","É","Ê","í","Í","ó","ô","õ","Ó","Ô","Õ","ú","ü","Ú","ç","Ç","°"};
    const char* to[]={"a","a","a","a","a","A","A","A","A","e","e","E","E","i","I","o","o","o","O","O","O","u","u","U","c","C","o"};
    for(size_t i=0;i<sizeof(from)/sizeof(from[0]);++i)value.replace(from[i],to[i]);
    String result; result.reserve(value.length());
    for(size_t i=0;i<value.length();++i)if(uint8_t(value[i])>=32 && uint8_t(value[i])<=126)result+=value[i];
    return result;
}
int UiPainter::textWidth(const String& value,Font font) {
    select(font); int16_t x=0,y=0;uint16_t w=0,h=0;
    canvas_.getTextBounds(ascii(value),0,0,&x,&y,&w,&h); resetFont();return w;
}
void UiPainter::text(String value,int x,int top,Font font,uint16_t color,int maxWidth,Align align) {
    value=ascii(value);
    if(maxWidth>0) {
        while(font!=Font::Small && textWidth(value,font)>maxWidth)font=static_cast<Font>(uint8_t(font)-1);
        if(textWidth(value,font)>maxWidth) {
            while(value.length() && textWidth(value+"...",font)>maxWidth)value.remove(value.length()-1);
            value+="...";
        }
    }
    select(font);int16_t bx=0,by=0;uint16_t w=0,h=0;
    canvas_.getTextBounds(value,0,0,&bx,&by,&w,&h);
    if(align==Align::Center)x-=w/2;else if(align==Align::Right)x-=w;
    canvas_.setTextColor(color);canvas_.setCursor(x-bx,top-by);canvas_.print(value);resetFont();
}
void UiPainter::card(int x,int y,int w,int h,uint16_t accent) {
    canvas_.fillRoundRect(x,y,w,h,16,Card);canvas_.drawRoundRect(x,y,w,h,16,Edge);
    canvas_.fillRoundRect(x,y+14,3,max(3,h-28),1,accent);
}
void UiPainter::meter(int x,int y,int width,int height,float percent,uint16_t color) {
    canvas_.fillRoundRect(x,y,width,height,height/2,Track);
    if(!isfinite(percent))return;
    const int used=int(constrain(percent,0.0f,100.0f)*width/100);
    if(used>0)canvas_.fillRoundRect(x,y,used,height,min(height/2,used/2),color);
}
void UiPainter::icon(Icon type,int x,int y,int size,uint16_t color) {
    auto X=[&](int v){return x+v*size/32;};auto Y=[&](int v){return y+v*size/32;};
    auto line=[&](int a,int b,int c,int d){canvas_.drawLine(X(a),Y(b),X(c),Y(d),color);};
    const int r=max(1,size/16);
    switch(type) {
    case Icon::Wifi:
        for(int arc=0;arc<3;++arc) {
            const float radius=(8+arc*7)*size/32.0f;
            for(int a=-45;a<45;++a) {
                const float aa=a*0.0174533f,bb=(a+1)*0.0174533f;
                canvas_.drawLine(X(16)+int(sinf(aa)*radius),Y(29)-int(cosf(aa)*radius),
                    X(16)+int(sinf(bb)*radius),Y(29)-int(cosf(bb)*radius),color);
            }
        }canvas_.fillCircle(X(16),Y(29),r,color);break;
    case Icon::Clock:case Icon::Sync:
        canvas_.drawCircle(X(16),Y(16),size*13/32,color);line(16,8,16,16);line(16,16,23,20);
        if(type==Icon::Sync){line(25,3,29,8);line(29,8,23,8);}break;
    case Icon::Chip:
        canvas_.drawRoundRect(X(7),Y(7),size*18/32,size*18/32,r,color);
        canvas_.fillRect(X(12),Y(12),size/4,size/4,color);
        for(int i=9;i<25;i+=6){line(i,2,i,7);line(i,25,i,30);line(2,i,7,i);line(25,i,30,i);}break;
    case Icon::Storage:
        canvas_.drawRoundRect(X(5),Y(2),size*22/32,size*28/32,r,color);
        for(int i=10;i<24;i+=5)canvas_.fillRect(X(i),Y(5),max(1,size*3/32),size*7/32,color);
        line(10,22,22,22);line(10,26,19,26);break;
    case Icon::Chart:line(3,4,3,29);line(3,29,30,29);line(6,23,12,17);line(12,17,18,21);line(18,21,27,7);break;
    case Icon::Cloud:
        canvas_.fillCircle(X(11),Y(17),size*7/32,color);canvas_.fillCircle(X(19),Y(12),size*9/32,color);
        canvas_.fillCircle(X(26),Y(18),size*6/32,color);canvas_.fillRect(X(9),Y(17),size*18/32,size*7/32,color);break;
    case Icon::Sun:
        canvas_.fillCircle(X(16),Y(16),size*7/32,color);
        for(int a=0;a<8;++a){float t=a*0.785398f;canvas_.drawLine(X(16)+int(cosf(t)*size*.34f),Y(16)+int(sinf(t)*size*.34f),X(16)+int(cosf(t)*size*.47f),Y(16)+int(sinf(t)*size*.47f),color);}break;
    case Icon::Console:canvas_.drawRoundRect(X(2),Y(4),size*28/32,size*24/32,r,color);line(7,11,12,16);line(12,16,7,21);line(16,22,24,22);break;
    case Icon::Power:canvas_.drawCircle(X(16),Y(18),size*11/32,color);canvas_.fillRect(X(12),Y(2),size/4,size*10/32,Card);line(16,2,16,16);break;
    case Icon::Battery:canvas_.drawRoundRect(X(2),Y(8),size*26/32,size*17/32,r,color);canvas_.fillRect(X(28),Y(13),max(1,size/10),size*7/32,color);break;
    }
}
void UiPainter::statusBar(uint8_t page,uint8_t pages,const BatterySnapshot& battery,bool pinned) {
    canvas_.fillRect(0,0,canvas_.width(),ScreenLayout::StatusHeight,0x0842);
    const bool online=WiFi.status()==WL_CONNECTED;
    const int rssi=online?WiFi.RSSI():-100;
    const bool ap=!online && (WiFi.getMode() & WIFI_AP);
    const uint16_t signal=online?(rssi>=-60?Green:rssi>=-75?Amber:rssi>=-85?uint16_t(0xFB80):Red):(ap?Amber:Red);
    text(String(page+1)+" / "+String(pages),14,9,Font::Small,Muted);
    if(pinned) {canvas_.drawRoundRect(76,13,8,7,1,Cyan);canvas_.drawCircle(80,12,3,Cyan);}
    const int bx=canvas_.width()/2-26;
    icon(Icon::Battery,bx,6,24,!battery.valid || !battery.connected?Muted:battery.percentValid && battery.percent<=20?Red:Green);
    if(battery.valid && battery.percentValid) {
        if(battery.percent>0)canvas_.fillRect(bx+3,13,max(1,int(battery.percent)*15/100),7,battery.charging?Cyan:Green);
        text(String(battery.percent)+"%",bx+30,9,Font::Small,Ink,65);
    } else text(battery.valid && battery.usb?"USB":"--%",bx+30,9,Font::Small,Muted,65);
    const int wx=canvas_.width()-38;
    const int bars=online?(rssi>=-60?4:rssi>=-70?3:rssi>=-80?2:1):0;
    for(int i=0;i<4;++i)canvas_.fillRoundRect(wx+i*6,24-(4+i*4),4,4+i*4,1,i<bars?signal:Track);
    if(!online){if(ap)canvas_.fillCircle(wx+12,16,3,Amber);else canvas_.drawLine(wx+2,7,wx+23,25,Red);}
    text(online?String(rssi):String(ap?"AP":"OFF"),wx-8,9,Font::Small,signal,50,Align::Right);
    canvas_.drawFastHLine(0,ScreenLayout::StatusHeight-1,canvas_.width(),Edge);
}
