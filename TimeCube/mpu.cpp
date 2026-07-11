#include "mpu.h"
#include <Wire.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include "config.h"
#include "network.h"

static const uint8_t MPU_ADDR = 0x68;

// Raw sensor values
int16_t acc_x, acc_y, acc_z;
int16_t temp_raw;
int16_t gyro_x, gyro_y, gyro_z;

// Gyro calibration offsets
long gyro_x_offset = 0;
long gyro_y_offset = 0;
long gyro_z_offset = 0;

// Face detection state
int candidateFace = -1;
int stableFace = -1;
int lastPostedFace = -1;
unsigned long candidateSinceMs = 0;

// Sleep state
volatile bool motionDetected = false;
unsigned long lastMotionTime = 0;
bool isSleeping = false;

// Tuning
const float DOMINANCE_RATIO = 1.25f;
const int GYRO_STABLE_THRESHOLD = 300;
const unsigned long PRINT_INTERVAL_MS = 250;
const unsigned long SAMPLE_INTERVAL_MS = 50;

unsigned long lastPrintMs = 0;
unsigned long lastSampleMs = 0;

static void writeRegister(uint8_t reg, uint8_t value);
static bool readMPU();
static bool isCubeStable();
static int getFaceFromAccel();
static void updateStableFace(int detectedFace, bool stableMotion);
static void printStatus(bool stableMotion, int detectedFace);
static void onFaceChanged(int face);
static void IRAM_ATTR motionISR();

void setupMPU() {
  Wire.begin();

  // Wake up MPU-6050
  writeRegister(0x6B, 0x00);

  // Accelerometer config: +/- 8g
  writeRegister(0x1C, 0x10);

  // Gyro config: 500 dps
  writeRegister(0x1B, 0x08);

  // Low pass filter
  writeRegister(0x1A, 0x03);
}

void setupMPUInterrupt() {
  // Configure motion detection threshold (MOT_THR)
  writeRegister(0x1F, systemConfiguration.motionThreshold);
  
  // Set motion detection duration (MOT_DUR) - 1 LSB = 1 ms @ 1 kHz
  writeRegister(0x20, 1);  // 1 ms duration
  
  // Disable motion detection initially to configure it
  writeRegister(0x38, 0x00);  // Clear INT_ENABLE
  
  // Configure motion detection on all axes
  writeRegister(0x69, 0x07);  // Enable motion detection on X, Y, Z axes
  
  // Enable motion detection interrupt
  writeRegister(0x38, 0x40);  // Enable MOT_EN bit in INT_ENABLE
  
  // Configure interrupt pin behavior (INT_PIN_CFG)
  writeRegister(0x37, 0x20);  // INT pin active high, push-pull, latch until cleared
  
  // Attach interrupt handler
  pinMode(systemConfiguration.interruptPin, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(systemConfiguration.interruptPin), motionISR, RISING);
  
  Serial.print("MPU interrupt configured on pin ");
  Serial.println(systemConfiguration.interruptPin);
}

static void writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

void calibrateGyro() {
  const int samples = 1000;

  Serial.println("Calibrating gyro... keep cube still");

  long sumX = 0;
  long sumY = 0;
  long sumZ = 0;
  int valid = 0;

  for (int i = 0; i < samples; i++) {
    if (readMPU()) {
      sumX += gyro_x;
      sumY += gyro_y;
      sumZ += gyro_z;
      valid++;
    }
    delay(3);
  }

  if (valid == 0) {
    Serial.println("Gyro calibration failed");
    return;
  }

  gyro_x_offset = sumX / valid;
  gyro_y_offset = sumY / valid;
  gyro_z_offset = sumZ / valid;

  Serial.print("Gyro offsets: ");
  Serial.print(gyro_x_offset);
  Serial.print(", ");
  Serial.print(gyro_y_offset);
  Serial.print(", ");
  Serial.println(gyro_z_offset);
}

static bool readMPU() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  uint8_t bytesRequested = 14;
  uint8_t bytesReceived = Wire.requestFrom(MPU_ADDR, bytesRequested, (uint8_t)true);
  if (bytesReceived != bytesRequested) {
    return false;
  }

  acc_x    = (Wire.read() << 8) | Wire.read();
  acc_y    = (Wire.read() << 8) | Wire.read();
  acc_z    = (Wire.read() << 8) | Wire.read();
  temp_raw = (Wire.read() << 8) | Wire.read();
  gyro_x   = (Wire.read() << 8) | Wire.read();
  gyro_y   = (Wire.read() << 8) | Wire.read();
  gyro_z   = (Wire.read() << 8) | Wire.read();

  gyro_x -= gyro_x_offset;
  gyro_y -= gyro_y_offset;
  gyro_z -= gyro_z_offset;

  return true;
}

static bool isCubeStable() {
  return abs(gyro_x) < GYRO_STABLE_THRESHOLD &&
         abs(gyro_y) < GYRO_STABLE_THRESHOLD &&
         abs(gyro_z) < GYRO_STABLE_THRESHOLD;
}

static int getFaceFromAccel() {
  long ax = acc_x;
  long ay = acc_y;
  long az = acc_z;

  long absX = abs(ax);
  long absY = abs(ay);
  long absZ = abs(az);

  if (absX < 1500 && absY < 1500 && absZ < 1500) {
    return -1;
  }

  if (absX > absY * DOMINANCE_RATIO && absX > absZ * DOMINANCE_RATIO) {
    return (ax > 0) ? 0 : 1;
  }

  if (absY > absX * DOMINANCE_RATIO && absY > absZ * DOMINANCE_RATIO) {
    return (ay > 0) ? 2 : 3;
  }

  if (absZ > absX * DOMINANCE_RATIO && absZ > absY * DOMINANCE_RATIO) {
    return (az > 0) ? 4 : 5;
  }

  return -1;
}

static void updateStableFace(int detectedFace, bool stableMotion) {
  unsigned long now = millis();

  if (!stableMotion || detectedFace == -1) {
    candidateFace = -1;
    candidateSinceMs = 0;
    return;
  }

  if (detectedFace != candidateFace) {
    candidateFace = detectedFace;
    candidateSinceMs = now;
    return;
  }

  if (stableFace != candidateFace &&
      (now - candidateSinceMs >= (unsigned long)systemConfiguration.settleTime)) {
    stableFace = candidateFace;

    Serial.print("CONFIRMED FACE UP: ");
    Serial.print(faceName(stableFace));
    Serial.print(" | acc=(");
    Serial.print(acc_x);
    Serial.print(", ");
    Serial.print(acc_y);
    Serial.print(", ");
    Serial.print(acc_z);
    Serial.print(") gyro=(");
    Serial.print(gyro_x);
    Serial.print(", ");
    Serial.print(gyro_y);
    Serial.print(", ");
    Serial.print(gyro_z);
    Serial.println(")");

    onFaceChanged(stableFace);
  }
}

static void printStatus(bool stableMotion, int detectedFace) {
  unsigned long now = millis();
  if (now - lastPrintMs < PRINT_INTERVAL_MS) {
    return;
  }
  lastPrintMs = now;

  Serial.print("motion=");
  Serial.print(stableMotion ? "STILL" : "MOVING");

  Serial.print(" | detected=");
  Serial.print(faceName(detectedFace));

  Serial.print(" | stable=");
  Serial.print(faceName(stableFace));

  Serial.print(" | acc=(");
  Serial.print(acc_x);
  Serial.print(", ");
  Serial.print(acc_y);
  Serial.print(", ");
  Serial.print(acc_z);
  Serial.print(")");

  Serial.print(" | gyro=(");
  Serial.print(gyro_x);
  Serial.print(", ");
  Serial.print(gyro_y);
  Serial.print(", ");
  Serial.print(gyro_z);
  Serial.println(")");
}

static void onFaceChanged(int face) {
  if (face == lastPostedFace) {
    return;
  }

  lastPostedFace = face;

  if (WiFi.status() == WL_CONNECTED) {
    //String path = "/face/" + String(face);
    postToEndpoint(faceName(face));
  }
}

static void IRAM_ATTR motionISR() {
  motionDetected = true;
}

void enterSleepMode() {
  if (!systemConfiguration.enableSleepMode || isSleeping) {
    return;
  }
  
  Serial.println("Entering deep sleep mode...");
  isSleeping = true;
  
  // Configure wake-up source
  esp_sleep_enable_ext0_wakeup((gpio_num_t)systemConfiguration.interruptPin, HIGH);
  
  // Enter deep sleep
  esp_deep_sleep_start();
}

void wakeFromInterrupt() {
  // Clear the interrupt flag by reading the interrupt status register
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3A); // INT_STATUS register
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 1, (uint8_t)true);
  Wire.read(); // Clear the interrupt
  
  motionDetected = false;
  isSleeping = false;
  lastMotionTime = millis();
  
  Serial.println("Woke from motion interrupt");
}

void loopMPU() {
  unsigned long now = millis();
  
  // Check if we should enter sleep mode
  if (systemConfiguration.enableSleepMode && 
      stableFace != -1 && 
      !isSleeping && 
      (now - lastMotionTime > systemConfiguration.sleepDelayMs)) {
    enterSleepMode();
    return;
  }
  
  if (now - lastSampleMs < SAMPLE_INTERVAL_MS) {
    return;
  }
  lastSampleMs = now;

  if (!readMPU()) {
    Serial.println("MPU read failed");
    return;
  }

  bool stableMotion = isCubeStable();
  int detectedFace = -1;

  // Update motion tracking
  if (!stableMotion) {
    lastMotionTime = now;
  }

  if (stableMotion) {
    detectedFace = getFaceFromAccel();
  }

  updateStableFace(detectedFace, stableMotion);
  printStatus(stableMotion, detectedFace);
}

const char* faceName(int face) {
  switch (face) {
    case 0: return "RED";
    case 1: return "ORANGE";
    case 2: return "WHITE";
    case 3: return "YELLOW";
    case 4: return "GREEN";
    case 5: return "BLUE";
    default: return "UNKNOWN";
  }
}
