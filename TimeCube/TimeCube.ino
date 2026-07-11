#include <Arduino.h>
#include "config.h"
#include "network.h"
#include "webui.h"
#include "mpu.h"

bool apMode = false;

void setup() {
  Serial.begin(115200);
  delay(200);

  Serial.println("Reading configuration");
  readConfig();

  if (systemConfiguration.wifiNetwork == "" ||
      systemConfiguration.wifiPassword == "" ||
      !wifiConnect(15000)) {
    apConnect();
    apMode = true;
  }

  setupWebServer();
  Serial.println("HTTP server started");

  if (!apMode) {
    setupMPU();
    calibrateGyro();
    setupMPUInterrupt();
    
    // Check if we woke from deep sleep
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
      wakeFromInterrupt();
    }

    Serial.println();
    Serial.println("MPU-6050 cube face detection started");
    Serial.println("Using accelerometer for face, gyroscope for motion filtering");
    Serial.print("Sleep mode: ");
    Serial.println(systemConfiguration.enableSleepMode ? "ENABLED" : "DISABLED");
  }
}

void loop() {
  webServer.handleClient();

  if (apMode) {
    delay(50);
    return;
  }

  loopMPU();
}
