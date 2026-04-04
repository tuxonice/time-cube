#ifndef NETWORK_H
#define NETWORK_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

bool wifiConnect(int timeoutMs);
void apConnect();

bool httpBegin(HTTPClient& http, WiFiClientSecure& secureClient, const String& path);
bool postToEndpoint(const String& path);

#endif

