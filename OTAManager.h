#pragma once
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <esp32-hal-cpu.h>
#include "ClimaManager.h"
#include "DisplayUtil.h"


class OTAManager {
 DisplayUtil& display;
 ClimaManager& climate_;
 bool active_ = false;
 uint32_t startedAt_ = 0, bytes_ = 0, savedCpu_ = 240;
 int lastProgress_ = -10;
 int lastScreenPercent_ = 0;
 uint32_t lastScreenAt_ = 0;
 UBaseType_t savedPriority_ = 1;

 void finish() {
   if (!active_) return;
   active_ = false;
   climate_.pauseForOta(false);
   vTaskPrioritySet(nullptr, savedPriority_);
   if (savedCpu_ != 240) setCpuFrequencyMhz(savedCpu_);
   // Keep sleep disabled: a failed upload can be retried without radio wake delays.
 }
public:
 explicit OTAManager(DisplayUtil& displayRef, ClimaManager& climate) : display(displayRef), climate_(climate) {}
 void begin(const char* hostname = "AMOLED-Gateway") {
   ArduinoOTA.setHostname(hostname);
   WiFi.setSleep(false);
   ArduinoOTA.onStart([this]() {
     active_ = true;
     startedAt_ = millis(); bytes_ = 0; lastProgress_ = -10;
     lastScreenPercent_ = 0; lastScreenAt_ = startedAt_;
     savedCpu_ = getCpuFrequencyMhz();
     if (savedCpu_ != 240) setCpuFrequencyMhz(240);
     savedPriority_ = uxTaskPriorityGet(nullptr);
     if (savedPriority_ < 2) vTaskPrioritySet(nullptr, 2);
     climate_.pauseForOta(true);
     WiFi.setSleep(false);
     display.wake();
     display.showOtaProgress(0, "Gravando...");
     Serial.println("[OTA] Transferencia prioritaria iniciada");
   });
   ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
     bytes_ = progress;
     const int percent = total ? uint64_t(progress) * 100 / total : 0;
     // Draw at most four frames/second and only after a meaningful advance.
     if ((percent >= lastScreenPercent_ + 5 && millis() - lastScreenAt_ >= 250) ||
         (percent == 100 && lastScreenPercent_ != 100)) {
       lastScreenPercent_ = percent;
       lastScreenAt_ = millis();
       display.showOtaProgress(percent, percent == 100 ? "Verificando..." : "Gravando...");
     }
     if (percent == 100 || percent >= lastProgress_ + 10) {
       lastProgress_ = percent;
       Serial.printf("[OTA] %d%%\n", percent);
     }
   });
   ArduinoOTA.onEnd([this]() {
     display.showOtaProgress(100, "Concluido!");
     const uint32_t elapsed = max(uint32_t(1), uint32_t(millis() - startedAt_));
     Serial.printf("[OTA] Concluido: %lu bytes em %.2f s (%.1f KiB/s)\n",
                   (unsigned long)bytes_, elapsed / 1000.0,
                   bytes_ * 1000.0 / (1024.0 * elapsed));
     finish();
   });
   ArduinoOTA.onError([this](ota_error_t error) {
     display.showOtaProgress(lastScreenPercent_, "Erro " + String(error));
     Serial.printf("[OTA] Erro %u; retomando servicos\n", error);
     finish();
   });
   ArduinoOTA.begin();
 }
 void handle() {
   // The first call accepts a pending invitation; the second starts it immediately.
   ArduinoOTA.handle();
   ArduinoOTA.handle();
 }
};
