#include "config.h"
#include <Arduino.h>

Configuration systemConfiguration;
Preferences preferences;

void readConfig() {
  if (!preferences.begin("time-cube", true)) {
    Serial.println("Failed to open Preferences (read mode)!");
    return;
  }

  systemConfiguration.wifiNetwork = preferences.getString("network", "");
  systemConfiguration.wifiPassword = preferences.getString("password", "");
  systemConfiguration.settleTime = preferences.getInt("settle-time", 2000);
  systemConfiguration.endpointBaseUrl = preferences.getString("endpoint-url", "");
  systemConfiguration.endpointToken = preferences.getString("endpoint-token", "");

  preferences.end();
}

bool saveConfig() {
  if (!preferences.begin("time-cube", false)) {
    Serial.println("Preferences begin() failed! (write mode)");
    return false;
  }

  size_t n1 = preferences.putString("network", systemConfiguration.wifiNetwork);
  size_t n2 = preferences.putString("password", systemConfiguration.wifiPassword);
  size_t n3 = preferences.putInt("settle-time", systemConfiguration.settleTime);
  size_t n4 = preferences.putString("endpoint-url", systemConfiguration.endpointBaseUrl);
  size_t n5 = preferences.putString("endpoint-token", systemConfiguration.endpointToken);

  preferences.end();

  bool ok = (n1 > 0 || systemConfiguration.wifiNetwork.length() == 0) &&
            (n2 > 0 || systemConfiguration.wifiPassword.length() == 0) &&
            (n3 == sizeof(int32_t)) &&
            (n4 > 0 || systemConfiguration.endpointBaseUrl.length() == 0) &&
            (n5 > 0 || systemConfiguration.endpointToken.length() == 0);

  return ok;
}

