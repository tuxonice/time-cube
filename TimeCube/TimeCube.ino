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

    Serial.println();
    Serial.println("MPU-6050 cube face detection started");
    Serial.println("Using accelerometer for face, gyroscope for motion filtering");
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
