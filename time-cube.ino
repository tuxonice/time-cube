#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

String SendHTML(String alertMessage);

bool apMode;

struct Configuration {
   String wifiNetwork = "";
   String wifiPassword = "";
   int settleTime = 2000;
};

struct Configuration systemConfiguration;

Preferences preferences;
WebServer webServer(80);

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
    WiFi.softAP("ESP32-PIXEL");
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
  if(apMode == true) {
      delay(500);
  }
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
  preferences.begin("time-cube-config", true);
  systemConfiguration.wifiNetwork = preferences.getString("network", "");
  systemConfiguration.wifiPassword = preferences.getString("password", "");
  systemConfiguration.settleTime = preferences.getString("settle-time", "");
  preferences.end();
}

bool saveConfig()
{
  preferences.begin("time-cube-config", false);
  bool ok = preferences.putString("network", systemConfiguration.wifiNetwork) &&
            preferences.putString("password", systemConfiguration.wifiPassword) &&
            preferences.putString("settle-time", systemConfiguration.settleTime);
  preferences.end();
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
    ptr += String("<h2>Wifi</h2> <label>Network</label> <input type=\"text\" name=\"wifi-network\" value=\"" + systemConfiguration.wifiNetwork + "\" required /> <label>Password</label><input type=\"password\" name=\"wifi-password\" value=\"**********\" required /><hr/><h2>Configuration</h2><label>Settle Time (ms)</label><input type=\"number\" name=\"settle-time\" value=\"\" required />\n");
    ptr += String("<input type=\"submit\" value=\"Save\"></form>\n");
    ptr += String("<footer><p>&copy; 2026 TLab</p> </footer></div></body></html>");

    return ptr;
}

String getAlertMessageHtml(String type, String message)
{
  return String("<div class=\"alert " + type + "\"><span class=\"closebtn\" onclick=\"this.parentElement.style.display='none';\">&times;</span>" + message + "</div>");
}
