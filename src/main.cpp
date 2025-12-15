#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <Adafruit_VEML7700.h>
#include "effects.h"
#include "sensors.h"
#include <esp_task_wdt.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <driver/i2s.h>
#include <arduinoFFT.h>

// Firebase token helper
#include <addons/TokenHelper.h>

// Configuration
#include "config.h"

// Configuration portal
#include "web_config_portal.h"

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================

Adafruit_NeoPixel strip(1, LED_PIN, NEO_GRB + NEO_KHZ800);
FirebaseData fbdoStream;
FirebaseData fbdoUpload;
FirebaseAuth auth;
FirebaseConfig config;

// ============================================================================
// LED ANIMATION CONTROL VARIABLES
// ============================================================================

volatile int currentEffect = 0;
volatile uint32_t effectSpeed = 50;
volatile uint32_t effectColor = 0xFF0000;
volatile bool updateEffect = false;
volatile bool firebaseConnected = false;
volatile bool stripEnabled = true;
volatile bool autoDarknessControl = true;
volatile bool turnedOffByDarkness = false;
bool defaultDataCreated = false;

// ============================================================================
// TIMER SETTINGS
// ============================================================================

char timerOnTime[6] = "09:00";
char timerOffTime[6] = "17:00";
bool timerEnabled = true;

// ============================================================================
// DEVICE CONFIGURATION FROM SPIFFS
// ============================================================================

String deviceID;
String wifiSSID;
String wifiPassword;
int ledCount;
String basePath;

// ============================================================================
// GITHUB OTA UPDATE CONFIGURATION
// ============================================================================

const char* GITHUB_FIRMWARE_URL = "https://github.com/majd-shwikani/Lumina-bin/releases/download/Lumina/firmware.bin";
const char* GITHUB_VERSION_URL = "https://raw.githubusercontent.com/majd-shwikani/Lumina-bin/refs/heads/main/version.txt";

const char* currentFirmwareVersion = "1.0.2";
const unsigned long UPDATE_CHECK_INTERVAL = 10 * 60 * 1000; // Check every 10 minutes
unsigned long lastUpdateCheck = 0;

// ============================================================================
// BUTTON PRESS DETECTION
// ============================================================================

#define BUTTON_PIN 19
unsigned long buttonPressStart = 0;
bool buttonActive = false;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

// SPIFFS & Configuration
void initSPIFFS();
bool loadConfig();
bool shouldStartConfigPortal();

// WiFi & Network
void connectToWiFi();
void setupTime();
void setupOTA();

// Firebase
void setupFirebase();
void readInitialFirebaseData();
void createDefaultFirebaseData();
void handleFirebaseData();
void streamCallback(FirebaseStream data);
void streamTimeoutCallback(bool timeout);

// LED Control
void updateLEDs();

// Timer Functions
bool checkTimeMatch(const char* scheduledTime);
void updateTimerState(bool state);

// GitHub OTA
void checkForGitHubUpdate();
String fetchLatestVersion();
bool startGitHubOTAUpdate(WiFiClient* client, int contentLength);
void downloadAndApplyFirmware();

// FreeRTOS Tasks
void firebaseTask(void *parameter);
void ledTask(void *parameter);
void automationtask(void *parameter);
void sensorDataTask(void *parameter);
void timerTask(void *parameter);

// ============================================================================
// SETUP FUNCTION
// ============================================================================

void setup() {
  Serial.begin(115200);
  
  // Initialize SPIFFS
  initSPIFFS();
  
  // Configure button input
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Check if config portal is needed
  if (shouldStartConfigPortal()) {
    Serial.println("No configuration found. Starting config portal...");
    startConfigPortal();
    return;
  }
  
  // Load configuration from SPIFFS
  if (!loadConfig()) {
    Serial.println("Failed to load config, restarting...");
    ESP.restart();
    return;
  }
  
  // Print loaded configuration
  Serial.println("\n=== Device Configuration ===");
  Serial.println("Device ID: " + deviceID);
  Serial.println("Base path: " + basePath);
  Serial.println("LED Count: " + String(ledCount));
  Serial.println("===========================\n");
  
  // Initialize NeoPixel strip with configured count
  strip.updateLength(ledCount);
  strip.begin();
  strip.show();
  strip.setBrightness(100);
  
  // Initialize watchdog timer (30s timeout)
  esp_task_wdt_init(30, true);
  
  // Initialize I2C for light sensor
  Wire.begin();
  
  // ========== SETUP SEQUENCE ==========
  
  Serial.println("Initializing systems...");
  
  connectToWiFi();
  Serial.println("✓ WiFi initialized");
  
  setupTime();
  Serial.println("✓ Time synchronized");
  
  setupOTA();
  Serial.println("✓ OTA enabled");
  
  setupVEML7700();
  Serial.println("✓ Light sensor initialized");
  
  setupFirebase();
  Serial.println("✓ Firebase initialized");
  
  setupFrequencyDetection();
  Serial.println("✓ Frequency detection initialized");
  Serial.println("✓ Starting microphone calibration...");
  
  Serial.println("All systems initialized!\n");
  
  // ========== CREATE FREERTOS TASKS ==========
  
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
  
  Serial.println("All tasks created successfully!");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  // Check for 7-second button press to reset config
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (!buttonActive) {
      buttonActive = true;
      buttonPressStart = millis();
      Serial.println("\nButton pressed - hold for 7 seconds to reset config");
    }
    
    // Check if held for 7 seconds
    if (millis() - buttonPressStart > 7000) {
      Serial.println("7-second button press detected - resetting configuration...");
      
      if (SPIFFS.exists("/config.json")) {
        SPIFFS.remove("/config.json");
      }
      
      // Restart to enter config portal
      ESP.restart();
    }
  } else {
    buttonActive = false;
  }
  
  // Check for GitHub OTA updates periodically
  if (WiFi.status() == WL_CONNECTED && millis() - lastUpdateCheck > UPDATE_CHECK_INTERVAL) {
    lastUpdateCheck = millis();
    checkForGitHubUpdate();
  }

  vTaskDelay(100 / portTICK_PERIOD_MS);
}

// ============================================================================
// SPIFFS & CONFIGURATION FUNCTIONS
// ============================================================================

void initSPIFFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
    return;
  }
  Serial.println("SPIFFS mounted successfully");
}

bool loadConfig() {
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return false;
  }
  
  size_t size = configFile.size();
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  configFile.close();
  
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, buf.get());
  if (error) {
    Serial.println("Failed to parse config file");
    return false;
  }
  
  wifiSSID = doc["wifi_ssid"].as<String>();
  wifiPassword = doc["wifi_password"].as<String>();
  deviceID = doc["device_id"].as<String>();
  ledCount = doc["num_leds"];
  
  // Set base path
  basePath = "/devices/" + deviceID;
  
  Serial.println("Configuration loaded:");
  Serial.println("  SSID: " + wifiSSID);
  Serial.println("  Device ID: " + deviceID);
  Serial.println("  LED Count: " + String(ledCount));
  
  return true;
}

bool shouldStartConfigPortal() {
  return !SPIFFS.exists("/config.json");
}

// ============================================================================
// WIFI & NETWORK FUNCTIONS
// ============================================================================

void connectToWiFi() {
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  Serial.print("Connecting to WiFi: " + wifiSSID);
  
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 30000) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to WiFi");
  }
}

void setupTime() {
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  
  Serial.print("Synchronizing time");
  struct tm timeinfo;
  unsigned long startTime = millis();
  while (!getLocalTime(&timeinfo) && millis() - startTime < 10000) {
    Serial.print(".");
    delay(1000);
  }
  
  if (getLocalTime(&timeinfo)) {
    Serial.println();
    Serial.println("Time synchronized:");
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  } else {
    Serial.println("\nFailed to synchronize time");
  }
}

void setupOTA() {
  String hostname = "esp32-neopixel-" + deviceID;
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

// ============================================================================
// FIREBASE SETUP & CONFIGURATION
// ============================================================================

void setupFirebase() {
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_SECRET;
  config.timeout.serverResponse = 10 * 1000;
  
  fbdoStream.setResponseSize(2048);
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);
  
  Serial.println("Connecting to Firebase...");
  delay(1000);
  
  // Read initial values from Firebase
  readInitialFirebaseData();

  // Start stream from device path
  String streamPath = "/devices/" + deviceID;
  Serial.println("Stream path: " + streamPath);
  
  if (!Firebase.RTDB.beginStream(&fbdoStream, streamPath.c_str())) {
    Serial.printf("Stream begin error: %s\n", fbdoStream.errorReason().c_str());
  }
  
  Firebase.RTDB.setStreamCallback(&fbdoStream, streamCallback, streamTimeoutCallback);
  Serial.println("Firebase initialized with stream and initial data reading");
}

// ============================================================================
// FIREBASE DATA READING (INITIAL)
// ============================================================================

void readInitialFirebaseData() {
  Serial.println("\nReading initial Firebase data for device: " + deviceID);
  
  // Read current effect
  String effectPath = basePath + "/effect";
  if (Firebase.RTDB.getInt(&fbdoUpload, effectPath.c_str())) {
    currentEffect = fbdoUpload.intData();
    Serial.printf("Initial effect: %d\n", currentEffect);
  } else {
    Serial.printf("Failed to read effect: %s\n", fbdoUpload.errorReason().c_str());
    currentEffect = 0;
    Firebase.RTDB.setInt(&fbdoUpload, effectPath.c_str(), currentEffect);
  }
  
  // Read animation speed
  String speedPath = basePath + "/speed";
  if (Firebase.RTDB.getInt(&fbdoUpload, speedPath.c_str())) {
    effectSpeed = fbdoUpload.intData();
    Serial.printf("Initial speed: %d\n", effectSpeed);
  } else {
    Serial.printf("Failed to read speed: %s\n", fbdoUpload.errorReason().c_str());
    effectSpeed = 50;
    Firebase.RTDB.setInt(&fbdoUpload, speedPath.c_str(), effectSpeed);
  }
  
  // Read LED color
  String colorPath = basePath + "/color";
  if (Firebase.RTDB.getString(&fbdoUpload, colorPath.c_str())) {
    String colorStr = fbdoUpload.stringData();
    if (colorStr.length() == 6) {
      effectColor = strtoul(colorStr.c_str(), NULL, 16);
      Serial.printf("Initial color: %s\n", colorStr.c_str());
    }
  } else {
    Serial.printf("Failed to read color: %s\n", fbdoUpload.errorReason().c_str());
    effectColor = 0xFF0000;
    Firebase.RTDB.setString(&fbdoUpload, colorPath.c_str(), "FF0000");
  }
  
  // Read enabled state
  String enabledPath = basePath + "/enabled";
  if (Firebase.RTDB.getBool(&fbdoUpload, enabledPath.c_str())) {
    stripEnabled = fbdoUpload.boolData();
    Serial.printf("Initial enabled state: %s\n", stripEnabled ? "true" : "false");
    if (!stripEnabled) {
      strip.clear();
      strip.show();
    }
  } else {
    Serial.printf("Failed to read enabled state: %s\n", fbdoUpload.errorReason().c_str());
    stripEnabled = true;
    Firebase.RTDB.setBool(&fbdoUpload, enabledPath.c_str(), stripEnabled);
  }
  
  // Read auto darkness control setting
  String autoDarknessPath = basePath + "/auto_darkness_control";
  if (Firebase.RTDB.getBool(&fbdoUpload, autoDarknessPath.c_str())) {
    autoDarknessControl = fbdoUpload.boolData();
    Serial.printf("Initial auto darkness control: %s\n", autoDarknessControl ? "true" : "false");
  } else {
    Serial.printf("Failed to read auto darkness control: %s\n", fbdoUpload.errorReason().c_str());
    autoDarknessControl = true;
    Firebase.RTDB.setBool(&fbdoUpload, autoDarknessPath.c_str(), autoDarknessControl);
  }
  
  // Read lux threshold for auto-off
  String luxThresholdPath = basePath + "/lux_threshold";
  if (Firebase.RTDB.getFloat(&fbdoUpload, luxThresholdPath.c_str())) {
    luxThreshold = fbdoUpload.floatData();
    Serial.printf("Initial lux threshold: %.2f\n", luxThreshold);
  } else {
    Serial.printf("Failed to read lux threshold: %s\n", fbdoUpload.errorReason().c_str());
    luxThreshold = 1.0;
    Firebase.RTDB.setFloat(&fbdoUpload, luxThresholdPath.c_str(), luxThreshold);
  }
  
  // Read timer on time
  String timerOnPath = basePath + "/timer_on";
  if (Firebase.RTDB.getString(&fbdoUpload, timerOnPath.c_str())) {
    String timeStr = fbdoUpload.stringData();
    if (timeStr.length() == 5) {
      strncpy(timerOnTime, timeStr.c_str(), sizeof(timerOnTime));
      Serial.printf("Initial timer on time: %s\n", timerOnTime);
    }
  } else {
    Serial.printf("Failed to read timer on time: %s\n", fbdoUpload.errorReason().c_str());
    strcpy(timerOnTime, "09:00");
    Firebase.RTDB.setString(&fbdoUpload, timerOnPath.c_str(), timerOnTime);
  }
  
  // Read timer off time
  String timerOffPath = basePath + "/timer_off";
  if (Firebase.RTDB.getString(&fbdoUpload, timerOffPath.c_str())) {
    String timeStr = fbdoUpload.stringData();
    if (timeStr.length() == 5) {
      strncpy(timerOffTime, timeStr.c_str(), sizeof(timerOffTime));
      Serial.printf("Initial timer off time: %s\n", timerOffTime);
    }
  } else {
    Serial.printf("Failed to read timer off time: %s\n", fbdoUpload.errorReason().c_str());
    strcpy(timerOffTime, "17:00");
    Firebase.RTDB.setString(&fbdoUpload, timerOffPath.c_str(), timerOffTime);
  }
  
  // Read timer enabled state
  String timerEnabledPath = basePath + "/timer_enabled";
  if (Firebase.RTDB.getBool(&fbdoUpload, timerEnabledPath.c_str())) {
    timerEnabled = fbdoUpload.boolData();
    Serial.printf("Initial timer enabled: %s\n", timerEnabled ? "true" : "false");
  } else {
    Serial.printf("Failed to read timer enabled: %s\n", fbdoUpload.errorReason().c_str());
    timerEnabled = true;
    Firebase.RTDB.setBool(&fbdoUpload, timerEnabledPath.c_str(), timerEnabled);
  }
  
  // Read reset flag
  String resetPath = basePath + "/reset";
  Firebase.RTDB.setBool(&fbdoUpload, resetPath.c_str(), false);
  
  // Publish firmware version
  String versionPath = basePath + "/version";
  if (Firebase.RTDB.setString(&fbdoUpload, versionPath.c_str(), currentFirmwareVersion)) {
    Serial.printf("Firmware version published: %s\n", currentFirmwareVersion);
  } else {
    Serial.printf("Failed to publish firmware version: %s\n", fbdoUpload.errorReason().c_str());
  }
  
  Serial.println("Initial data read complete.\n");
}

// ============================================================================
// FIREBASE STREAM CALLBACK
// ============================================================================

void streamCallback(FirebaseStream data) {
  Serial.printf("Stream data path: %s, type: %s, value: %s\n",
                data.dataPath().c_str(),
                data.dataType().c_str(),
                data.stringData().c_str());

  String dataPath = data.dataPath().c_str();

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
    turnedOffByDarkness = false;
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
  // Reset handler
  else if (dataPath == "/reset" && data.boolData() == true) {
    Serial.println("Reset command received - deleting config and restarting...");
    
    if (SPIFFS.exists("/config.json")) {
      SPIFFS.remove("/config.json");
    }
    
    ESP.restart();
    return;
  }
}

void streamTimeoutCallback(bool timeout) {
  if (timeout) {
    Serial.println("Stream timeout, resuming...");
  }
  if (!fbdoStream.httpConnected()) {
    Serial.printf("Stream error: %s\n", fbdoStream.errorReason().c_str());
  }
}

// ============================================================================
// CREATE DEFAULT FIREBASE DATA
// ============================================================================

void createDefaultFirebaseData() {
  String testPath = basePath + "/effect";
  
  if (Firebase.RTDB.getInt(&fbdoUpload, testPath.c_str())) {
    Serial.println("Firebase data already exists, skipping default data creation");
    defaultDataCreated = true;
    return;
  }
  
  Serial.println("Creating default Firebase structure for device: " + deviceID);
  
  bool allSuccess = true;
  
  // Set default effect
  String effectPath = basePath + "/effect";
  if (Firebase.RTDB.setInt(&fbdoUpload, effectPath.c_str(), 0)) {
    Serial.println("Effect set to default: 0");
  } else {
    Serial.printf("Failed to set effect: %s\n", fbdoUpload.errorReason().c_str());
    allSuccess = false;
  }
  
  // Set default speed
  String speedPath = basePath + "/speed";
  if (Firebase.RTDB.setInt(&fbdoUpload, speedPath.c_str(), 50)) {
    Serial.println("Speed set to default: 50");
  } else {
    Serial.printf("Failed to set speed: %s\n", fbdoUpload.errorReason().c_str());
    allSuccess = false;
  }
  
  // Set default color
  String colorPath = basePath + "/color";
  if (Firebase.RTDB.setString(&fbdoUpload, colorPath.c_str(), "FF0000")) {
    Serial.println("Color set to default: FF0000");
  } else {
    Serial.printf("Failed to set color: %s\n", fbdoUpload.errorReason().c_str());
    allSuccess = false;
  }
  
  // Set default enabled state
  String enabledPath = basePath + "/enabled";
  if (Firebase.RTDB.setBool(&fbdoUpload, enabledPath.c_str(), true)) {
    Serial.println("Enabled set to default: true");
  } else {
    Serial.printf("Failed to set enabled: %s\n", fbdoUpload.errorReason().c_str());
    allSuccess = false;
  }
  
  // Set default auto darkness control
  String autoDarknessPath = basePath + "/auto_darkness_control";
  if (Firebase.RTDB.setBool(&fbdoUpload, autoDarknessPath.c_str(), true)) {
    Serial.println("Auto darkness control set to default: true");
  } else {
    Serial.printf("Failed to set auto darkness control: %s\n", fbdoUpload.errorReason().c_str());
    allSuccess = false;
  }
  
  // Set default lux threshold
  String luxThresholdPath = basePath + "/lux_threshold";
  if (Firebase.RTDB.setFloat(&fbdoUpload, luxThresholdPath.c_str(), 1.0)) {
    Serial.println("Lux threshold set to default: 1.0");
  } else {
    Serial.printf("Failed to set lux threshold: %s\n", fbdoUpload.errorReason().c_str());
    allSuccess = false;
  }
  
  // Set default timer on time
  String timerOnPath = basePath + "/timer_on";
  if (Firebase.RTDB.setString(&fbdoUpload, timerOnPath.c_str(), "09:00")) {
    Serial.println("Timer ON set to default: 09:00");
  } else {
    Serial.printf("Failed to set timer ON: %s\n", fbdoUpload.errorReason().c_str());
    allSuccess = false;
  }
  
  // Set default timer off time
  String timerOffPath = basePath + "/timer_off";
  if (Firebase.RTDB.setString(&fbdoUpload, timerOffPath.c_str(), "17:00")) {
    Serial.println("Timer OFF set to default: 17:00");
  } else {
    Serial.printf("Failed to set timer OFF: %s\n", fbdoUpload.errorReason().c_str());
    allSuccess = false;
  }
  
  // Set default timer enabled state
  String timerEnabledPath = basePath + "/timer_enabled";
  if (Firebase.RTDB.setBool(&fbdoUpload, timerEnabledPath.c_str(), true)) {
    Serial.println("Timer enabled set to default: true");
  } else {
    Serial.printf("Failed to set timer enabled: %s\n", fbdoUpload.errorReason().c_str());
    allSuccess = false;
  }
  
  // Set default reset flag
  String resetPath = basePath + "/reset";
  if (Firebase.RTDB.setBool(&fbdoUpload, resetPath.c_str(), false)) {
    Serial.println("Reset flag set to default: false");
  } else {
    Serial.printf("Failed to set reset flag: %s\n", fbdoUpload.errorReason().c_str());
    allSuccess = false;
  }
  
  defaultDataCreated = allSuccess;
  
  if (allSuccess) {
    Serial.println("Default Firebase data structure created successfully");
  } else {
    Serial.println("Some default values failed to set. Check Firebase rules.");
  }
}

// ============================================================================
// FREERTOS TASKS
// ============================================================================

// Firebase management task - handles OTA and connection monitoring
void firebaseTask(void *parameter) {
  for(;;) {
    if (WiFi.status() == WL_CONNECTED) {
      ArduinoOTA.handle();
      
      if (!Firebase.ready()) {
        Serial.println("Firebase not ready, reconnecting...");
        firebaseConnected = false;
        delay(1000);
      } else {
        firebaseConnected = true;
      }
    } else {
      firebaseConnected = false;
      Serial.println("WiFi disconnected, attempting reconnect...");
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

// Sensor data task - reads and reports light levels to Firebase
void sensorDataTask(void *parameter) {
  const TickType_t xDelay = 2000 / portTICK_PERIOD_MS; // 2 second interval
  
  for(;;) {
    if (firebaseConnected) {
      if (sensorAvailable) {
        updateSensorData();
        
        String luxPath = basePath + "/lux";
        
        if (Firebase.RTDB.setFloat(&fbdoUpload, luxPath.c_str(), currentLux)) {
          Serial.printf("Lux data sent: %.2f\n", currentLux);
        } else {
          Serial.printf("Failed to send lux data: %s\n", fbdoUpload.errorReason().c_str());
        }
      }
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
          Serial.println("Timer ON triggered");
          updateTimerState(true);
          lastOnTriggered = true;
        }
      } else {
        lastOnTriggered = false;
      }
      
      // Check timer off time
      if (checkTimeMatch(timerOffTime)) {
        if (!lastOffTriggered) {
          Serial.println("Timer OFF triggered");
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

// ============================================================================
// TIMER & TIME FUNCTIONS
// ============================================================================

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

void updateTimerState(bool state) {
  esp_task_wdt_reset();
  String enabledPath = basePath + "/enabled";
  if (Firebase.RTDB.setBool(&fbdoUpload, enabledPath.c_str(), state)) {
    stripEnabled = state;
    Serial.printf("Timer updated enabled state to: %s\n", state ? "true" : "false");
    
    if (!state) {
      strip.clear();
      strip.show();
    }
  } else {
    Serial.printf("Failed to update enabled state: %s\n", fbdoUpload.errorReason().c_str());
  }
  esp_task_wdt_reset();
}

// ============================================================================
// LED CONTROL FUNCTION
// ============================================================================

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
    // Original effects (0-21)
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
    
    // NEW: Sound-reactive effects (22-32)
    case 22: effectFrequencySpectrum(); break;
    case 23: effectReactiveWaveform(); break;
    case 24: effectBeatPulse(); break;
    case 25: effectFrequencyBloom(); break;
    case 26: effectAudioReactiveFire(); break;
    case 27: effectMusicalRainbow(); break;
    case 28: effectReactiveStrobe(); break;
    case 29: effectGuitarVisualizer(); break;
    case 30: effectCascadingFrequency(); break;
    case 31: effectEnergyOrbits(); break;
    case 32: effectAudioRipples(); break;
    
    default: effectRainbow(); break;
  }
  strip.show();
}

// ============================================================================
// GITHUB OTA UPDATE FUNCTIONS
// ============================================================================

void checkForGitHubUpdate() {
  Serial.println("Checking for GitHub firmware update...");
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected for GitHub OTA check");
    return;
  }

  String latestVersion = fetchLatestVersion();
  if (latestVersion == "") {
    Serial.println("Failed to fetch latest version from GitHub");
    return;
  }

  Serial.println("Current Firmware Version: " + String(currentFirmwareVersion));
  Serial.println("Latest Firmware Version: " + latestVersion);

  if (latestVersion != currentFirmwareVersion) {
    Serial.println("New firmware available. Starting OTA update...");
    downloadAndApplyFirmware();
  } else {
    Serial.println("Device is up to date.");
  }
}

String fetchLatestVersion() {
  HTTPClient http;
  http.begin(GITHUB_VERSION_URL);

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String latestVersion = http.getString();
    latestVersion.trim();
    http.end();
    return latestVersion;
  } else {
    Serial.printf("Failed to fetch version. HTTP code: %d\n", httpCode);
    http.end();
    return "";
  }
}

void downloadAndApplyFirmware() {
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(GITHUB_FIRMWARE_URL);

  int httpCode = http.GET();
  Serial.printf("HTTP GET code for firmware: %d\n", httpCode);

  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    Serial.printf("Firmware size: %d bytes\n", contentLength);

    if (contentLength > 0) {
      strip.clear();
      strip.show();
      Serial.println("Turning off LEDs for OTA process...");

      WiFiClient* stream = http.getStreamPtr();
      if (startGitHubOTAUpdate(stream, contentLength)) {
        Serial.println("GitHub OTA update successful, restarting...");
        delay(2000);
        ESP.restart();
      } else {
        Serial.println("GitHub OTA update failed");
      }
    } else {
      Serial.println("Invalid firmware size for GitHub OTA");
    }
  } else {
    Serial.printf("Failed to fetch firmware from GitHub. HTTP code: %d\n", httpCode);
  }
  http.end();
}

bool startGitHubOTAUpdate(WiFiClient* client, int contentLength) {
  Serial.println("Initializing GitHub update...");
  if (!Update.begin(contentLength)) {
    Serial.printf("Update begin failed: %s\n", Update.errorString());
    return false;
  }

  Serial.println("Writing firmware...");
  size_t written = 0;
  int progress = 0;
  int lastProgress = 0;

  const unsigned long timeoutDuration = 10 * 1000;
  unsigned long lastDataTime = millis();

  while (written < contentLength) {
    if (client->available()) {
      uint8_t buffer[128];
      size_t len = client->read(buffer, sizeof(buffer));
      if (len > 0) {
        Update.write(buffer, len);
        written += len;
        lastDataTime = millis();

        progress = (written * 100) / contentLength;
        if (progress != lastProgress) {
          Serial.printf("Writing Progress: %d%%\r", progress);
          lastProgress = progress;
        }
      }
    }
    
    if (millis() - lastDataTime > timeoutDuration) {
      Serial.println("\nTimeout: No data received. Aborting update...");
      Update.abort();
      return false;
    }

    yield();
  }
  Serial.println("\nWriting complete");

  if (written != contentLength) {
    Serial.printf("Error: Write incomplete. Expected %d but got %d bytes\n", contentLength, written);
    Update.abort();
    return false;
  }

  if (!Update.end()) {
    Serial.printf("Error: Update end failed: %s\n", Update.errorString());
    return false;
  }

  Serial.println("Update successfully completed");
  return true;
}