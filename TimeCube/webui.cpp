#include "webui.h"
#include "config.h"

WebServer webServer(80);

static String htmlEscape(const String& input);
static String renderTemplate(const String& page);
static String maskIfSet(const String& value);

static const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Time Cube</title>
  <style>
    :root{
      --bg:#f3f4f6;
      --card:#ffffff;
      --text:#1f2937;
      --muted:#6b7280;
      --border:#d1d5db;
      --primary:#2563eb;
      --primary-hover:#1d4ed8;
      --success:#16a34a;
      --danger:#dc2626;
      --shadow:0 10px 25px rgba(0,0,0,.08);
      --radius:16px;
    }

    *{box-sizing:border-box}

    body{
      margin:0;
      font-family:Arial,Helvetica,sans-serif;
      background:linear-gradient(180deg,#eef2ff 0%, #f3f4f6 140px, #f3f4f6 100%);
      color:var(--text);
    }

    .wrap{
      max-width:720px;
      margin:0 auto;
      padding:24px 16px 40px;
    }

    .hero{
      text-align:center;
      margin-bottom:20px;
    }

    .hero h1{
      margin:0;
      font-size:2rem;
      font-weight:700;
      letter-spacing:.3px;
    }

    .hero p{
      margin:8px 0 0;
      color:var(--muted);
      font-size:.98rem;
    }

    .card{
      background:var(--card);
      border-radius:var(--radius);
      box-shadow:var(--shadow);
      padding:20px;
    }

    .section{
      margin-bottom:22px;
    }

    .section:last-child{
      margin-bottom:0;
    }

    .section-title{
      font-size:1.05rem;
      font-weight:700;
      margin:0 0 14px;
      padding-bottom:8px;
      border-bottom:1px solid #e5e7eb;
    }

    .field{
      margin-bottom:14px;
    }

    .field:last-child{
      margin-bottom:0;
    }

    label{
      display:block;
      margin-bottom:6px;
      font-size:.92rem;
      font-weight:600;
    }

    .hint{
      display:block;
      margin-top:6px;
      color:var(--muted);
      font-size:.82rem;
    }

    input[type=text],
    input[type=password],
    input[type=number]{
      width:100%;
      border:1px solid var(--border);
      border-radius:12px;
      padding:12px 14px;
      font-size:1rem;
      outline:none;
      background:#fff;
      transition:border-color .15s ease, box-shadow .15s ease;
    }

    input[type=text]:focus,
    input[type=password]:focus,
    input[type=number]:focus{
      border-color:var(--primary);
      box-shadow:0 0 0 3px rgba(37,99,235,.15);
    }

    .grid{
      display:grid;
      grid-template-columns:1fr;
      gap:14px;
    }

    .actions{
      margin-top:24px;
    }

    .btn{
      width:100%;
      border:none;
      border-radius:12px;
      background:var(--primary);
      color:#fff;
      padding:14px 18px;
      font-size:1rem;
      font-weight:700;
      cursor:pointer;
      transition:background .15s ease, transform .04s ease;
    }

    .btn:hover{
      background:var(--primary-hover);
    }

    .btn:active{
      transform:translateY(1px);
    }

    .alert{
      position:relative;
      border-radius:12px;
      padding:14px 16px;
      margin-bottom:16px;
      color:#fff;
      font-size:.95rem;
      font-weight:600;
    }

    .alert.success{ background:var(--success); }
    .alert.danger{ background:var(--danger); }

    .closebtn{
      float:right;
      margin-left:12px;
      color:#fff;
      font-weight:700;
      font-size:20px;
      line-height:20px;
      cursor:pointer;
    }

    .footer{
      text-align:center;
      color:var(--muted);
      font-size:.85rem;
      margin-top:18px;
    }

    @media (min-width: 640px){
      .grid.two{
        grid-template-columns:1fr 1fr;
      }

      .card{
        padding:24px;
      }

      .hero h1{
        font-size:2.2rem;
      }
    }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="hero">
      <h1>Time Cube</h1>
      <p>Configure Wi-Fi, detection timing, and endpoint settings.</p>
    </div>

    <div class="card">
      {{ALERT}}

      <form action="/" method="POST">
        <div class="section">
          <h2 class="section-title">Wi-Fi</h2>
          <div class="grid">
            <div class="field">
              <label for="wifi-network">Network</label>
              <input id="wifi-network" type="text" name="wifi-network" value="{{WIFI_NETWORK}}" required>
              <span class="hint">Name of the Wi-Fi network the cube should connect to.</span>
            </div>

            <div class="field">
              <label for="wifi-password">Password</label>
              <input id="wifi-password" type="password" name="wifi-password" value="{{WIFI_PASSWORD}}" required>
              <span class="hint">Leave unchanged to keep the current password.</span>
            </div>
          </div>
        </div>

        <div class="section">
          <h2 class="section-title">Cube Settings</h2>
          <div class="grid two">
            <div class="field">
              <label for="settle-time">Settle Time (ms)</label>
              <input id="settle-time" type="number" min="100" step="50" name="settle-time" value="{{SETTLE_TIME}}" required>
              <span class="hint">How long a face must remain stable before it is accepted.</span>
            </div>

            <div class="field">
              <label for="endpoint-token">Endpoint Token</label>
              <input id="endpoint-token" type="password" name="endpoint-token" value="{{ENDPOINT_TOKEN}}">
              <span class="hint">Optional security token sent to your server.</span>
            </div>
          </div>

          <div class="field">
            <label for="endpoint-base-url">Endpoint Base URL</label>
            <input id="endpoint-base-url" type="text" name="endpoint-base-url" placeholder="https://example.com/api" value="{{ENDPOINT_BASE_URL}}">
            <span class="hint">Base URL used when posting cube face changes.</span>
          </div>
        </div>

        <div class="actions">
          <button class="btn" type="submit">Save Configuration</button>
        </div>
      </form>
    </div>

    <div class="footer">&copy; 2026 TLab</div>
  </div>
</body>
</html>
)rawliteral";

void setupWebServer() {
  webServer.on("/", HTTP_GET, handle_OnConnect);
  webServer.on("/", HTTP_POST, handle_Update);
  webServer.onNotFound(handle_NotFound);
  webServer.begin();
}

void handle_OnConnect() {
  webServer.send(200, "text/html", SendHTML(""));
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

  const bool ok = saveConfig();

  String alertMessage;
  if (ok) {
    alertMessage = getAlertMessageHtml("success", "Configuration saved!");
  } else {
    alertMessage = getAlertMessageHtml("danger", "Configuration not saved!");
  }

  webServer.send(200, "text/html", SendHTML(alertMessage));
}

String SendHTML(const String& alertMessage) {
  String page = FPSTR(PAGE_HTML);
  page = renderTemplate(page);
  page.replace("{{ALERT}}", alertMessage);
  return page;
}

String getAlertMessageHtml(const String& type, const String& message) {
  return "<div class=\"alert " + htmlEscape(type) + "\">"
         "<span class=\"closebtn\" onclick=\"this.parentElement.style.display='none';\">&times;</span>" +
         htmlEscape(message) +
         "</div>";
}

static String renderTemplate(const String& pageTemplate) {
  String page = pageTemplate;

  page.replace("{{WIFI_NETWORK}}", htmlEscape(systemConfiguration.wifiNetwork));
  page.replace("{{WIFI_PASSWORD}}", maskIfSet(systemConfiguration.wifiPassword));
  page.replace("{{SETTLE_TIME}}", String(systemConfiguration.settleTime));
  page.replace("{{ENDPOINT_BASE_URL}}", htmlEscape(systemConfiguration.endpointBaseUrl));
  page.replace("{{ENDPOINT_TOKEN}}", maskIfSet(systemConfiguration.endpointToken));

  return page;
}

static String maskIfSet(const String& value) {
  return value.length() ? "**********" : "";
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
