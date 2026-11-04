#include "EyesAnimation.h"
#include <math.h>

// Conserva a animação original em coordenadas lógicas, ampliada para a AMOLED.
namespace {
class EyeCanvas final {
    Arduino_Canvas& canvas_;
    float scale_;
    int offsetX_, offsetY_;
    int x(int value) const { return offsetX_ + int(value*scale_); }
    int y(int value) const { return offsetY_ + int(value*scale_); }
    int size(int value) const { return max(1,int(value*scale_)); }
public:
    explicit EyeCanvas(Arduino_Canvas& canvas) : canvas_(canvas),
        scale_(min(canvas.width(),canvas.height())/128.0f),
        offsetX_((canvas.width()-int(128*scale_))/2), offsetY_((canvas.height()-int(128*scale_))/2) {}
    void fillScreen(uint16_t color) { canvas_.fillScreen(color); }
    void fillRoundRect(int a,int b,int w,int h,int r,uint16_t c) { canvas_.fillRoundRect(x(a),y(b),size(w),size(h),size(r),c); }
    void fillRect(int a,int b,int w,int h,uint16_t c) { canvas_.fillRect(x(a),y(b),size(w),size(h),c); }
    void fillCircle(int a,int b,int r,uint16_t c) { canvas_.fillCircle(x(a),y(b),size(r),c); }
    void drawCircle(int a,int b,int r,uint16_t c) { canvas_.drawCircle(x(a),y(b),size(r),c); }
    void drawLine(int a,int b,int c,int d,uint16_t color) { canvas_.drawLine(x(a),y(b),x(c),y(d),color); }
    void flush() { canvas_.flush(); }
};
}

void EyesAnimation::draw(Arduino_Canvas& canvas, uint8_t rotation) {
 const uint32_t now = millis();
 if (now - lastFrame < 33) return;
 // A new screensaver session uses the current position as its neutral pose.
 if (now - lastFrame > 250) referenceReady = false;
 lastFrame = now;
 float ax = 0, ay = 0, az = 0;
 bool suddenMotion = false;
 if (board_.readAcceleration(&ax, &ay, &az) &&
     isfinite(ax) && isfinite(ay) && isfinite(az)) {
   const float acceleration = sqrtf(ax * ax + ay * ay + az * az);
   suddenMotion = acceleration > 1.65f || acceleration < 0.35f;
   // Rotate the sensor axes into the current canvas orientation.
   float x = ax, y = -ay;
   switch (rotation & 3) {
     case 1: x = ay; y = ax; break;
     case 2: x = -ax; y = ay; break;
     case 3: x = -ay; y = -ax; break;
   }
   if (!referenceReady) {
     neutralX = filteredX = x; neutralY = filteredY = y;
     referenceReady = true;
   }
   filteredX += (x - filteredX) * 0.18f;
   filteredY += (y - filteredY) * 0.18f;
   const float dx = filteredX - neutralX, dy = filteredY - neutralY;
   // Ignore small vibrations, and keep both irises inside their eye outlines.
   targetX = fabsf(dx) < 0.03f ? 0 : constrain(dx * 12.0f, -6.0f, 6.0f);
   targetY = fabsf(dy) < 0.03f ? 0 : constrain(dy * 10.0f, -5.0f, 5.0f);
   lastAccelAt = now;
 }
 // Retain automatic motion only when no valid accelerometer data is available.
 if ((!referenceReady || now - lastAccelAt > 1000) &&
     (!nextLook || int32_t(now - nextLook) >= 0)) {
   targetX = random(-6, 7); targetY = random(-5, 6);
   nextLook = now + random(900, 2400);
 }
 lookX += (targetX - lookX) * 0.12f;
 lookY += (targetY - lookY) * 0.12f;
 if (suddenMotion && now - lastSurpriseAt >= 4000) {
   expression = SURPRISED; lastSurpriseAt = now; nextExpressionAt = now + 2000;
 } else if (!nextExpressionAt || int32_t(now - nextExpressionAt) >= 0) {
   // Pick a different expression without blocking the network or the animation.
   if (nextExpressionAt) expression = (expression + 1 + random(3)) % 4;
   nextExpressionAt = now + random(3500, 6500);
 }
 if (!nextBlink) nextBlink = now + random(2000, 5000);
 if (!blinking && int32_t(now - nextBlink) >= 0) { blinking = true; blinkAt = now; }
 int lid = 0;
 if (blinking) {
   const uint32_t elapsed = now - blinkAt;
   if (elapsed >= 200) { blinking = false; nextBlink = now + random(2500, 6000); }
   else lid = (elapsed < 100 ? elapsed : 200 - elapsed) * 25 / 100;
 }
 EyeCanvas frame(canvas);
 frame.fillScreen(0);
 for (int eye = 0; eye < 2; ++eye) {
   const int x = eye ? 70 : 12;
   const int cx = x + 23 + int(lookX), cy = 53 + int(lookY);
   const uint16_t iris = 0x07FF;
   frame.fillRoundRect(x, 28, 46, 50, 16, 0xDFFF);
   frame.fillCircle(cx, cy, 16, iris);
   frame.drawCircle(cx, cy, 13, 0x041F);
   frame.fillCircle(cx, cy, expression == SURPRISED ? 6 : 8, 0);
   frame.fillCircle(cx - 4, cy - 5, 3, 0xFFFF);
   frame.fillCircle(cx + 4, cy + 4, 1, 0xFFFF);
   const float targetTop = expression == SLEEPY ? 19 : expression == CURIOUS ? (eye ? 12 : 2) : 0;
   const float targetBottom = expression == HAPPY ? 6 : expression == SLEEPY ? 10 : 0;
   topLids[eye] += (targetTop - topLids[eye]) * 0.18f;
   bottomLids[eye] += (targetBottom - bottomLids[eye]) * 0.18f;
   const int top = max(lid, int(topLids[eye])), bottom = max(lid, int(bottomLids[eye]));
   if (top) frame.fillRect(x, 28, 46, top, 0);
   if (bottom) frame.fillRect(x, 78 - bottom, 46, bottom, 0);
   // Eyebrows reinforce the expression without touching the moving eyes.
   const int browY = expression == SURPRISED ? 13 : expression == SLEEPY ? 23 : 19;
   const int tilt = expression == CURIOUS ? (eye ? 5 : -5) : 0;
   for (int stroke = 0; stroke < 2; ++stroke)
     frame.drawLine(x + 8, browY + stroke, x + 37, browY + tilt + stroke, iris);
 }
 // Mouth sits below the eyes, entirely inside the 128x128 canvas.
 const int mouthX = 64 + int(lookX * 0.4f);
 if (expression == SURPRISED) {
   frame.fillRoundRect(mouthX - 7, 94, 14, 21, 7, 0x07FF);
   frame.fillRoundRect(mouthX - 4, 97, 8, 15, 4, 0);
 } else if (expression == SLEEPY) {
   frame.fillRoundRect(mouthX - 11, 103, 22, 4, 2, 0x07FF);
 } else if (expression == CURIOUS) {
   for (int stroke = 0; stroke < 3; ++stroke)
     frame.drawLine(mouthX - 10, 108 + stroke, mouthX + 10, 103 + stroke, 0x07FF);
 } else {
   // A curved smile, with a tiny breathing motion.
   const int bounce = int(sinf(now * 0.002f) * 1.5f);
   for (int dx = -17; dx <= 17; ++dx) {
     const int y = 102 + bounce + (289 - dx * dx) * 10 / 289;
     frame.fillCircle(mouthX + dx, y, 1, 0x07FF);
   }
 }
 frame.flush();
}
