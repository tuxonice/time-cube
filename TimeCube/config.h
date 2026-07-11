#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <Preferences.h>

struct Configuration {
  String wifiNetwork = "";
  String wifiPassword = "";
  int settleTime = 2000;
  String endpointBaseUrl = "";
  String endpointToken = "";
  int interruptPin = 34;
  bool enableSleepMode = true;
  int motionThreshold = 20;
  int sleepDelayMs = 5000;
};

extern Configuration systemConfiguration;
extern Preferences preferences;

void readConfig();
bool saveConfig();

#endif
