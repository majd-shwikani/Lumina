#include "web_config_portal.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include "globals.h"

WebServer server(80);

const char* ap_ssid = "Lumina";
const char* ap_password = "";

const char* htmlContent = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Lumina - Device Configuration</title>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css">
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-body: #050505;
            --bg-card: #121212;
            --bg-input: #2a2a2a;
            --primary: #3b82f6;
            --primary-glow: rgba(59, 130, 246, 0.4);
            --text-main: #ffffff;
            --text-secondary: #a3a3a3;
            --border-light: rgba(255, 255, 255, 0.1);
            --radius-lg: 24px;
            --radius-md: 16px;
            --transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
        }

        * { margin: 0; padding: 0; box-sizing: border-box; font-family: 'Inter', sans-serif; }

        body {
            background-color: var(--bg-body);
            color: var(--text-main);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            padding: 40px 20px;
        }

        .container { width: 100%; max-width: 600px; z-index: 1; }

        header {
            display: flex;
            flex-direction: column;
            align-items: center;
            margin-bottom: 40px;
            text-align: center;
        }

        .logo-svg {
            width: 80px;
            height: 80px;
            margin-bottom: 20px;
            filter: drop-shadow(0 0 10px var(--primary-glow));
        }

        h1 {
            font-size: 2.5rem;
            font-weight: 700;
            background: linear-gradient(135deg, #fff 0%, #a5b4fc 100%);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            margin-bottom: 5px;
            letter-spacing: -1px;
        }

        .subtitle { color: var(--text-secondary); font-size: 1rem; }

        .card {
            background: rgba(18, 18, 18, 0.6);
            backdrop-filter: blur(20px);
            border: 1px solid var(--border-light);
            border-radius: var(--radius-lg);
            padding: 30px;
            margin-bottom: 24px;
            transition: var(--transition);
        }

        .card-header {
            display: flex;
            align-items: center;
            gap: 12px;
            margin-bottom: 24px;
        }

        .card-header i {
            color: var(--primary);
            font-size: 1.2rem;
            background: rgba(59, 130, 246, 0.1);
            padding: 10px;
            border-radius: 12px;
        }

        .card-header h2 { font-size: 1.25rem; font-weight: 600; }

        .form-group { margin-bottom: 20px; }

        label {
            display: block;
            margin-bottom: 8px;
            color: var(--text-secondary);
            font-weight: 500;
            font-size: 0.9rem;
        }

        /* WiFi Scan Section */
        .scan-controls {
            display: flex;
            gap: 10px;
            margin-bottom: 15px;
        }

        .btn-secondary {
            padding: 10px 20px;
            background: var(--bg-input);
            color: var(--text-main);
            border: 1px solid var(--border-light);
            border-radius: var(--radius-md);
            cursor: pointer;
            font-weight: 500;
            transition: var(--transition);
            display: flex;
            align-items: center;
            gap: 8px;
        }

        .btn-secondary:hover {
            background: #333;
            border-color: var(--primary);
        }

        .btn-secondary:active { transform: scale(0.98); }

        .ssid-selection { display: none; }
        .ssid-selection.active { display: block; }

        .ssid-list {
            max-height: 200px;
            overflow-y: auto;
            background: var(--bg-input);
            border-radius: var(--radius-md);
            margin-bottom: 15px;
        }

        .ssid-item {
            padding: 12px 16px;
            border-bottom: 1px solid rgba(255,255,255,0.05);
            cursor: pointer;
            transition: var(--transition);
            display: flex;
            justify-content: space-between;
            align-items: center;
        }

        .ssid-item:hover { background: rgba(59, 130, 246, 0.1); }

        .ssid-item.selected { background: rgba(59, 130, 246, 0.2); border-color: var(--primary); }

        .ssid-name { font-weight: 500; }
        .ssid-info { display: flex; align-items: center; gap: 12px; }
        .ssid-rssi {
            display: flex;
            align-items: center;
            gap: 4px;
            font-size: 0.85rem;
            color: var(--text-secondary);
        }

        .rssi-bar {
            width: 60px;
            height: 6px;
            background: #333;
            border-radius: 3px;
            overflow: hidden;
        }

        .rssi-fill {
            height: 100%;
            background: var(--primary);
            transition: width 0.3s ease;
        }

        .rssi-fill.weak { background: #f43f5e; }
        .rssi-fill.medium { background: #f59e0b; }
        .rssi-fill.strong { background: #10b981; }

        .password-wrapper { position: relative; }

        .toggle-password {
            position: absolute;
            right: 12px;
            top: 50%;
            transform: translateY(-50%);
            color: var(--text-secondary);
            cursor: pointer;
            font-size: 0.9rem;
        }

        input[type="text"],
        input[type="password"],
        input[type="number"] {
            width: 100%;
            padding: 14px 20px;
            background: var(--bg-input);
            border: 1px solid transparent;
            border-radius: var(--radius-md);
            color: var(--text-main);
            font-size: 1rem;
            outline: none;
            transition: var(--transition);
        }

        input:focus { border-color: var(--primary); box-shadow: 0 0 0 4px rgba(59, 130, 246, 0.1); }

        .control-row {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 10px 0;
        }

        .toggle { position: relative; width: 52px; height: 28px; }
        .toggle input { opacity: 0; width: 0; height: 0; }
        .slider {
            position: absolute; cursor: pointer;
            top: 0; left: 0; right: 0; bottom: 0;
            background-color: var(--bg-input);
            transition: .4s;
            border-radius: 34px;
        }
        .slider:before {
            position: absolute; content: "";
            height: 20px; width: 20px;
            left: 4px; bottom: 4px;
            background-color: white;
            transition: .4s;
            border-radius: 50%;
        }
        input:checked + .slider { background-color: var(--primary); }
        input:checked + .slider:before { transform: translateX(24px); }

        .btn-primary {
            width: 100%;
            padding: 16px;
            background: var(--primary);
            color: white;
            border: none;
            border-radius: var(--radius-md);
            font-weight: 600;
            font-size: 1.1rem;
            cursor: pointer;
            transition: var(--transition);
            display: flex;
            justify-content: center;
            align-items: center;
            gap: 10px;
            margin-top: 20px;
        }

        .btn-primary:hover {
            background: #2563eb;
            box-shadow: 0 4px 12px var(--primary-glow);
            transform: translateY(-2px);
        }

        .btn-primary:disabled { opacity: 0.6; cursor: not-allowed; transform: none; }

        .status {
            padding: 15px;
            border-radius: var(--radius-md);
            margin-top: 20px;
            font-weight: 500;
            text-align: center;
            display: none;
        }

        .status.success { background: rgba(16, 185, 129, 0.1); color: #10b981; border: 1px solid rgba(16, 185, 129, 0.2); }
        .status.error { background: rgba(244, 63, 94, 0.1); color: #f43f5e; border: 1px solid rgba(244, 63, 94, 0.2); }

        footer { text-align: center; margin-top: 40px; color: var(--text-secondary); font-size: 0.8rem; opacity: 0.6; }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>Lumina</h1>
            <span class="subtitle">System Configuration Portal</span>
            <span class="subtitle" id="versionDisplay" style="font-size: 0.8rem; opacity: 0.5; margin-top: 5px;"></span>
        </header>

        <form id="configForm">
            <!-- WiFi Section -->
            <div class="card">
                <div class="card-header">
                    <i class="fas fa-wifi"></i>
                    <h2>WiFi Network</h2>
                </div>

                <div id="scanControls" class="scan-controls">
                    <button type="button" class="btn-secondary" onclick="scanWiFi()">
                        <i class="fas fa-wifi"></i>
                        Scan WiFi
                    </button>
                    <span class="subtitle" id="scanStatus" style="font-size: 0.8rem; opacity: 0.6;"></span>
                </div>

                <div id="ssidSelection" class="ssid-selection">
                    <label>Available Networks</label>
                    <div class="ssid-list" id="ssidList"></div>
                    <input type="hidden" name="wifi_ssid" id="selectedSsid" required>
                </div>

                <div class="form-group">
                    <label>Password</label>
                    <div class="password-wrapper">
                        <input type="password" name="wifi_password" id="wifiPassword" required placeholder="Enter Password">
                        <i class="fas fa-eye toggle-password" id="togglePassword"></i>
                    </div>
                </div>
            </div>

            <!-- Device Section -->
            <div class="card">
                <div class="card-header">
                    <i class="fas fa-microchip"></i>
                    <h2>Device Settings</h2>
                </div>
                <div class="form-group">
                    <label>Device ID</label>
                    <input type="text" name="device_id" required placeholder="e.g., living-room-lights">
                </div>
                <div class="form-group">
                    <label>Number of LEDs</label>
                    <input type="number" name="num_leds" required min="1" max="600" value="60">
                </div>
            </div>

            <!-- Sinric Pro Section -->
            <div class="card">
                <div class="card-header">
                    <i class="fas fa-cloud"></i>
                    <h2>Sinric Pro (Google Home)</h2>
                </div>
                <div class="form-group">
                    <label>App Key</label>
                    <input type="text" name="sinric_app_key" placeholder="Enter App Key">
                </div>
                <div class="form-group">
                    <label>App Secret</label>
                    <input type="password" name="sinric_app_secret" placeholder="Enter App Secret">
                </div>
                <div class="form-group">
                    <label>Light ID</label>
                    <input type="text" name="sinric_light_id" placeholder="Enter Light ID">
                </div>
            </div>

            <!-- MQTT Section -->
            <div class="card">
                <div class="card-header">
                    <i class="fas fa-network-wired"></i>
                    <h2>MQTT Integration</h2>
                </div>
                <div class="control-row">
                    <label>Enable MQTT</label>
                    <label class="toggle">
                        <input type="checkbox" name="mqtt_enabled" id="mqtt_enabled">
                        <span class="slider"></span>
                    </label>
                </div>
                <div class="form-group">
                    <label>Broker Address</label>
                    <input type="text" name="mqtt_broker" placeholder="e.g., 192.168.1.100">
                </div>
                <div class="form-group">
                    <label>Port</label>
                    <input type="number" name="mqtt_port" value="1883">
                </div>
                <div class="form-group">
                    <label>Username (Optional)</label>
                    <input type="text" name="mqtt_user" placeholder="Enter Username">
                </div>
                <div class="form-group">
                    <label>Password (Optional)</label>
                    <input type="password" name="mqtt_pass" placeholder="Enter Password">
                </div>
                <div class="form-group">
                    <label>Device Name (Discovery)</label>
                    <input type="text" name="mqtt_device_name" placeholder="Lumina Gateway">
                </div>
            </div>

            <button type="submit" class="btn-primary">
                <i class="fas fa-save"></i>
                Save & Restart
            </button>

            <div id="status" class="status"></div>
        </form>

        <footer>
            <p>&copy; 2026 Lumina Inc. &bull; Designed by Majd Shwikani</p>
        </footer>
    </div>

    <script>
        let scanInterval;

        // WiFi scanning function
        async function scanWiFi() {
            const btn = document.querySelector('#scanControls .btn-secondary');
            const status = document.getElementById('scanStatus');
            const ssidList = document.getElementById('ssidList');

            btn.disabled = true;
            status.textContent = 'Scanning...';
            ssidList.innerHTML = '<div class="ssid-item"><span class="subtitle">Scanning for networks...</span></div>';

            try {
                const response = await fetch('/scan');
                const data = await response.json();

                ssidList.innerHTML = '';

                if (data.networks && data.networks.length > 0) {
                    // Sort by signal strength (strongest first)
                    data.networks.sort((a, b) => b.rssi - a.rssi);

                    data.networks.forEach(net => {
                        const item = document.createElement('div');
                        item.className = 'ssid-item';
                        item.onclick = () => selectSSID(net.ssid, item);

                        const quality = net.quality;
                        let qualityClass = 'weak';
                        if (quality > 50) qualityClass = 'strong';
                        else if (quality > 30) qualityClass = 'medium';

                        const secureIcon = net.secure ? '<i class="fas fa-lock"></i>' : '<i class="fas fa-unlock"></i>';

                        item.innerHTML = `
                            <div class="ssid-info">
                                <span class="ssid-name">${escapeHtml(net.ssid)}</span>
                                ${secureIcon}
                            </div>
                            <div class="ssid-rssi">
                                <span>${net.rssi} dBm</span>
                                <div class="rssi-bar">
                                    <div class="rssi-fill ${qualityClass}" style="width: ${quality}%"></div>
                                </div>
                            </div>
                        `;
                        ssidList.appendChild(item);
                    });

                    status.textContent = `${data.networks.length} network(s) found`;
                    document.getElementById('ssidSelection').classList.add('active');
                } else {
                    ssidList.innerHTML = '<div class="ssid-item"><span class="subtitle">No networks found</span></div>';
                    status.textContent = 'Try again';
                    document.getElementById('ssidSelection').classList.remove('active');
                }

                btn.disabled = false;

            } catch (err) {
                ssidList.innerHTML = '<div class="ssid-item"><span class="subtitle">Scan failed</span></div>';
                status.textContent = 'Error: ' + err.message;
                btn.disabled = false;
            }
        }

        function selectSSID(ssid, element) {
            document.getElementById('selectedSsid').value = ssid;
            document.getElementById('wifiPassword').focus();

            // Update visual selection
            document.querySelectorAll('.ssid-item').forEach(el => el.classList.remove('selected'));
            if (element) element.classList.add('selected');
        }

        function escapeHtml(text) {
            const div = document.createElement('div');
            div.textContent = text;
            return div.innerHTML;
        }

        // Password toggle
        const togglePassword = document.getElementById('togglePassword');
        const wifiPassword = document.getElementById('wifiPassword');

        togglePassword.addEventListener('click', function() {
            const type = wifiPassword.getAttribute('type') === 'password' ? 'text' : 'password';
            wifiPassword.setAttribute('type', type);
            this.classList.toggle('fa-eye');
            this.classList.toggle('fa-eye-slash');
        });

        document.getElementById('configForm').addEventListener('submit', function(e) {
            e.preventDefault();
            const btn = this.querySelector('button');
            const status = document.getElementById('status');
            const formData = new FormData(this);

            // Handle checkbox manually since FormData behaves weirdly with it
            formData.set('mqtt_enabled', document.getElementById('mqtt_enabled').checked ? '1' : '0');

            btn.disabled = true;
            btn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> Saving...';
            status.style.display = 'none';

            fetch('/save', { method: 'POST', body: formData })
            .then(r => r.json())
            .then(data => {
                if (data.success) {
                    status.className = 'status success';
                    status.textContent = 'Success! Device is restarting...';
                    status.style.display = 'block';
                    btn.innerHTML = 'Saved';
                } else {
                    throw new Error(data.message);
                }
            })
            .catch(err => {
                status.className = 'status error';
                status.textContent = 'Error: ' + err.message;
                status.style.display = 'block';
                btn.disabled = false;
                btn.innerHTML = '<i class="fas fa-save"></i> Save & Restart';
            });
        });
    </script>
</body>
</html>
)rawliteral";

bool saveConfig(const String& ssid, const String& password, const String& device_id, int num_leds, 
                const String& skey, const String& ssecret, const String& slid,
                const String& mbroker, int mport, const String& muser, const String& mpass, bool menabled, const String& mname) {
  
  DynamicJsonDocument doc(2048);
  doc["wifi_ssid"] = ssid;
  doc["wifi_password"] = password;
  doc["device_id"] = device_id;
  doc["num_leds"] = num_leds;
  
  doc["sinric_app_key"] = skey;
  doc["sinric_app_secret"] = ssecret;
  doc["sinric_light_id"] = slid;
  
  doc["mqtt_broker"] = mbroker;
  doc["mqtt_port"] = mport;
  doc["mqtt_user"] = muser;
  doc["mqtt_pass"] = mpass;
  doc["mqtt_enabled"] = menabled;
  doc["mqtt_device_name"] = mname;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) return false;

  serializeJson(doc, configFile);
  configFile.close();
  return true;
}

void handleRoot() {
  String html = String(htmlContent);
  html.replace("id=\"versionDisplay\"></span>", "id=\"versionDisplay\">v" + String(currentFirmwareVersion) + "</span>");
  server.send(200, "text/html", html);
}

void handleSave() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  String ssid = server.arg("wifi_ssid");
  String pass = server.arg("wifi_password");
  String devid = server.arg("device_id");
  int leds = server.arg("num_leds").toInt();
  
  String skey = server.arg("sinric_app_key");
  String ssecret = server.arg("sinric_app_secret");
  String slid = server.arg("sinric_light_id");
  
  String mbroker = server.arg("mqtt_broker");
  int mport = server.arg("mqtt_port").toInt();
  String muser = server.arg("mqtt_user");
  String mpass = server.arg("mqtt_pass");
  bool menabled = server.arg("mqtt_enabled") == "1";
  String mname = server.arg("mqtt_device_name");

  ssid.trim(); pass.trim(); devid.trim();
  skey.trim(); ssecret.trim(); slid.trim();
  mbroker.trim(); muser.trim(); mpass.trim(); mname.trim();

  if (saveConfig(ssid, pass, devid, leds, skey, ssecret, slid, mbroker, mport, muser, mpass, menabled, mname)) {
    server.send(200, "application/json", "{\"success\":true}");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    ESP.restart();
  } else {
    server.send(500, "application/json", "{\"success\":false,\"message\":\"Failed to save\"}");
  }
}

void handleScan() {
  if (server.method() != HTTP_GET) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  // Use synchronous scan (false) and show hidden networks (true)
  int n = WiFi.scanNetworks(false, true);

  DynamicJsonDocument doc(8192);
  JsonArray networks = doc["networks"].to<JsonArray>();

  for (int i = 0; i < n; ++i) {
    int rssi = WiFi.RSSI(i);
    // Aggressive filter to -80 to ensure only strong networks are shown
    if (rssi >= -80) {
      JsonObject net = networks.createNestedObject();
      net["ssid"] = WiFi.SSID(i);
      net["rssi"] = rssi;
      net["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
      int quality = min(100, max(0, (rssi + 100) * 100 / 20));
      net["quality"] = quality;
    }
  }

  // Delete scan results from memory
  WiFi.scanDelete();

  String jsonOutput;
  serializeJson(doc, jsonOutput);
  server.send(200, "application/json", jsonOutput);
}

void handleNotFound() {
  server.send(404, "text/plain", "Not Found");
}

void startConfigPortal() {
  // Use WIFI_AP_STA to allow scanning while the AP is active
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ap_ssid, ap_password);

  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.on("/scan", handleScan);
  server.onNotFound(handleNotFound);
  server.begin();

  unsigned long lastFlash = 0;
  bool ledOn = false;

  while (true) {
    server.handleClient();
    if (millis() - lastFlash > 500) {
      lastFlash = millis();
      ledOn = !ledOn;
      onboardLed[0] = ledOn ? CRGB::Blue : CRGB::Black;
      FastLED.show();
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}
