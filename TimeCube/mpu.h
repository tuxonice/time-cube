#ifndef MPU_H
#define MPU_H

#include <Arduino.h>

void setupMPU();
void calibrateGyro();
void loopMPU();

const char* faceName(int face);

#endif

