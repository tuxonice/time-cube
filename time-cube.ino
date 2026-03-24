#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "nvs_flash.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

String SendHTML(String alertMessage);

bool apMode;

struct Configuration {
   String wifiNetwork = "";
   String wifiPassword = "";
   int settleTime = 2000;
   String endpointBaseUrl = "";
   String endpointToken = "";
};

struct Configuration systemConfiguration;

Preferences preferences;
WebServer webServer(80);

Adafruit_MPU6050 mpu;

// Face index → color mapping (adjust order to match physical cube orientation)
const char* faceColors[6] = { "blue", "yellow", "red", "green", "orange", "white" };

// Task IDs loaded from backend, indexed by face. -1 means not mapped.
int faceTasks[6] = { -1, -1, -1, -1, -1, -1 };

int8_t currentFace = -1;
int8_t previousFace = -1;
int8_t pendingFace = -1;
unsigned long faceChangedAt = 0;

void mpu6050Init()
{
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    return;
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  Serial.println("MPU6050 initialized");
}

// Returns face index 0-5, or -1 if read fails.
// Face mapping based on which axis has dominant gravity vector:
//   0 (blue)   : +Z up
//   1 (yellow) : -Z up
//   2 (red)    : +X up
//   3 (green)  : -X up
//   4 (orange) : +Y up
//   5 (white)  : -Y up
int8_t readFace()
{
  sensors_event_t a, g, temp;
  if (!mpu.getEvent(&a, &g, &temp)) return -1;

  float ax = a.acceleration.x;
  float ay = a.acceleration.y;
  float az = a.acceleration.z;

  float absX = fabsf(ax);
  float absY = fabsf(ay);
  float absZ = fabsf(az);

  if (absZ >= absX && absZ >= absY) return (az > 0) ? 0 : 1;
  if (absX >= absY)                  return (ax > 0) ? 2 : 3;
  return                                    (ay > 0) ? 4 : 5;
}

bool isHttps()
{
  return systemConfiguration.endpointBaseUrl.startsWith("https://");
}

// Initialises an HTTPClient for the given path, handling HTTP and HTTPS.
// The caller must call http.end() after use.
// Returns false if no base URL is configured.
bool httpBegin(HTTPClient& http, WiFiClientSecure& secureClient, const String& path)
{
  if (systemConfiguration.endpointBaseUrl == "") return false;

  String url = systemConfiguration.endpointBaseUrl + path;
  if (isHttps()) {
    secureClient.setInsecure(); // skips certificate validation
    http.begin(secureClient, url);
  } else {
    http.begin(url);
  }

  if (systemConfiguration.endpointToken != "") {
    http.addHeader("X-Time-Cube-Token", systemConfiguration.endpointToken);
  }
  return true;
}

bool loadCubeConfig()
{
  HTTPClient http;
  WiFiClientSecure secureClient;

  if (!httpBegin(http, secureClient, "/cube-config")) {
    Serial.println("No endpoint URL configured, skipping cube config load");
    return false;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("loadCubeConfig failed, HTTP %d\n", httpCode);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.print("loadCubeConfig JSON parse error: ");
    Serial.println(err.c_str());
    return false;
  }

  JsonObject faces = doc["faces"];
  for (int i = 0; i < 6; i++) {
    if (faces[faceColors[i]].is<JsonObject>()) {
      faceTasks[i] = faces[faceColors[i]]["task_id"];
      Serial.printf("  %s → task_id %d\n", faceColors[i], faceTasks[i]);
    }
  }

  Serial.println("Cube config loaded");
  return true;
}

bool postToEndpoint(String path)
{
  HTTPClient http;
  WiFiClientSecure secureClient;

  if (!httpBegin(http, secureClient, path)) return false;

  int httpCode = http.POST("");
  http.end();
  return httpCode == HTTP_CODE_OK;
}

void onFaceChanged(int8_t face)
{
  Serial.printf("Active face: %s (task_id: %d)\n", faceColors[face], faceTasks[face]);

  if (previousFace >= 0 && faceTasks[previousFace] >= 0) {
    String path = "/stop/" + String(faceTasks[previousFace]);
    Serial.printf("Stopping task_id %d\n", faceTasks[previousFace]);
    if (!postToEndpoint(path)) {
      Serial.println("Failed to stop previous task");
    }
  }

  if (faceTasks[face] >= 0) {
    String path = "/start/" + String(faceTasks[face]);
    Serial.printf("Starting task_id %d\n", faceTasks[face]);
    if (!postToEndpoint(path)) {
      Serial.println("Failed to start new task");
    }
  }

  previousFace = face;
}

void setup()
{
  Serial.begin(115200);
  apMode = false;

  // 1. Read config
  Serial.println("Reading configuration");
  readConfig();
  

  // 3. Connect to wifi STA or AP mode
  if(systemConfiguration.wifiNetwork == "" || systemConfiguration.wifiPassword == "" || !wifiConnect(15000)) {
      apConnect();
      apMode = true;
  }

  webServer.on("/", HTTP_GET, handle_OnConnect);
  webServer.on("/", HTTP_POST, handle_Update);
  webServer.onNotFound(handle_NotFound);

  webServer.begin();
  Serial.println("HTTP server started");

  if (!apMode) {
    mpu6050Init();
    loadCubeConfig();
  }
}

bool wifiConnect(int timeout)
{
    unsigned long startTime = millis();
    Serial.print("Connecting to ");
    Serial.println(systemConfiguration.wifiNetwork.c_str());
    WiFi.begin(systemConfiguration.wifiNetwork.c_str(), systemConfiguration.wifiPassword.c_str());
    while(WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.print(".");
      if((millis() - startTime) > timeout) {
        WiFi.disconnect();
        return false;
      }
    }
    Serial.println("WiFi connected..!");
    Serial.print("Got IP: ");
    Serial.println(WiFi.localIP());
    return true;
}

void apConnect()
{
    IPAddress localIP(192,168,4,1);
    IPAddress gateway(192,168,4,1);
    IPAddress subnet(255,255,255,0);
    Serial.println("Setting AP (Access Point)...");
    WiFi.softAP("TIME-CUBE");
    delay(2000); // VERY IMPORTANT
    WiFi.softAPConfig(localIP, gateway, subnet);
    WiFi.persistent(false);
    delay(1000);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);
}

void loop()
{
  webServer.handleClient();

  if (apMode) {
    delay(500);
    return;
  }

  int8_t rawFace = readFace();
  if (rawFace < 0) {
    delay(100);
    return;
  }

  // Reset settle timer whenever the raw reading changes
  if (rawFace != pendingFace) {
    pendingFace = rawFace;
    faceChangedAt = millis();
  }

  // Confirm face change only after it has been stable for settleTime ms
  if (pendingFace != currentFace && (millis() - faceChangedAt) >= (unsigned long)systemConfiguration.settleTime) {
    currentFace = pendingFace;
    onFaceChanged(currentFace);
  }

  delay(100);
}

void handle_OnConnect()
{
  webServer.send(200, "text/html", SendHTML(""));
}

void handle_NotFound()
{
  webServer.send(404, "text/plain", "Not found");
}

void handle_Update()
{
  if (webServer.hasArg("wifi-network")) {
      systemConfiguration.wifiNetwork = webServer.arg("wifi-network");
  }

  if (webServer.hasArg("wifi-password") && webServer.arg("wifi-password") != "**********") {
      systemConfiguration.wifiPassword = webServer.arg("wifi-password");
  }

  if (webServer.hasArg("settle-time")) {
    systemConfiguration.settleTime = webServer.arg("settle-time").toInt();
  }

  if (webServer.hasArg("endpoint-base-url")) {
    systemConfiguration.endpointBaseUrl = webServer.arg("endpoint-base-url");
  }

  if (webServer.hasArg("endpoint-token") && webServer.arg("endpoint-token") != "**********") {
    systemConfiguration.endpointToken = webServer.arg("endpoint-token");
  }

  bool configFileSaved = saveConfig();
  String alertMessage = "";
  if(configFileSaved) {
    alertMessage = getAlertMessageHtml("success", "Configuration saved!");
  } else {
    alertMessage = getAlertMessageHtml("danger", "Configuration not saved!");
  }
  webServer.send(200, "text/html", SendHTML(alertMessage));
}

void readConfig()
{
  if (!preferences.begin("time-cube", true)) {
    Serial.println("Failed to open Preferences (read mode)!");
    return;
  }
  
  systemConfiguration.wifiNetwork = preferences.getString("network", "");
  systemConfiguration.wifiPassword = preferences.getString("password", "");
  systemConfiguration.settleTime = preferences.getInt("settle-time", 4000);
  systemConfiguration.endpointBaseUrl = preferences.getString("endpoint-url", "");
  systemConfiguration.endpointToken = preferences.getString("endpoint-token", "");
  preferences.end();
}

bool saveConfig()
{
  if (!preferences.begin("time-cube", false))
  {
    Serial.println("Preferences begin() failed! (write mode)");
    return false;
  }

  size_t n1 = preferences.putString("network", systemConfiguration.wifiNetwork);
  size_t n2 = preferences.putString("password", systemConfiguration.wifiPassword);
  size_t n3 = preferences.putInt("settle-time", systemConfiguration.settleTime);
  size_t n4 = preferences.putString("endpoint-url", systemConfiguration.endpointBaseUrl);
  size_t n5 = preferences.putString("endpoint-token", systemConfiguration.endpointToken);

  preferences.end();

  bool ok = (n3 == sizeof(int32_t)) && (n4 > 0 || systemConfiguration.endpointBaseUrl.length() == 0) && (n5 > 0 || systemConfiguration.endpointToken.length() == 0);

  return ok;
}

String SendHTML(String alertMessage)
{
    String ptr = String("<!doctype html>\n");
    ptr += String("<html lang=\"en\"><head><meta charset=\"utf-8\">\n");
    ptr += String("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, shrink-to-fit=no\"><title>Time Cube</title>\n");
    ptr += String("<style>body{margin:0;font-family:Roboto,Arial,sans-serif;line-height:1.5;color:#616264;background:#e5e5e5}input[type=text],input[type=number],input[type=password]{width:100%;padding:12px 20px;margin:8px 0;border:1px solid #ccc;border-radius:4px;box-sizing:border-box}input[type=submit]{width:100%;background:#4caf50;color:#fff;padding:14px 20px;margin:8px 0;border:none;border-radius:4px;cursor:pointer;font-size:1.2rem}input[type=submit]:hover{background:#45a049}.container{max-width:960px;padding:0 15px;margin:0 auto}.section-header{text-align:center;padding:1.5rem 0}h2{font-size:1.5rem;margin:0 0 1rem;font-weight:500}hr{margin:1rem 0;border:0;border-top:1px solid rgba(0,0,0,.1)}.alert{padding:20px;color:#fff;margin-bottom:15px;border-radius:4px;background:#999}.alert.danger{background:#f44336}.alert.success{background:#4caf50}.closebtn{margin-left:15px;color:#fff;font-weight:700;float:right;font-size:22px;line-height:20px;cursor:pointer;transition:.3s}.closebtn:hover{color:#000}footer{color:#6c757d;text-align:center;padding-top:1rem;margin:1.5rem 0 1rem}</style>\n");
    ptr += String("</head><body><div class=\"container\"><div class=\"section-header\"><h1>Time Cube Configuration</h1></div>\n");
    ptr += alertMessage;
    ptr += String("<form action=\"/\" method=\"POST\">\n");
    ptr += String("<h2>Wifi</h2> <label>Network</label> <input type=\"text\" name=\"wifi-network\" value=\"" + systemConfiguration.wifiNetwork + "\" required /> <label>Password</label><input type=\"password\" name=\"wifi-password\" value=\"**********\" required /><hr/><h2>Configuration</h2><label>Settle Time (ms)</label><input type=\"number\" name=\"settle-time\" value=\"" + String(systemConfiguration.settleTime) + "\" required /><label>Endpoint Base URL</label><input type=\"text\" name=\"endpoint-base-url\" value=\"" + systemConfiguration.endpointBaseUrl + "\" /><label>Endpoint Security Token</label><input type=\"password\" name=\"endpoint-token\" value=\"**********\" />\n");
    ptr += String("<input type=\"submit\" value=\"Save\"></form>\n");
    ptr += String("<footer><p>&copy; 2026 TLab</p> </footer></div></body></html>");

    return ptr;
}

String getAlertMessageHtml(String type, String message)
{
  return String("<div class=\"alert " + type + "\"><span class=\"closebtn\" onclick=\"this.parentElement.style.display='none';\">&times;</span>" + message + "</div>");
}
