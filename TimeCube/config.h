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
};

extern Configuration systemConfiguration;
extern Preferences preferences;

void readConfig();
bool saveConfig();

#endif
