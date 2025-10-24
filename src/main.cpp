#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <Adafruit_VEML7700.h>
#include "effects.h"
#include <esp_task_wdt.h>  // Watchdog timer for system stability
#include <SPIFFS.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// Project configuration (WiFi, Firebase, etc.)
#include "config.h"

// Firebase token helper for authentication
#include <addons/TokenHelper.h>

// Web server for configuration
WebServer server(80);

// Light sensor threshold (controlled via Firebase)
volatile float luxThreshold = 1.0;

// Global objects
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_VEML7700 veml = Adafruit_VEML7700();
FirebaseData fbdoStream;  // For Firebase real-time stream
FirebaseData fbdoUpload;  // For Firebase write operations
// Global configuration data
ConfigData configData;
// Timer settings (HH:MM format)
char timerOnTime[6] = "09:00";
char timerOffTime[6] = "17:00";
bool timerEnabled = true;

// Firebase authentication and configuration
FirebaseAuth auth;
FirebaseConfig config;

// LED animation control variables
volatile int currentEffect = 0;
volatile uint32_t effectSpeed = 50;
volatile uint32_t effectColor = 0xFF0000;
volatile bool updateEffect = false;
volatile bool firebaseConnected = false;
volatile bool stripEnabled = true;           // Manual on/off toggle
volatile bool autoDarknessControl = true;    // Auto-off in low light
volatile bool turnedOffByDarkness = false;   // Track auto-off state

// Sensor data
volatile float currentLux = 0;
volatile bool sensorAvailable = false;

// Device-specific base path (FIXED: removed trailing slash)
String basePath;

// Configuration data structure



// Configuration state
bool configMode = false;

// Function declarations
void setupSPIFFS();
void loadConfig();
void saveConfig();
void startConfigMode();
void handleRoot();
void handleSaveConfig();
void connectToWiFi();
void setupFirebase();
void setupOTA();
void setupVEML7700();
void firebaseTask(void *parameter);
void ledTask(void *parameter);
void automationtask(void *parameter);
void sensorDataTask(void *parameter);
void timerTask(void *parameter);
void handleFirebaseData();
void updateLEDs();
void streamCallback(FirebaseStream data);
void streamTimeoutCallback(bool timeout);
void createDefaultFirebaseData();
void updateSensorData();
bool shouldTurnOffDueToDarkness();
bool checkTimeMatch(const char* scheduledTime);
void updateTimerState(bool state);
void readInitialFirebaseData();
void setupTime();

void setup() {
  Serial.begin(115200);
  
  // Initialize SPIFFS and load configuration
  setupSPIFFS();
  
  // If configuration is not complete, start config mode
  if (strlen(configData.wifiSSID) == 0 || strlen(configData.wifiPassword) == 0 || 
      strlen(configData.deviceID) == 0 || configData.numLeds == 0) {
    startConfigMode();
    return; // Don't continue with normal setup
  }
  
  // Set base path with loaded device ID
  basePath = "/devices/" + String(configData.deviceID);
  
  // Print device ID for verification
  Serial.println("Device ID: " + String(configData.deviceID));
  Serial.println("Base path: " + basePath);
  Serial.println("Number of LEDs: " + String(configData.numLeds));
  
  // Initialize NeoPixel strip with configured number of LEDs
  strip.updateLength(configData.numLeds);
  strip.begin();
  strip.show();
  strip.setBrightness(100);
  
  // Initialize watchdog timer (30s timeout)
  esp_task_wdt_init(30, true);
  
  // Initialize I2C for light sensor
  Wire.begin();
  
  // Setup sequences
  connectToWiFi();
  setupTime();
  setupOTA();
  setupVEML7700();
  setupFirebase();
  
  // Create FreeRTOS tasks for parallel processing
  
  // Firebase management task (core 0)
  xTaskCreatePinnedToCore(
    firebaseTask,
    "FirebaseTask",
    15000,
    NULL,
    1,
    NULL,
    0
  );

  // LED animation task (core 1)
  xTaskCreatePinnedToCore(
    ledTask,
    "LEDTask",
    15000,
    NULL,
    1,
    NULL,
    1
  );

  // Sensor data reporting task (core 0)
  xTaskCreatePinnedToCore(
    sensorDataTask,
    "SensorDataTask",
    15000,
    NULL,
    1,
    NULL,
    0
  );

  // Light-based automation task (core 0)
  xTaskCreatePinnedToCore(
    automationtask,
    "AutomationTask",
    4000,
    NULL,
    0,
    NULL,
    0
  );

  // Timer-based control task (core 0)
  xTaskCreatePinnedToCore(
    timerTask,
    "TimerTask",
    8000,
    NULL,
    1,
    NULL,
    0
  );
}

void loop() {
  if (configMode) {
    server.handleClient();
  } else {
    // Empty - all functionality handled by FreeRTOS tasks
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

// Initialize SPIFFS and load configuration
void setupSPIFFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println("An error occurred while mounting SPIFFS");
    return;
  }
  Serial.println("SPIFFS mounted successfully");
  loadConfig();
}

// Load configuration from SPIFFS
void loadConfig() {
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("No configuration file found, using defaults");
    memset(&configData, 0, sizeof(configData));
    return;
  }
  
  size_t size = configFile.size();
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  configFile.close();
  
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, buf.get());
  
  if (error) {
    Serial.println("Failed to parse config file");
    return;
  }
  
  strlcpy(configData.wifiSSID, doc["wifiSSID"] | "", sizeof(configData.wifiSSID));
  strlcpy(configData.wifiPassword, doc["wifiPassword"] | "", sizeof(configData.wifiPassword));
  strlcpy(configData.deviceID, doc["deviceID"] | "", sizeof(configData.deviceID));
  configData.numLeds = doc["numLeds"] | NUM_LEDS;
  
  Serial.println("Configuration loaded from SPIFFS");
}

// Save configuration to SPIFFS
void saveConfig() {
  DynamicJsonDocument doc(1024);
  doc["wifiSSID"] = configData.wifiSSID;
  doc["wifiPassword"] = configData.wifiPassword;
  doc["deviceID"] = configData.deviceID;
  doc["numLeds"] = configData.numLeds;
  
  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return;
  }
  
  serializeJson(doc, configFile);
  configFile.close();
  Serial.println("Configuration saved to SPIFFS");
}

// Start configuration mode with AP and web server
void startConfigMode() {
  configMode = true;
  Serial.println("Starting configuration mode...");
  
  // Set up Access Point
  WiFi.softAP("Lumina", "");
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  
  // Set up web server routes
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSaveConfig);
  server.begin();
  
  Serial.println("Web server started. Connect to WiFi 'Lumina' and visit http://192.168.4.1");
}

// Handle root page
void handleRoot() {
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <title>Lumina Configuration</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
      body { font-family: Arial; margin: 40px; background: #f0f0f0; }
      .container { background: white; padding: 20px; border-radius: 10px; max-width: 400px; margin: 0 auto; }
      h1 { color: #333; text-align: center; }
      .form-group { margin-bottom: 15px; }
      label { display: block; margin-bottom: 5px; font-weight: bold; }
      input { width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }
      button { background: #4CAF50; color: white; padding: 10px 15px; border: none; border-radius: 4px; cursor: pointer; width: 100%; }
      button:hover { background: #45a049; }
    </style>
  </head>
  <body>
    <div class="container">
      <h1>Lumina Configuration</h1>
      <form action="/save" method="post">
        <div class="form-group">
          <label for="ssid">WiFi SSID:</label>
          <input type="text" id="ssid" name="ssid" required>
        </div>
        <div class="form-group">
          <label for="password">WiFi Password:</label>
          <input type="password" id="password" name="password">
        </div>
        <div class="form-group">
          <label for="deviceid">Device ID:</label>
          <input type="text" id="deviceid" name="deviceid" required>
        </div>
        <div class="form-group">
          <label for="numleds">Number of LEDs:</label>
          <input type="number" id="numleds" name="numleds" value="180" min="1" max="1000" required>
        </div>
        <button type="submit">Save Configuration</button>
      </form>
    </div>
  </body>
  </html>
  )rawliteral";
  
  server.send(200, "text/html", html);
}

// Handle configuration save
void handleSaveConfig() {
  if (server.hasArg("ssid") && server.hasArg("deviceid") && server.hasArg("numleds")) {
    strlcpy(configData.wifiSSID, server.arg("ssid").c_str(), sizeof(configData.wifiSSID));
    strlcpy(configData.wifiPassword, server.arg("password").c_str(), sizeof(configData.wifiPassword));
    strlcpy(configData.deviceID, server.arg("deviceid").c_str(), sizeof(configData.deviceID));
    configData.numLeds = server.arg("numleds").toInt();
    
    saveConfig();
    
    String html = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <title>Configuration Saved</title>
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <style>
        body { font-family: Arial; margin: 40px; background: #f0f0f0; }
        .container { background: white; padding: 20px; border-radius: 10px; max-width: 400px; margin: 0 auto; text-align: center; }
        h1 { color: #4CAF50; }
      </style>
    </head>
    <body>
      <div class="container">
        <h1>Configuration Saved!</h1>
        <p>Device will restart and connect to your network.</p>
        <p>Device ID: )rawliteral" + String(configData.deviceID) + R"rawliteral(</p>
        <p>LEDs: )rawliteral" + String(configData.numLeds) + R"rawliteral(</p>
      </div>
      <script>
        setTimeout(function() {
          window.location.href = "/";
        }, 5000);
      </script>
    </body>
    </html>
    )rawliteral";
    
    server.send(200, "text/html", html);
    
    delay(3000);
    ESP.restart();
  } else {
    server.send(400, "text/plain", "Missing required fields");
  }
}

// Connect to WiFi network using configured credentials
void connectToWiFi() {
  WiFi.begin(configData.wifiSSID, configData.wifiPassword);
  Serial.print("Connecting to WiFi: ");
  Serial.println(configData.wifiSSID);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to WiFi. Starting configuration mode...");
    startConfigMode();
  }
}

// Synchronize with NTP server for accurate time
void setupTime() {
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  
  Serial.print("Waiting for time synchronization");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println();
  
  Serial.println("Time synchronized:");
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

// Setup Over-The-Air updates
void setupOTA() {
  String hostname = "esp32-neopixel-" + String(configData.deviceID);
  ArduinoOTA.setHostname(hostname.c_str());
  
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else {
        type = "filesystem";
      }
      Serial.println("Start updating " + type);
      // Turn off LEDs during update
      strip.clear();
      strip.show();
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();
  Serial.println("OTA Ready - Hostname: " + hostname);
}

// Initialize VEML7700 light sensor
void setupVEML7700() {
  if (!veml.begin()) {
    Serial.println("VEML7700 sensor not found, continuing without light sensor");
    sensorAvailable = false;
    return;
  }
  
  sensorAvailable = true;
  veml.setGain(VEML7700_GAIN_1_8);
  veml.setIntegrationTime(VEML7700_IT_100MS);
  Serial.println("VEML7700 initialized");
}

// Configure Firebase connection and stream
void setupFirebase() {
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_SECRET;
  
  // Firebase timeout configuration
  config.timeout.serverResponse = 10 * 1000;
  
  fbdoStream.setResponseSize(2048);
  
  // Initialize Firebase connection
  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);
  
  Serial.println("Connecting to Firebase...");
  delay(1000);
  
  // Read initial values from Firebase
  readInitialFirebaseData();

  createDefaultFirebaseData();
  
  // FIX: Stream from the parent devices path, not the specific device
  String streamPath = "/devices/" + String(configData.deviceID);
  Serial.println("Stream path: " + streamPath);
  
  if (!Firebase.RTDB.beginStream(&fbdoStream, streamPath.c_str())) {
    Serial.printf("Stream begin error: %s\n", fbdoStream.errorReason().c_str());
  }
  
  Firebase.RTDB.setStreamCallback(&fbdoStream, streamCallback, streamTimeoutCallback);
  Serial.println("Firebase initialized with stream and initial data reading");
}

// Read initial values from Firebase on startup
void readInitialFirebaseData() {
  Serial.println("Reading initial Firebase data for device: " + String(configData.deviceID));
  
  // Read current effect (FIXED: added leading slash)
  String effectPath = basePath + "/effect";
  if (Firebase.RTDB.getInt(&fbdoUpload, effectPath.c_str())) {
    currentEffect = fbdoUpload.intData();
    Serial.printf("Initial effect: %d\n", currentEffect);
  } else {
    Serial.printf("Failed to read effect, using default: %s\n", fbdoUpload.errorReason().c_str());
    currentEffect = 0;
    // Create default value if it doesn't exist
    Firebase.RTDB.setInt(&fbdoUpload, effectPath.c_str(), currentEffect);
  }
  
  // Read animation speed (FIXED: added leading slash)
  String speedPath = basePath + "/speed";
  if (Firebase.RTDB.getInt(&fbdoUpload, speedPath.c_str())) {
    effectSpeed = fbdoUpload.intData();
    Serial.printf("Initial speed: %d\n", effectSpeed);
  } else {
    Serial.printf("Failed to read speed, using default: %s\n", fbdoUpload.errorReason().c_str());
    effectSpeed = 50;
    Firebase.RTDB.setInt(&fbdoUpload, speedPath.c_str(), effectSpeed);
  }
  
  // Read LED color (FIXED: added leading slash)
  String colorPath = basePath + "/color";
  if (Firebase.RTDB.getString(&fbdoUpload, colorPath.c_str())) {
    String colorStr = fbdoUpload.stringData();
    if (colorStr.length() == 6) {
      effectColor = strtoul(colorStr.c_str(), NULL, 16);
      Serial.printf("Initial color: %s\n", colorStr.c_str());
    }
  } else {
    Serial.printf("Failed to read color, using default: %s\n", fbdoUpload.errorReason().c_str());
    effectColor = 0xFF0000;
    Firebase.RTDB.setString(&fbdoUpload, colorPath.c_str(), "FF0000");
  }
  
  // Read enabled state (FIXED: added leading slash)
  String enabledPath = basePath + "/enabled";
  if (Firebase.RTDB.getBool(&fbdoUpload, enabledPath.c_str())) {
    stripEnabled = fbdoUpload.boolData();
    Serial.printf("Initial enabled state: %s\n", stripEnabled ? "true" : "false");
    
    // Turn off LEDs if disabled
    if (!stripEnabled) {
      strip.clear();
      strip.show();
    }
  } else {
    Serial.printf("Failed to read enabled state, using default: %s\n", fbdoUpload.errorReason().c_str());
    stripEnabled = true;
    Firebase.RTDB.setBool(&fbdoUpload, enabledPath.c_str(), stripEnabled);
  }
  
  // Read auto darkness control setting (FIXED: added leading slash)
  String autoDarknessPath = basePath + "/auto_darkness_control";
  if (Firebase.RTDB.getBool(&fbdoUpload, autoDarknessPath.c_str())) {
    autoDarknessControl = fbdoUpload.boolData();
    Serial.printf("Initial auto darkness control: %s\n", autoDarknessControl ? "true" : "false");
  } else {
    Serial.printf("Failed to read auto darkness control, using default: %s\n", fbdoUpload.errorReason().c_str());
    autoDarknessControl = true;
    Firebase.RTDB.setBool(&fbdoUpload, autoDarknessPath.c_str(), autoDarknessControl);
  }
  
  // Read lux threshold for auto-off (FIXED: added leading slash)
  String luxThresholdPath = basePath + "/lux_threshold";
  if (Firebase.RTDB.getFloat(&fbdoUpload, luxThresholdPath.c_str())) {
    luxThreshold = fbdoUpload.floatData();
    Serial.printf("Initial lux threshold: %.2f\n", luxThreshold);
  } else {
    Serial.printf("Failed to read lux threshold, using default: %s\n", fbdoUpload.errorReason().c_str());
    luxThreshold = 1.0;
    Firebase.RTDB.setFloat(&fbdoUpload, luxThresholdPath.c_str(), luxThreshold);
  }
  
  // Read timer on time (FIXED: added leading slash)
  String timerOnPath = basePath + "/timer_on";
  if (Firebase.RTDB.getString(&fbdoUpload, timerOnPath.c_str())) {
    String timeStr = fbdoUpload.stringData();
    if (timeStr.length() == 5) {
      strncpy(timerOnTime, timeStr.c_str(), sizeof(timerOnTime));
      Serial.printf("Initial timer on time: %s\n", timerOnTime);
    }
  } else {
    Serial.printf("Failed to read timer on time, using default: %s\n", fbdoUpload.errorReason().c_str());
    strcpy(timerOnTime, "09:00");
    Firebase.RTDB.setString(&fbdoUpload, timerOnPath.c_str(), timerOnTime);
  }
  
  // Read timer off time (FIXED: added leading slash)
  String timerOffPath = basePath + "/timer_off";
  if (Firebase.RTDB.getString(&fbdoUpload, timerOffPath.c_str())) {
    String timeStr = fbdoUpload.stringData();
    if (timeStr.length() == 5) {
      strncpy(timerOffTime, timeStr.c_str(), sizeof(timerOffTime));
      Serial.printf("Initial timer off time: %s\n", timerOffTime);
    }
  } else {
    Serial.printf("Failed to read timer off time, using default: %s\n", fbdoUpload.errorReason().c_str());
    strcpy(timerOffTime, "17:00");
    Firebase.RTDB.setString(&fbdoUpload, timerOffPath.c_str(), timerOffTime);
  }
  
  // Read timer enabled state (FIXED: added leading slash)
  String timerEnabledPath = basePath + "/timer_enabled";
  if (Firebase.RTDB.getBool(&fbdoUpload, timerEnabledPath.c_str())) {
    timerEnabled = fbdoUpload.boolData();
    Serial.printf("Initial timer enabled: %s\n", timerEnabled ? "true" : "false");
  } else {
    Serial.printf("Failed to read timer enabled, using default: %s\n", fbdoUpload.errorReason().c_str());
    timerEnabled = true;
    Firebase.RTDB.setBool(&fbdoUpload, timerEnabledPath.c_str(), timerEnabled);
  }
}

// Firebase management task - handles OTA and connection monitoring
void firebaseTask(void *parameter) {
  for(;;) {
    if (WiFi.status() == WL_CONNECTED) {
      ArduinoOTA.handle();
      
      // Maintain Firebase connection
      if (!Firebase.ready()) {
        Serial.println("Firebase not ready, reconnecting...");
        firebaseConnected = false;
        setupFirebase();
      } else {
        firebaseConnected = true;
      }
    } else {
      firebaseConnected = false;
      Serial.println("WiFi disconnected, reconnecting...");
      connectToWiFi();
    }
    
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

// LED animation task - runs selected effect
void ledTask(void *parameter) {
  for(;;) {
    updateLEDs();
    vTaskDelay(effectSpeed / portTICK_PERIOD_MS);
  }
}

// Automation task - handles light-based auto on/off
void automationtask(void *parameter) {
  const TickType_t xDelay = 100 / portTICK_PERIOD_MS;

  for(;;) {
    if (sensorAvailable) {
      updateSensorData();

      // Auto-off when too dark
      if (autoDarknessControl && shouldTurnOffDueToDarkness()) {
        if (stripEnabled) {
          stripEnabled = false;
          turnedOffByDarkness = true;
          strip.clear();
          strip.show();
          Serial.println("Darkness detected - turning LEDs off automatically");
        }
      } 
      // Auto-on when light returns
      else if (autoDarknessControl && turnedOffByDarkness && !shouldTurnOffDueToDarkness()) {
        stripEnabled = true;
        turnedOffByDarkness = false;
        Serial.println("Light detected - turning LEDs back on automatically");
      }
    }

    vTaskDelay(xDelay);
  }
}

// Sensor data task - reads and reports light levels to Firebase (FIXED: corrected path)
void sensorDataTask(void *parameter) {
  const TickType_t xDelay = 2000 / portTICK_PERIOD_MS; // 2 second interval
  
  for(;;) {
    if (firebaseConnected) {
      // Update sensor reading
      if (sensorAvailable) {
        updateSensorData();
        
        // Send lux data to Firebase (FIXED: added leading slash)
        String luxPath = basePath + "/lux";
        Serial.println("Attempting to send lux data to path: " + luxPath);
        Serial.printf("Lux value: %.2f\n", currentLux);
        
        if (Firebase.RTDB.setFloat(&fbdoUpload, luxPath.c_str(), currentLux)) {
          Serial.printf("Lux data sent successfully: %.2f\n", currentLux);
        } else {
          Serial.printf("Failed to send lux data: %s\n", fbdoUpload.errorReason().c_str());
        }
      }
      
    } else {
      Serial.println("Firebase not connected, skipping sensor data send");
    }
    
    vTaskDelay(xDelay);
  }
}

// Timer task - handles scheduled on/off times
void timerTask(void *parameter) {
  const TickType_t xDelay = 1000 / portTICK_PERIOD_MS; // Check every second
  bool lastOnTriggered = false;
  bool lastOffTriggered = false;
  
  for(;;) {
    if (firebaseConnected && timerEnabled) {
      // Check timer on time
      if (checkTimeMatch(timerOnTime)) {
        if (!lastOnTriggered) {
          Serial.println("Timer ON triggered - forcing enabled to true");
          updateTimerState(true);
          lastOnTriggered = true;
        }
      } else {
        lastOnTriggered = false;
      }
      
      // Check timer off time
      if (checkTimeMatch(timerOffTime)) {
        if (!lastOffTriggered) {
          Serial.println("Timer OFF triggered - forcing enabled to false");
          updateTimerState(false);
          lastOffTriggered = true;
        }
      } else {
        lastOffTriggered = false;
      }
    }
    
    vTaskDelay(xDelay);
  }
}

// Update timer state in Firebase and locally (FIXED: corrected path)
void updateTimerState(bool state) {
  esp_task_wdt_reset();
  String enabledPath = basePath + "/enabled";
  if (Firebase.RTDB.setBool(&fbdoUpload, enabledPath.c_str(), state)) {
    stripEnabled = state;
    Serial.printf("Timer updated enabled state to: %s\n", state ? "true" : "false");
    
    // Turn off LEDs immediately if disabled
    if (!state) {
      strip.clear();
      strip.show();
    }
  } else {
    Serial.printf("Failed to update enabled state: %s\n", fbdoUpload.errorReason().c_str());
  }
  esp_task_wdt_reset();
}

// Check if current time matches scheduled time
bool checkTimeMatch(const char* scheduledTime) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return false;
  }
  
  char currentTime[6];
  strftime(currentTime, sizeof(currentTime), "%H:%M", &timeinfo);
  
  return strcmp(currentTime, scheduledTime) == 0;
}

// Read current light level from sensor
void updateSensorData() {
  currentLux = veml.readLux();
}

// Check if light level is below threshold
bool shouldTurnOffDueToDarkness() {
  return currentLux < luxThreshold;
}

// Handle real-time Firebase data changes
void streamCallback(FirebaseStream data) {
  Serial.printf("Stream data path: %s, event: %s, type: %s, value: %s\n",
                data.streamPath().c_str(),
                data.dataType().c_str(),
                data.eventType().c_str(),
                data.stringData().c_str());

  // FIX: Get the actual data path from the stream
  String dataPath = data.dataPath().c_str();
  Serial.println("Data path: " + dataPath);

  // Effect change
  if (dataPath == "/effect") {
    currentEffect = data.intData();
    Serial.printf("Effect changed to: %d\n", currentEffect);
  }
  // Speed change
  else if (dataPath == "/speed") {
    effectSpeed = data.intData();
    Serial.printf("Speed changed to: %d\n", effectSpeed);
  }
  // Color change
  else if (dataPath == "/color") {
    String colorStr = data.stringData();
    if (colorStr.length() == 6) {
      effectColor = strtoul(colorStr.c_str(), NULL, 16);
      Serial.printf("Color changed to: %s\n", colorStr.c_str());
    }
  }
  // Lux threshold change
  else if (dataPath == "/lux_threshold") {
    luxThreshold = data.floatData();
    Serial.printf("Lux threshold changed to: %.2f\n", luxThreshold);
  }
  // Enabled state change
  else if (dataPath == "/enabled") {
    bool newState = data.boolData();
    
    stripEnabled = newState;
    turnedOffByDarkness = false;  // Reset auto-off flag on manual toggle
    Serial.printf("Strip %s\n", stripEnabled ? "enabled" : "disabled");
    if (!stripEnabled) {
      strip.clear();
      strip.show();
    }
  }
  // Timer enabled change
  else if (dataPath == "/timer_enabled") {
    timerEnabled = data.boolData();
    Serial.printf("Timer %s\n", timerEnabled ? "enabled" : "disabled");
  }
  // Auto darkness control change
  else if (dataPath == "/auto_darkness_control") {
    autoDarknessControl = data.boolData();
    Serial.printf("Auto darkness control %s\n", autoDarknessControl ? "enabled" : "disabled");
  }
  // Timer on time change
  else if (dataPath == "/timer_on") {
    String timeStr = data.stringData();
    if (timeStr.length() == 5) {
      strncpy(timerOnTime, timeStr.c_str(), sizeof(timerOnTime));
      Serial.printf("Timer ON time changed to: %s\n", timerOnTime);
    }
  }
  // Timer off time change
  else if (dataPath == "/timer_off") {
    String timeStr = data.stringData();
    if (timeStr.length() == 5) {
      strncpy(timerOffTime, timeStr.c_str(), sizeof(timerOffTime));
      Serial.printf("Timer OFF time changed to: %s\n", timerOffTime);
    }
  }
}

// Handle Firebase stream timeout
void streamTimeoutCallback(bool timeout) {
  if (timeout) {
    Serial.println("Stream timeout, resuming...");
  }
  if (!fbdoStream.httpConnected()) {
    Serial.printf("Stream error: %s\n", fbdoStream.errorReason().c_str());
  }
}

// Update LED strip with current effect
void updateLEDs() {
  // Auto-off due to darkness
  if (autoDarknessControl && sensorAvailable && shouldTurnOffDueToDarkness()) {
    strip.clear();
    strip.show();
    return;
  }
  
  // Manual off state
  if (!stripEnabled) {
    strip.clear();
    strip.show();
    return;
  }
  
  // Run selected animation effect
  switch(currentEffect) {
    case 0: effectRainbow(); break;
    case 1: effectMeteorShower(); break;
    case 2: effectDigitalRain(); break;
    case 3: effectPulsingSpheres(); break;
    case 4: effectBinaryClock(); break;
    case 5: effectVortex(); break;
    case 6: effectDNAHelix(); break;
    case 7: effectAudioVisualizer(); break;
    case 8: effectLavaLamp(); break;
    case 9: effectRadarSweep(); break;
    case 10: effectQuantumParticles(); break;
    case 11: effectNeuralNetwork(); break;
    case 12: effectGalaxySpin(); break;
    case 13: effectCrystalGrowth(); break;
    case 14: effectLightningStorm(); break;
    case 15: effectOceanDepth(); break;
    case 16: effectNorthernLights(); break;
    case 17: effectTimeTunnel(); break;
    case 18: effectCyberCity(); break;
    case 19: effectSolarFlare(); break;
    case 20: effectFireSimulation(); break;
    case 21: effectSolidColor(); break;
    default: effectRainbow(); break;
  }
  strip.show();
}

// Create default data structure in Firebase (backup function) - FIXED: all paths corrected
void createDefaultFirebaseData() {
  Serial.println("Creating default Firebase data structure for device: " + String(configData.deviceID));
  
  // Set default effect (FIXED: added leading slash)
  String effectPath = basePath + "/effect";
  if (Firebase.RTDB.setInt(&fbdoUpload, effectPath.c_str(), 0)) {
    Serial.println("Effect set to default: 0");
  } else {
    Serial.printf("Failed to set effect: %s\n", fbdoUpload.errorReason().c_str());
  }
  
  // Set default speed (FIXED: added leading slash)
  String speedPath = basePath + "/speed";
  if (Firebase.RTDB.setInt(&fbdoUpload, speedPath.c_str(), 50)) {
    Serial.println("Speed set to default: 50");
  } else {
    Serial.printf("Failed to set speed: %s\n", fbdoUpload.errorReason().c_str());
  }
  
  // Set default color (FIXED: added leading slash)
  String colorPath = basePath + "/color";
  if (Firebase.RTDB.setString(&fbdoUpload, colorPath.c_str(), "FF0000")) {
    Serial.println("Color set to default: FF0000");
  } else {
    Serial.printf("Failed to set color: %s\n", fbdoUpload.errorReason().c_str());
  }
  
  // Set default enabled state (FIXED: added leading slash)
  String enabledPath = basePath + "/enabled";
  if (Firebase.RTDB.setBool(&fbdoUpload, enabledPath.c_str(), true)) {
    Serial.println("Enabled set to default: true");
  } else {
    Serial.printf("Failed to set enabled: %s\n", fbdoUpload.errorReason().c_str());
  }
  
  // Set default auto darkness control (FIXED: added leading slash)
  String autoDarknessPath = basePath + "/auto_darkness_control";
  if (Firebase.RTDB.setBool(&fbdoUpload, autoDarknessPath.c_str(), true)) {
    Serial.println("Auto darkness control set to default: true");
  } else {
    Serial.printf("Failed to set auto darkness control: %s\n", fbdoUpload.errorReason().c_str());
  }
  
  // Set default lux threshold (FIXED: added leading slash)
  String luxThresholdPath = basePath + "/lux_threshold";
  if (Firebase.RTDB.setFloat(&fbdoUpload, luxThresholdPath.c_str(), 1.0)) {
    Serial.println("Lux threshold set to default: 1.0");
  } else {
    Serial.printf("Failed to set lux threshold: %s\n", fbdoUpload.errorReason().c_str());
  }
  
  // Set default timer on time (FIXED: added leading slash)
  String timerOnPath = basePath + "/timer_on";
  if (Firebase.RTDB.setString(&fbdoUpload, timerOnPath.c_str(), "09:00")) {
    Serial.println("Timer ON set to default: 09:00");
  } else {
    Serial.printf("Failed to set timer ON: %s\n", fbdoUpload.errorReason().c_str());
  }
  
  // Set default timer off time (FIXED: added leading slash)
  String timerOffPath = basePath + "/timer_off";
  if (Firebase.RTDB.setString(&fbdoUpload, timerOffPath.c_str(), "17:00")) {
    Serial.println("Timer OFF set to default: 17:00");
  } else {
    Serial.printf("Failed to set timer OFF: %s\n", fbdoUpload.errorReason().c_str());
  }
  
  // Set default timer enabled state (FIXED: added leading slash)
  String timerEnabledPath = basePath + "/timer_enabled";
  if (Firebase.RTDB.setBool(&fbdoUpload, timerEnabledPath.c_str(), true)) {
    Serial.println("Timer enabled set to default: true");
  } else {
    Serial.printf("Failed to set timer enabled: %s\n", fbdoUpload.errorReason().c_str());
  }
  
  Serial.println("Default Firebase data structure created");
}