#include "RGBLed.h"
#include "DisplayUtil.h"

void RGBLed::begin() {
 ready_=false; Serial.println("[RGB] Sem LED RGB integrado; status exibido na tela");
}
void RGBLed::apply() {
 const uint8_t r=uint16_t(r_)*brightness_/255, g=uint16_t(g_)*brightness_/255, b=uint16_t(b_)*brightness_/255;
 display_.setStatusColor((uint16_t(r & 0xF8)<<8) | (uint16_t(g & 0xFC)<<3) | (b>>3));
}
void RGBLed::setColor(uint8_t r,uint8_t g,uint8_t b){r_=r;g_=g;b_=b;apply();}
void RGBLed::setBrightness(uint8_t value){brightness_=value;apply();}
void RGBLed::breathingTask(uint8_t r,uint8_t g,uint8_t b){
 const uint32_t now=millis();if(now-breathAt_<20)return;breathAt_=now;
 const uint32_t phase=now%2400;const uint16_t level=(phase<1200?phase:2400-phase)*255/1200;
 setColor(uint16_t(r)*level/255,uint16_t(g)*level/255,uint16_t(b)*level/255);
}
void RGBLed::blink(uint8_t color,uint16_t,uint16_t){
 switch(color){case RED:red();break;case GREEN:green();break;case BLUE:blue();break;case YELLOW:yellow();break;case CYAN:cyan();break;case MAGENTA:magenta();break;default:white();}
}
