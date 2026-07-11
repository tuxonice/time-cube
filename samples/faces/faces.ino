/*
  MPU-6050 Face Up Detection
  Detects which cube face is pointing up using accelerometer data

  Face mapping:
    0 = +X
    1 = -X
    2 = +Y
    3 = -Y
    4 = +Z
    5 = -Z
*/

#include <Wire.h>

// Raw accelerometer and gyro values
int16_t acc_x, acc_y, acc_z;
int16_t temp;
int16_t gyro_x, gyro_y, gyro_z;

// For stable face detection
int currentFace = -1;
int lastReportedFace = -1;
unsigned long faceChangeTime = 0;

// Require the same face for this long before confirming
const unsigned long STABLE_TIME_MS = 1000;

// Threshold to reject uncertain positions
const float DOMINANCE_RATIO = 1.2;

void setup() {
  Serial.begin(115200);
  Wire.begin();

  setup_mpu_6050_registers();

  Serial.println("MPU-6050 face detection started");
}

void loop() {
  read_mpu_6050_data();

  int detectedFace = getFaceFromAccel();

  if (detectedFace != currentFace) {
    currentFace = detectedFace;
    faceChangeTime = millis();
  }

  if (currentFace != -1 &&
      currentFace != lastReportedFace &&
      millis() - faceChangeTime >= STABLE_TIME_MS) {
    lastReportedFace = currentFace;

    Serial.print("Face up: ");
    Serial.print(faceName(lastReportedFace));
    Serial.print("   | acc_x=");
    Serial.print(acc_x);
    Serial.print(" acc_y=");
    Serial.print(acc_y);
    Serial.print(" acc_z=");
    Serial.println(acc_z);
  }

  delay(50);
}

void setup_mpu_6050_registers() {
  // Wake up MPU-6050
  Wire.beginTransmission(0x68);
  Wire.write(0x6B);
  Wire.write(0x00);
  Wire.endTransmission();

  // Accelerometer config: +/-8g
  Wire.beginTransmission(0x68);
  Wire.write(0x1C);
  Wire.write(0x10);
  Wire.endTransmission();

  // Gyro config: 500 dps
  Wire.beginTransmission(0x68);
  Wire.write(0x1B);
  Wire.write(0x08);
  Wire.endTransmission();
}

void read_mpu_6050_data() {
  Wire.beginTransmission(0x68);
  Wire.write(0x3B);
  Wire.endTransmission();

  Wire.requestFrom(0x68, 14);
  while (Wire.available() < 14);

  acc_x  = Wire.read() << 8 | Wire.read();
  acc_y  = Wire.read() << 8 | Wire.read();
  acc_z  = Wire.read() << 8 | Wire.read();
  temp   = Wire.read() << 8 | Wire.read();
  gyro_x = Wire.read() << 8 | Wire.read();
  gyro_y = Wire.read() << 8 | Wire.read();
  gyro_z = Wire.read() << 8 | Wire.read();
}

int getFaceFromAccel() {
  long ax = acc_x;
  long ay = acc_y;
  long az = acc_z;

  long absX = abs(ax);
  long absY = abs(ay);
  long absZ = abs(az);

  // Find the dominant axis with a little margin
  if (absX > absY * DOMINANCE_RATIO && absX > absZ * DOMINANCE_RATIO) {
    return (ax > 0) ? 0 : 1;
  }

  if (absY > absX * DOMINANCE_RATIO && absY > absZ * DOMINANCE_RATIO) {
    return (ay > 0) ? 2 : 3;
  }

  if (absZ > absX * DOMINANCE_RATIO && absZ > absY * DOMINANCE_RATIO) {
    return (az > 0) ? 4 : 5;
  }

  // Not clearly on one face
  return -1;
}

bool isStable() {
  return abs(gyro_x) < 200 &&
         abs(gyro_y) < 200 &&
         abs(gyro_z) < 200;
}

const char* faceName(int face) {
  switch (face) {
    case 0: return "+X";
    case 1: return "-X";
    case 2: return "+Y";
    case 3: return "-Y";
    case 4: return "+Z";
    case 5: return "-Z";
    default: return "UNKNOWN";
  }
}
