#include "web_config_portal.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

WebServer server(80);

const char* ap_ssid = "Lumina-mini";
const char* ap_password = "";

const char* htmlContent = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Lumina Mini - Configuration</title>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css">
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-body: #050505;
            --bg-card: #121212;
            --bg-input: #2a2a2a;
            --primary: #8b5cf6;
            --primary-glow: rgba(139, 92, 246, 0.4);
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

        .container { width: 100%; max-width: 500px; z-index: 1; }

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
            font-size: 2.2rem;
            font-weight: 700;
            background: linear-gradient(135deg, #fff 0%, #c4b5fd 100%);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            margin-bottom: 5px;
            letter-spacing: -1px;
        }

        .subtitle { color: var(--text-secondary); font-size: 0.95rem; }

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
            font-size: 1.1rem;
            background: rgba(139, 92, 246, 0.1);
            padding: 10px;
            border-radius: 12px;
        }

        .card-header h2 { font-size: 1.15rem; font-weight: 600; }

        .form-group { margin-bottom: 20px; }

        label {
            display: block;
            margin-bottom: 8px;
            color: var(--text-secondary);
            font-weight: 500;
            font-size: 0.85rem;
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

        input:focus { border-color: var(--primary); box-shadow: 0 0 0 4px rgba(139, 92, 246, 0.1); }

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
            margin-top: 10px;
        }

        .btn-primary:hover {
            background: #7c3aed;
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
            <h1>Lumina Mini</h1>
            <span class="subtitle">Receiver Configuration Portal</span>
            <span class="subtitle" id="versionDisplay" style="font-size: 0.8rem; opacity: 0.5; margin-top: 5px;"></span>
        </header>

        <form id="configForm">
            <div class="card">
                <div class="card-header">
                    <i class="fas fa-wifi"></i>
                    <h2>WiFi Network</h2>
                </div>
                <div class="form-group">
                    <label>Network Name (SSID)</label>
                    <input type="text" name="wifi_ssid" required placeholder="Enter SSID">
                </div>
                <div class="form-group">
                    <label>Password</label>
                    <input type="password" name="wifi_password" required placeholder="Enter Password">
                </div>
            </div>

            <div class="card">
                <div class="card-header">
                    <i class="fas fa-microchip"></i>
                    <h2>Device Settings</h2>
                </div>
                <div class="form-group">
                    <label>Device ID</label>
                    <input type="text" name="device_id" required placeholder="e.g., bedroom-mini">
                </div>
                <div class="form-group">
                    <label>Number of LEDs</label>
                    <input type="number" name="num_leds" required min="1" max="500" value="60">
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
        document.getElementById('configForm').addEventListener('submit', function(e) {
            e.preventDefault();
            const btn = this.querySelector('button');
            const status = document.getElementById('status');
            const formData = new FormData(this);

            btn.disabled = true;
            btn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> Saving...';
            status.style.display = 'none';

            fetch('/save', { method: 'POST', body: formData })
            .then(r => r.json())
            .then(data => {
                if (data.success) {
                    status.className = 'status success';
                    status.textContent = '✅ Success! Receiver is restarting...';
                    status.style.display = 'block';
                    btn.innerHTML = '✅ Saved';
                } else {
                    throw new Error(data.message);
                }
            })
            .catch(err => {
                status.className = 'status error';
                status.textContent = '❌ Error: ' + err.message;
                status.style.display = 'block';
                btn.disabled = false;
                btn.innerHTML = '<i class="fas fa-save"></i> Save & Restart';
            });
        });
    </script>
</body>
</html>
)rawliteral";

bool saveConfig(const String& ssid, const String& password, const String& device_id, int num_leds) {
  DynamicJsonDocument doc(1024);
  doc["wifi_ssid"] = ssid;
  doc["wifi_password"] = password;
  doc["device_id"] = device_id;
  doc["num_leds"] = num_leds;

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

  ssid.trim(); pass.trim(); devid.trim();

  if (saveConfig(ssid, pass, devid, leds)) {
    server.send(200, "application/json", "{\"success\":true}");
    delay(1000);
    ESP.restart();
  } else {
    server.send(500, "application/json", "{\"success\":false,\"message\":\"Failed to save\"}");
  }
}

void handleNotFound() {
  server.send(404, "text/plain", "Not Found");
}

void startConfigPortal() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);
  
  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.onNotFound(handleNotFound);
  server.begin();

  pinMode(2, OUTPUT);
  unsigned long lastBlink = 0;
  bool ledState = false;

  while (true) {
    server.handleClient();
    if (millis() - lastBlink > 500) {
      ledState = !ledState;
      digitalWrite(2, ledState);
      lastBlink = millis();
    }
    delay(10);
  }
}
