#include "webui.h"
#include "config.h"

WebServer webServer(80);

static void sendEscapedHtml(const String& input);
static String htmlEscape(const String& input);

void setupWebServer() {
  webServer.on("/", HTTP_GET, handle_OnConnect);
  webServer.on("/", HTTP_POST, handle_Update);
  webServer.onNotFound(handle_NotFound);
  webServer.begin();
}

void handle_OnConnect() {
  sendPage("");
}

void handle_NotFound() {
  webServer.send(404, "text/plain", "Not found");
}

void handle_Update() {
  if (webServer.hasArg("wifi-network")) {
    systemConfiguration.wifiNetwork = webServer.arg("wifi-network");
  }

  if (webServer.hasArg("wifi-password") &&
      webServer.arg("wifi-password") != "**********") {
    systemConfiguration.wifiPassword = webServer.arg("wifi-password");
  }

  if (webServer.hasArg("settle-time")) {
    systemConfiguration.settleTime = webServer.arg("settle-time").toInt();
    if (systemConfiguration.settleTime < 100) {
      systemConfiguration.settleTime = 100;
    }
  }

  if (webServer.hasArg("endpoint-base-url")) {
    systemConfiguration.endpointBaseUrl = webServer.arg("endpoint-base-url");
  }

  if (webServer.hasArg("endpoint-token") &&
      webServer.arg("endpoint-token") != "**********") {
    systemConfiguration.endpointToken = webServer.arg("endpoint-token");
  }

  bool ok = saveConfig();

  String alert;
  if (ok) {
    alert = getAlertMessageHtml("success", "Configuration saved!");
  } else {
    alert = getAlertMessageHtml("danger", "Configuration not saved!");
  }

  sendPage(alert);
}

void sendPage(const String& alertMessage)
{
  webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  webServer.send(200, "text/html", "");

  auto send = [&](const __FlashStringHelper* chunk) {
    webServer.sendContent(chunk);
    delay(0); // allow background tasks (WiFi stack)
  };

  auto sendVar = [&](const String& value) {
    webServer.sendContent(value);
  };

  send(F("<!doctype html><html><head>"));
  send(F("<meta charset='utf-8'>"));
  send(F("<meta name='viewport' content='width=device-width,initial-scale=1'>"));
  send(F("<title>Time Cube</title>"));

  // --- CSS ---
  send(F("<style>"));
  send(F(R"rawliteral(
    body{margin:0;font-family:Arial;background:#f3f4f6;color:#111}
    .wrap{max-width:720px;margin:auto;padding:20px}
    .card{background:#fff;border-radius:16px;padding:20px;box-shadow:0 10px 25px rgba(0,0,0,.08)}
    h1{text-align:center}
    .field{margin-bottom:14px}
    label{display:block;font-weight:bold;margin-bottom:4px}
    input{width:100%;padding:10px;border-radius:10px;border:1px solid #ccc}
    button{width:100%;padding:14px;border:none;border-radius:12px;background:#2563eb;color:#fff;font-weight:bold}
    .alert{padding:10px;border-radius:10px;margin-bottom:10px;color:#fff}
    .success{background:#16a34a}
    .danger{background:#dc2626}
  )rawliteral"));
  send(F("</style></head><body>"));

  send(F("<div class='wrap'>"));
  send(F("<h1>Time Cube</h1>"));
  send(F("<div class='card'>"));

  // alert
  if (alertMessage.length()) {
    sendVar(alertMessage);
  }

  send(F("<form method='POST'>"));

  // WiFi
  send(F("<div class='field'><label>Network</label><input name='wifi-network' value='"));
  sendEscapedHtml(systemConfiguration.wifiNetwork);
  send(F("'></div>"));

  send(F("<div class='field'><label>Password</label><input type='password' name='wifi-password' value='**********'></div>"));

  // Settings
  send(F("<div class='field'><label>Settle Time</label><input type='number' name='settle-time' value='"));
  sendVar(String(systemConfiguration.settleTime));
  send(F("'></div>"));

  send(F("<div class='field'><label>Endpoint URL</label><input name='endpoint-base-url' value='"));
  sendEscapedHtml(systemConfiguration.endpointBaseUrl);
  send(F("'></div>"));

  send(F("<div class='field'><label>Endpoint Token</label><input type='password' name='endpoint-token' value='**********'></div>"));

  send(F("<button type='submit'>Save</button>"));
  send(F("</form></div></div></body></html>"));

  //webServer.client().stop();
  webServer.sendContent("");
}

String getAlertMessageHtml(const String& type, const String& message) {
  return "<div class=\"alert " + htmlEscape(type) + "\">"
         "<span class=\"closebtn\" onclick=\"this.parentElement.style.display='none';\">&times;</span>" +
         htmlEscape(message) +
         "</div>";
}


static void sendEscapedHtml(const String& input) {
  for (size_t i = 0; i < input.length(); i++) {
    char c = input[i];
    switch (c) {
      case '&':
        webServer.sendContent(F("&amp;"));
        break;
      case '<':
        webServer.sendContent(F("&lt;"));
        break;
      case '>':
        webServer.sendContent(F("&gt;"));
        break;
      case '"':
        webServer.sendContent(F("&quot;"));
        break;
      case '\'':
        webServer.sendContent(F("&#39;"));
        break;
      default: {
        char buf[2] = { c, 0 };
        webServer.sendContent(buf);
        break;
      }
    }
  }
}

static String htmlEscape(const String& input) {
  String out;
  out.reserve(input.length() + 16);

  for (size_t i = 0; i < input.length(); i++) {
    const char c = input[i];
    switch (c) {
      case '&': out += F("&amp;");  break;
      case '<': out += F("&lt;");   break;
      case '>': out += F("&gt;");   break;
      case '"': out += F("&quot;"); break;
      case '\'': out += F("&#39;"); break;
      default: out += c;            break;
    }
  }

  return out;
}
