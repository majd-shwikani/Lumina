#include "web_config_portal.h"
#include <Arduino.h>

// Include necessary headers that were in the .h file
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include "globals.h"   // for onboardLed[]

WebServer server(80);

const char* ap_ssid = "Lumina";
const char* ap_password = ""; // No password

// HTML content for configuration page
const char* htmlContent = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Lumina - Device Configuration</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 20px;
        }
        
        .container {
            background: rgba(255, 255, 255, 0.95);
            border-radius: 20px;
            padding: 40px;
            box-shadow: 0 15px 35px rgba(0, 0, 0, 0.1);
            backdrop-filter: blur(10px);
            width: 100%;
            max-width: 500px;
        }
        
        .header {
            text-align: center;
            margin-bottom: 30px;
        }
        
        .header h1 {
            color: #333;
            font-size: 2.5em;
            margin-bottom: 10px;
            background: linear-gradient(135deg, #667eea, #764ba2);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
        }
        
        .header p {
            color: #666;
            font-size: 1.1em;
        }
        
        .form-group {
            margin-bottom: 25px;
        }
        
        label {
            display: block;
            margin-bottom: 8px;
            color: #333;
            font-weight: 600;
            font-size: 1em;
        }
        
        input[type="text"],
        input[type="password"],
        input[type="number"] {
            width: 100%;
            padding: 15px;
            border: 2px solid #e1e5e9;
            border-radius: 12px;
            font-size: 1em;
            transition: all 0.3s ease;
            background: #f8f9fa;
        }
        
        input[type="text"]:focus,
        input[type="password"]:focus,
        input[type="number"]:focus {
            outline: none;
            border-color: #667eea;
            background: #fff;
            box-shadow: 0 0 0 3px rgba(102, 126, 234, 0.1);
        }
        
        .btn {
            width: 100%;
            padding: 16px;
            background: linear-gradient(135deg, #667eea, #764ba2);
            color: white;
            border: none;
            border-radius: 12px;
            font-size: 1.1em;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.3s ease;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }
        
        .btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 10px 25px rgba(102, 126, 234, 0.3);
        }
        
        .btn:active {
            transform: translateY(0);
        }
        
        .info-box {
            background: #e3f2fd;
            border: 1px solid #bbdefb;
            border-radius: 12px;
            padding: 20px;
            margin-bottom: 25px;
        }
        
        .info-box h3 {
            color: #1976d2;
            margin-bottom: 10px;
            font-size: 1.2em;
        }
        
        .info-box p {
            color: #555;
            line-height: 1.5;
            font-size: 0.95em;
        }
        
        .status {
            text-align: center;
            padding: 15px;
            border-radius: 12px;
            margin-top: 20px;
            font-weight: 600;
            display: none;
        }
        
        .status.success {
            background: #e8f5e8;
            color: #2e7d32;
            border: 1px solid #c8e6c9;
        }
        
        .status.error {
            background: #ffebee;
            color: #c62828;
            border: 1px solid #ffcdd2;
        }
        
        .required::after {
            content: " *";
            color: #e53935;
        }
        
        @media (max-width: 600px) {
            .container {
                padding: 30px 20px;
            }
            
            .header h1 {
                font-size: 2em;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🌙 Lumina</h1>
            <p>Configure your LED device</p>
        </div>
        
        <div class="info-box">
            <h3>📋 Configuration Guide</h3>
            <p><strong>Device ID:</strong> Unique identifier for your device (e.g., living-room-lights)</p>
            <p><strong>LED Count:</strong> Number of LEDs in your strip (typically 30-144)</p>
            <p><strong>WiFi Credentials:</strong> Your network name and password</p>
        </div>
        
        <form id="configForm">
            <div class="form-group">
                <label for="wifi_ssid" class="required">WiFi Network Name (SSID)</label>
                <input type="text" id="wifi_ssid" name="wifi_ssid" required placeholder="Enter your WiFi network name">
            </div>
            
            <div class="form-group">
                <label for="wifi_password" class="required">WiFi Password</label>
                <input type="password" id="wifi_password" name="wifi_password" required placeholder="Enter your WiFi password">
            </div>
            
            <div class="form-group">
                <label for="device_id" class="required">Device ID</label>
                <input type="text" id="device_id" name="device_id" required placeholder="e.g., living-room-lights">
            </div>
            
            <div class="form-group">
                <label for="num_leds" class="required">Number of LEDs</label>
                <input type="number" id="num_leds" name="num_leds" required min="1" max="500" value="60" placeholder="Enter number of LEDs">
            </div>
            
            <button type="submit" class="btn">💾 Save Configuration</button>
            
            <div id="status" class="status"></div>
        </form>
    </div>

    <script>
        document.getElementById('configForm').addEventListener('submit', function(e) {
            e.preventDefault();
            
            const formData = new FormData(this);
            const statusDiv = document.getElementById('status');
            const submitBtn = this.querySelector('button[type="submit"]');
            
            // Show loading state
            submitBtn.disabled = true;
            submitBtn.textContent = 'Saving...';
            statusDiv.style.display = 'none';
            
            fetch('/save', {
                method: 'POST',
                body: formData
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    statusDiv.className = 'status success';
                    statusDiv.textContent = '✅ Configuration saved successfully! Device will restart and connect to your network.';
                    statusDiv.style.display = 'block';
                    
                    // Scroll to status message
                    statusDiv.scrollIntoView({ behavior: 'smooth' });
                    
                    // Update button text
                    submitBtn.textContent = 'Saved! Restarting...';
                    
                    // Inform user about restart
                    setTimeout(() => {
                        statusDiv.textContent += ' Please wait for device to restart and connect...';
                    }, 2000);
                    
                } else {
                    throw new Error(data.message || 'Unknown error');
                }
            })
            .catch(error => {
                statusDiv.className = 'status error';
                statusDiv.textContent = '❌ Error: ' + error.message;
                statusDiv.style.display = 'block';
                
                // Reset button
                submitBtn.disabled = false;
                submitBtn.textContent = '💾 Save Configuration';
                
                // Scroll to error message
                statusDiv.scrollIntoView({ behavior: 'smooth' });
            });
        });

        // Form validation
        const inputs = document.querySelectorAll('input[required]');
        inputs.forEach(input => {
            input.addEventListener('blur', function() {
                if (!this.value.trim()) {
                    this.style.borderColor = '#e53935';
                } else {
                    this.style.borderColor = '#e1e5e9';
                }
            });
        });

        // Focus on first input
        document.getElementById('wifi_ssid').focus();
    </script>
</body>
</html>
)rawliteral";

// Save configuration to SPIFFS
bool saveConfig(const String& ssid, const String& password, const String& device_id, int num_leds) {
  // Validate inputs
  if (ssid.length() == 0 || password.length() == 0 || device_id.length() == 0 || num_leds <= 0) {
    Serial.println("Invalid configuration values provided");
    return false;
  }

  DynamicJsonDocument doc(1024);
  doc["wifi_ssid"] = ssid;
  doc["wifi_password"] = password;
  doc["device_id"] = device_id;
  doc["num_leds"] = num_leds;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  serializeJson(doc, configFile);
  configFile.close();
  
  // Verify the file was written correctly
  configFile = SPIFFS.open("/config.json", "r");
  if (configFile) {
    String content = configFile.readString();
    configFile.close();
    Serial.println("Verified config file content:");
    Serial.println(content);
  }
  
  Serial.println("Configuration saved to SPIFFS:");
  Serial.println("SSID: " + ssid);
  Serial.println("Device ID: " + device_id);
  Serial.println("LED Count: " + String(num_leds));
  
  return true;
}
// Handle root page
void handleRoot() {
  server.send(200, "text/html", htmlContent);
}

// Handle configuration save
void handleSave() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  // Get and trim whitespace from all fields
  String ssid = server.arg("wifi_ssid");
  String password = server.arg("wifi_password");
  String device_id = server.arg("device_id");
  String num_leds_str = server.arg("num_leds");

  // Remove leading and trailing whitespace from all fields
  ssid.trim();
  password.trim();
  device_id.trim();
  num_leds_str.trim();

  // Validate required fields after trimming
  if (ssid.length() == 0 || password.length() == 0 || device_id.length() == 0 || num_leds_str.length() == 0) {
    DynamicJsonDocument doc(256);
    doc["success"] = false;
    doc["message"] = "All fields are required";
    String response;
    serializeJson(doc, response);
    server.send(400, "application/json", response);
    return;
  }

  int num_leds = num_leds_str.toInt();
  if (num_leds <= 0 || num_leds > 500) {
    DynamicJsonDocument doc(256);
    doc["success"] = false;
    doc["message"] = "LED count must be between 1 and 500";
    String response;
    serializeJson(doc, response);
    server.send(400, "application/json", response);
    return;
  }

  // Clean device ID - remove ALL whitespace but preserve case
  device_id.replace(" ", "");
  device_id.replace("\t", "");
  device_id.replace("\n", "");
  device_id.replace("\r", "");

  // Save configuration
  if (saveConfig(ssid, password, device_id, num_leds)) {
    DynamicJsonDocument doc(256);
    doc["success"] = true;
    doc["message"] = "Configuration saved successfully";
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);

    // Give time for response to be sent
    delay(1000);
    digitalWrite(2, LOW);
    Serial.println("Configuration saved. Restarting...");
    ESP.restart();
  } else {
    DynamicJsonDocument doc(256);
    doc["success"] = false;
    doc["message"] = "Failed to save configuration to storage";
    String response;
    serializeJson(doc, response);
    server.send(500, "application/json", response);
  }
}

// Handle not found
void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

// Start configuration portal
void startConfigPortal() {
  Serial.println("Starting Configuration Portal");
  
  // Start AP mode
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);
  
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  Serial.println("Configuration portal started");
  Serial.println("Connect to WiFi: " + String(ap_ssid));
  Serial.println("Then visit: http://" + myIP.toString());

  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.onNotFound(handleNotFound);

  // Start server
  server.begin();
  Serial.println("HTTP server started");

  // Drive the onboard LED solid blue to signal "waiting for config"
  onboardLed[0] = CRGB::Blue;
  FastLED.show();

  // Main loop for config portal
  while (true) {
    server.handleClient();
    delay(10);
  }
}