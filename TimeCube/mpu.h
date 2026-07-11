#ifndef MPU_H
#define MPU_H

#include <Arduino.h>

void setupMPU();
void calibrateGyro();
void loopMPU();
void setupMPUInterrupt();
void enterSleepMode();
void wakeFromInterrupt();

const char* faceName(int face);

#endif
