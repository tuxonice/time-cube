#include <Wire.h>

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
unsigned long candidateSinceMs = 0;

// Tuning
const float DOMINANCE_RATIO = 1.25f;          // higher = stricter face selection
const int GYRO_STABLE_THRESHOLD = 300;        // lower = stricter stillness
const unsigned long FACE_STABLE_MS = 350;     // time face must remain stable
const unsigned long PRINT_INTERVAL_MS = 250;  // periodic status print

unsigned long lastPrintMs = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin();

  setupMPU();
  calibrateGyro();

  Serial.println();
  Serial.println("MPU-6050 cube face detection started");
  Serial.println("Using accelerometer for face, gyroscope for motion filtering");
}

void loop() {
  readMPU();

  bool stableMotion = isCubeStable();
  int detectedFace = -1;

  if (stableMotion) {
    detectedFace = getFaceFromAccel();
  }

  updateStableFace(detectedFace, stableMotion);
  printStatus(stableMotion, detectedFace);

  delay(500);
}

void setupMPU() {
  // Wake up MPU-6050
  writeRegister(0x6B, 0x00);

  // Accelerometer config: +/- 8g
  writeRegister(0x1C, 0x10);

  // Gyro config: 500 dps
  writeRegister(0x1B, 0x08);

  // Optional: low pass filter
  writeRegister(0x1A, 0x03);
}

void writeRegister(uint8_t reg, uint8_t value) {
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

  for (int i = 0; i < samples; i++) {
    readMPU();
    sumX += gyro_x;
    sumY += gyro_y;
    sumZ += gyro_z;
    delay(3);
  }

  gyro_x_offset = sumX / samples;
  gyro_y_offset = sumY / samples;
  gyro_z_offset = sumZ / samples;

  Serial.print("Gyro offsets: ");
  Serial.print(gyro_x_offset);
  Serial.print(", ");
  Serial.print(gyro_y_offset);
  Serial.print(", ");
  Serial.println(gyro_z_offset);
}

void readMPU() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);

  Wire.requestFrom(MPU_ADDR, (uint8_t)14);
  while (Wire.available() < 14) {}

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
}

bool isCubeStable() {
  return abs(gyro_x) < GYRO_STABLE_THRESHOLD &&
         abs(gyro_y) < GYRO_STABLE_THRESHOLD &&
         abs(gyro_z) < GYRO_STABLE_THRESHOLD;
}

int getFaceFromAccel() {
  long ax = acc_x;
  long ay = acc_y;
  long az = acc_z;

  long absX = abs(ax);
  long absY = abs(ay);
  long absZ = abs(az);

  // Reject weak / ambiguous readings
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

void updateStableFace(int detectedFace, bool stableMotion) {
  unsigned long now = millis();

  // If moving or no clear face, reset candidate timer
  if (!stableMotion || detectedFace == -1) {
    candidateFace = -1;
    candidateSinceMs = 0;
    return;
  }

  // New candidate face
  if (detectedFace != candidateFace) {
    candidateFace = detectedFace;
    candidateSinceMs = now;
    return;
  }

  // Same candidate long enough -> commit
  if (stableFace != candidateFace && (now - candidateSinceMs >= FACE_STABLE_MS)) {
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
  }
}

void printStatus(bool stableMotion, int detectedFace) {
  unsigned long now = millis();
  if (now - lastPrintMs < PRINT_INTERVAL_MS) return;
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
