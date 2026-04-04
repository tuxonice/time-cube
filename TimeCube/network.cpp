#include "network.h"
#include "config.h"

static const char* CUBE_ID = "cube-xyz123";

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

bool httpBegin(HTTPClient& http, WiFiClientSecure& secureClient, const char* faceName) {
  if (systemConfiguration.endpointBaseUrl == "") {
    return false;
  }

  String url = systemConfiguration.endpointBaseUrl;

  secureClient.setInsecure();
  if (!http.begin(secureClient, url)) {
    return false;
  }

  if (systemConfiguration.endpointToken != "") {
    http.addHeader("X-Time-Cube-Token", systemConfiguration.endpointToken);
  }

  return true;
}

bool postToEndpoint(const String& face) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return false;
  }

  if (systemConfiguration.endpointBaseUrl.isEmpty()) {
    Serial.println("Endpoint not configured");
    return false;
  }

  HTTPClient http;
  WiFiClientSecure secureClient;

  secureClient.setInsecure(); // ⚠️ skip cert validation

  if (!http.begin(secureClient, systemConfiguration.endpointBaseUrl)) {
    Serial.println("HTTP begin failed");
    return false;
  }

  http.setTimeout(3000);
  http.addHeader("Content-Type", "application/json");

  if (!systemConfiguration.endpointToken.isEmpty()) {
    http.addHeader("X-Time-Cube-Token", systemConfiguration.endpointToken);
  }

  // Build JSON payload
  String payload;
  payload.reserve(80);
  payload += F("{\"cubeId\":\"");
  payload += CUBE_ID;
  payload += F("\",\"face\":\"");
  payload += face;
  payload += F("\"}");

  int httpCode = http.POST(payload);

  // 👇 GET RESPONSE BODY
  String response = http.getString();

  Serial.print("POST ");
  Serial.print(systemConfiguration.endpointBaseUrl);
  Serial.print(" payload=");
  Serial.print(payload);
  Serial.print(" -> HTTP ");
  Serial.println(httpCode);

  if (httpCode <= 0) {
    Serial.print("Error: ");
    Serial.println(http.errorToString(httpCode));
  }

  // 👇 PRINT RESPONSE
  if (httpCode > 0) {
    Serial.print("Response: ");
    Serial.println(response);
  } else {
    Serial.print("Error: ");
    Serial.println(http.errorToString(httpCode));
  }

  http.end();

  return httpCode >= 200 && httpCode < 300;
}
