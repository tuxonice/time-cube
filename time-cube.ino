#include <WiFi.h>
#include <Wire.h>
#include <WebServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

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

//-------------- MPU ---------------------
static const uint8_t MPU_ADDR = 0x68;

// Raw sensor values
int16_t acc_x, acc_y, acc_z;
int16_t temp_raw;
int16_t gyro_x, gyro_y, gyro_z;

// Gyro calibration offsets
long gyro_x_offset = 0;
long gyro_y_offset = 0;
long gyro_z_offset = 0;

// Face detection state
int candidateFace = -1;
int stableFace = -1;
unsigned long candidateSinceMs = 0;

// Tuning
const float DOMINANCE_RATIO = 1.25f;          // higher = stricter face selection
const int GYRO_STABLE_THRESHOLD = 300;        // lower = stricter stillness
const unsigned long FACE_STABLE_MS = 350;     // time face must remain stable
const unsigned long PRINT_INTERVAL_MS = 250;  // periodic status print

unsigned long lastPrintMs = 0;

//--------------- END MPU ----------

Preferences preferences;
WebServer webServer(80);


// Initialises an HTTPClient for the given path, handling HTTP and HTTPS.
// The caller must call http.end() after use.
// Returns false if no base URL is configured.
bool httpBegin(HTTPClient& http, WiFiClientSecure& secureClient, const String& path)
{
  if (systemConfiguration.endpointBaseUrl == "") return false;

  String url = systemConfiguration.endpointBaseUrl + path;
  secureClient.setInsecure(); // skips certificate validation
  http.begin(secureClient, url);

  if (systemConfiguration.endpointToken != "") {
    http.addHeader("X-Time-Cube-Token", systemConfiguration.endpointToken);
  }
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
  
  //------------- MPU ------------------
  Wire.begin();

  setupMPU();
  calibrateGyro();

  Serial.println();
  Serial.println("MPU-6050 cube face detection started");
  Serial.println("Using accelerometer for face, gyroscope for motion filtering");
  //------------ END MPU ---------------
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

  delay(100);
  
  //-------------- MPU -----------------
  readMPU();

  bool stableMotion = isCubeStable();
  int detectedFace = -1;

  if (stableMotion) {
    detectedFace = getFaceFromAccel();
  }

  updateStableFace(detectedFace, stableMotion);
  printStatus(stableMotion, detectedFace);

  delay(500);
  
  //------------ END MPU ------------
  
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

//-------------- MPU -----------------
void setupMPU() {
  // Wake up MPU-6050
  writeRegister(0x6B, 0x00);

  // Accelerometer config: +/- 8g
  writeRegister(0x1C, 0x10);

  // Gyro config: 500 dps
  writeRegister(0x1B, 0x08);

  // Optional: low pass filter
  writeRegister(0x1A, 0x03);
}

void writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

void calibrateGyro() {
  const int samples = 1000;

  Serial.println("Calibrating gyro... keep cube still");

  long sumX = 0;
  long sumY = 0;
  long sumZ = 0;

  for (int i = 0; i < samples; i++) {
    readMPU();
    sumX += gyro_x;
    sumY += gyro_y;
    sumZ += gyro_z;
    delay(3);
  }

  gyro_x_offset = sumX / samples;
  gyro_y_offset = sumY / samples;
  gyro_z_offset = sumZ / samples;

  Serial.print("Gyro offsets: ");
  Serial.print(gyro_x_offset);
  Serial.print(", ");
  Serial.print(gyro_y_offset);
  Serial.print(", ");
  Serial.println(gyro_z_offset);
}

void readMPU() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);

  Wire.requestFrom(MPU_ADDR, (uint8_t)14);
  while (Wire.available() < 14) {}

  acc_x    = (Wire.read() << 8) | Wire.read();
  acc_y    = (Wire.read() << 8) | Wire.read();
  acc_z    = (Wire.read() << 8) | Wire.read();
  temp_raw = (Wire.read() << 8) | Wire.read();
  gyro_x   = (Wire.read() << 8) | Wire.read();
  gyro_y   = (Wire.read() << 8) | Wire.read();
  gyro_z   = (Wire.read() << 8) | Wire.read();

  gyro_x -= gyro_x_offset;
  gyro_y -= gyro_y_offset;
  gyro_z -= gyro_z_offset;
}

bool isCubeStable() {
  return abs(gyro_x) < GYRO_STABLE_THRESHOLD &&
         abs(gyro_y) < GYRO_STABLE_THRESHOLD &&
         abs(gyro_z) < GYRO_STABLE_THRESHOLD;
}

int getFaceFromAccel() {
  long ax = acc_x;
  long ay = acc_y;
  long az = acc_z;

  long absX = abs(ax);
  long absY = abs(ay);
  long absZ = abs(az);

  // Reject weak / ambiguous readings
  if (absX < 1500 && absY < 1500 && absZ < 1500) {
    return -1;
  }

  if (absX > absY * DOMINANCE_RATIO && absX > absZ * DOMINANCE_RATIO) {
    return (ax > 0) ? 0 : 1;
  }

  if (absY > absX * DOMINANCE_RATIO && absY > absZ * DOMINANCE_RATIO) {
    return (ay > 0) ? 2 : 3;
  }

  if (absZ > absX * DOMINANCE_RATIO && absZ > absY * DOMINANCE_RATIO) {
    return (az > 0) ? 4 : 5;
  }

  return -1;
}

void updateStableFace(int detectedFace, bool stableMotion) {
  unsigned long now = millis();

  // If moving or no clear face, reset candidate timer
  if (!stableMotion || detectedFace == -1) {
    candidateFace = -1;
    candidateSinceMs = 0;
    return;
  }

  // New candidate face
  if (detectedFace != candidateFace) {
    candidateFace = detectedFace;
    candidateSinceMs = now;
    return;
  }

  // Same candidate long enough -> commit
  if (stableFace != candidateFace && (now - candidateSinceMs >= FACE_STABLE_MS)) {
    stableFace = candidateFace;

    Serial.print("CONFIRMED FACE UP: ");
    Serial.print(faceName(stableFace));
    Serial.print(" | acc=(");
    Serial.print(acc_x);
    Serial.print(", ");
    Serial.print(acc_y);
    Serial.print(", ");
    Serial.print(acc_z);
    Serial.print(") gyro=(");
    Serial.print(gyro_x);
    Serial.print(", ");
    Serial.print(gyro_y);
    Serial.print(", ");
    Serial.print(gyro_z);
    Serial.println(")");
  }
}

void printStatus(bool stableMotion, int detectedFace) {
  unsigned long now = millis();
  if (now - lastPrintMs < PRINT_INTERVAL_MS) return;
  lastPrintMs = now;

  Serial.print("motion=");
  Serial.print(stableMotion ? "STILL" : "MOVING");

  Serial.print(" | detected=");
  Serial.print(faceName(detectedFace));

  Serial.print(" | stable=");
  Serial.print(faceName(stableFace));

  Serial.print(" | acc=(");
  Serial.print(acc_x);
  Serial.print(", ");
  Serial.print(acc_y);
  Serial.print(", ");
  Serial.print(acc_z);
  Serial.print(")");

  Serial.print(" | gyro=(");
  Serial.print(gyro_x);
  Serial.print(", ");
  Serial.print(gyro_y);
  Serial.print(", ");
  Serial.print(gyro_z);
  Serial.println(")");
}

const char* faceName(int face) {
  switch (face) {
    case 0: return "RED";
    case 1: return "ORANGE";
    case 2: return "WHITE";
    case 3: return "YELLOW";
    case 4: return "GREEN";
    case 5: return "BLUE";
    default: return "UNKNOWN";
  }
}

//-------------- END MPU ------------
