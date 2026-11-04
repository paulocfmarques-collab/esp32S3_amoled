#ifndef RGBLED_H
#define RGBLED_H
#include <Arduino.h>
class DisplayUtil;
#define RED 0
#define GREEN 1
#define BLUE 2
#define YELLOW 3
#define CYAN 4
#define MAGENTA 5
#define WHITE 6

// Adaptador do indicador de status na tela; esta placa nao possui o RGB do Atom.
class RGBLed {
public:
 explicit RGBLed(DisplayUtil& display) : display_(display) {}
 bool available() const { return ready_; }
 void begin();
 void off(){setColor(0,0,0);}
 void red(){setColor(255,0,0);}
 void green(){setColor(0,255,0);}
 void blue(){setColor(0,0,255);}
 void yellow(){setColor(255,255,0);}
 void cyan(){setColor(0,255,255);}
 void magenta(){setColor(255,0,255);}
 void white(){setColor(255,255,255);}
 void setColor(uint8_t r,uint8_t g,uint8_t b);
 void setBrightness(uint8_t value);
 void breathing(uint8_t r,uint8_t g,uint8_t b,uint16_t=5){breathingTask(r,g,b);}
 void breathingTask(uint8_t r,uint8_t g,uint8_t b);
 void blink(uint8_t,uint16_t,uint16_t); // Legacy API; command animations use update().
private:
 DisplayUtil& display_;
 bool ready_=false;
 uint8_t r_=0,g_=0,b_=0,brightness_=80;
 uint32_t breathAt_=0;
 void apply();
};
#endif
