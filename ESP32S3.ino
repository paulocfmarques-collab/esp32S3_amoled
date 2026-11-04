#include "Application.h"

void setup() {
    Serial.begin(115200);
    delay(1500); // Allow the USB monitor to reconnect after a reset.
    Serial.printf("[BOOT] AMOLED Gateway V%d; constructing services...\n", AMOLED_BOARD_REVISION);
    static Application application;
    Serial.println("[BOOT] Services constructed; starting board...");
    application.begin();
}

void loop() { vTaskDelay(pdMS_TO_TICKS(1000)); }
