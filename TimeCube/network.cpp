#include "network.h"
#include "config.h"

bool wifiConnect(int timeoutMs) {
  unsigned long startTime = millis();

  Serial.print("Connecting to ");
  Serial.println(systemConfiguration.wifiNetwork);

  WiFi.mode(WIFI_STA);
  WiFi.begin(systemConfiguration.wifiNetwork.c_str(),
             systemConfiguration.wifiPassword.c_str());

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

    if (millis() - startTime > (unsigned long)timeoutMs) {
      WiFi.disconnect(true);
      Serial.println();
      Serial.println("WiFi connection timeout");
      return false;
    }
  }

  Serial.println();
  Serial.println("WiFi connected!");
  Serial.print("Got IP: ");
  Serial.println(WiFi.localIP());

  return true;
}

void apConnect() {
  IPAddress localIP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);

  Serial.println("Setting AP (Access Point)...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP("TIME-CUBE");
  delay(500);
  WiFi.softAPConfig(localIP, gateway, subnet);
  WiFi.persistent(false);

  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
}

bool httpBegin(HTTPClient& http, WiFiClientSecure& secureClient, const String& path) {
  if (systemConfiguration.endpointBaseUrl == "") {
    return false;
  }

  String url = systemConfiguration.endpointBaseUrl + path;

  secureClient.setInsecure();
  if (!http.begin(secureClient, url)) {
    return false;
  }

  if (systemConfiguration.endpointToken != "") {
    http.addHeader("X-Time-Cube-Token", systemConfiguration.endpointToken);
  }

  return true;
}

bool postToEndpoint(const String& path) {
  HTTPClient http;
  WiFiClientSecure secureClient;

  if (!httpBegin(http, secureClient, path)) {
    Serial.println("Endpoint not configured");
    return false;
  }

  int httpCode = http.POST("");
  http.end();

  Serial.print("POST ");
  Serial.print(path);
  Serial.print(" -> HTTP ");
  Serial.println(httpCode);

  return httpCode > 0 && httpCode < 300;
}

