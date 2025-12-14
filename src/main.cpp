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
#include <driver/adc.h>
#include <arduinoFFT.h>
#include <addons/TokenHelper.h>
#include <esp_crt_bundle.h>
#include "config.h"
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

// ============================================================================
// AUTO-CALIBRATION VARIABLES
// ============================================================================

volatile bool autoCalibrationEnabled = true;
volatile bool calibrationRequested = false;
unsigned long lastCalibrationTime = 0;
static const unsigned long AUTO_RECALIBRATION_INTERVAL = 300000;

// ============================================================================
// TIMER SETTINGS
// ============================================================================

char timerOnTime[6] = "09:00";
char timerOffTime[6] = "17:00";
bool timerEnabled = true;

// ============================================================================
// DEVICE CONFIGURATION
// ============================================================================

String deviceID;
String wifiSSID;
String wifiPassword;
int ledCount;
String basePath;

// ============================================================================
// GITHUB OTA CONFIGURATION
// ============================================================================

const char* GITHUB_FIRMWARE_URL = "https://github.com/majd-shwikani/Lumina-bin/releases/download/Lumina/firmware.bin";
const char* GITHUB_VERSION_URL = "https://raw.githubusercontent.com/majd-shwikani/Lumina-bin/refs/heads/main/version.txt";
const char* currentFirmwareVersion = "2.0.0";
const unsigned long UPDATE_CHECK_INTERVAL = 10 * 60 * 1000;
unsigned long lastUpdateCheck = 0;

// ============================================================================
// CONNECTION STATE TRACKING
// ============================================================================

unsigned long lastSuccessfulFirebaseTime = 0;
unsigned long lastWiFiAttempt = 0;
const unsigned long WIFI_RECONNECT_INTERVAL = 10000;
const unsigned long FIREBASE_RECONNECT_INTERVAL = 5000;

#define BUTTON_PIN 0

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

void initSPIFFS();
bool loadConfig();
bool shouldStartConfigPortal();
void connectToWiFi();
void setupTime();
void setupOTA();
void setupFirebase();
void readInitialFirebaseData();
void updateLEDs();
bool checkTimeMatch(const char* scheduledTime);
void updateTimerState(bool state);
void checkForGitHubUpdate();
String fetchLatestVersion();
bool startGitHubOTAUpdate(WiFiClient* client, int contentLength);
void downloadAndApplyFirmware();

void firebaseTask(void *parameter);
void ledTask(void *parameter);
void automationtask(void *parameter);
void sensorDataTask(void *parameter);
void timerTask(void *parameter);
void calibrationTask(void *parameter);
void streamCallback(FirebaseStream data);
void streamTimeoutCallback(bool timeout);

// ============================================================================
// SETUP FUNCTION
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  initSPIFFS();
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  if (shouldStartConfigPortal()) {
    Serial.println("No configuration found. Starting config portal...");
    startConfigPortal();
    return;
  }
  
  if (!loadConfig()) {
    Serial.println("Failed to load config, restarting...");
    delay(2000);
    ESP.restart();
    return;
  }
  
  Serial.println("\n=== Device Configuration ===");
  Serial.println("Device ID: " + deviceID);
  Serial.println("Base path: " + basePath);
  Serial.println("LED Count: " + String(ledCount));
  Serial.println("===========================\n");
  
  strip.updateLength(ledCount);
  strip.begin();
  strip.show();
  strip.setBrightness(100);
  
  esp_task_wdt_init(30, true);
  Wire.begin();
  
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
  
  Serial.println("All systems initialized!\n");
  
  // Firebase management task (core 0) - INCREASED STACK TO 20KB
  xTaskCreatePinnedToCore(firebaseTask, "FirebaseTask", 20000, NULL, 2, NULL, 0);

  // LED animation task (core 1)
  xTaskCreatePinnedToCore(ledTask, "LEDTask", 15000, NULL, 1, NULL, 1);

  // Sensor data reporting task (core 0) - INCREASED STACK TO 18KB
  xTaskCreatePinnedToCore(sensorDataTask, "SensorDataTask", 18000, NULL, 2, NULL, 0);

  // Light-based automation task (core 0)
  xTaskCreatePinnedToCore(automationtask, "AutomationTask", 4000, NULL, 0, NULL, 0);

  // Timer-based control task (core 0)
  xTaskCreatePinnedToCore(timerTask, "TimerTask", 8000, NULL, 1, NULL, 0);
  
  // Auto-calibration task (core 0) - INCREASED STACK TO 15KB
  xTaskCreatePinnedToCore(calibrationTask, "CalibrationTask", 15000, NULL, 1, NULL, 0);
  
  Serial.println("All tasks created successfully!");
  Serial.println("\nStarting auto-calibration in 3 seconds...");
  delay(3000);
  startAutoCalibration();
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  if (WiFi.status() == WL_CONNECTED && millis() - lastUpdateCheck > UPDATE_CHECK_INTERVAL) {
    lastUpdateCheck = millis();
    checkForGitHubUpdate();
  }
  vTaskDelay(100 / portTICK_PERIOD_MS);
}

// ============================================================================
// SPIFFS & CONFIGURATION
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
// WIFI & NETWORK
// ============================================================================

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  
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
      String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
      Serial.println("Start updating " + type);
      strip.clear();
      strip.show();
    })
    .onEnd([]() { Serial.println("\nOTA End"); })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("OTA Error[%u]: ", error);
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
// FIREBASE SETUP - IMPROVED SSL CONFIGURATION
// ============================================================================

void setupFirebase() {
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_SECRET;
  
  // CRITICAL: Improved timeout settings
  config.timeout.serverResponse = 10 * 1000;
  config.timeout.socketConnection = 10 * 1000;
  
  fbdoStream.setResponseSize(2048);
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);
  
  Serial.println("Connecting to Firebase...");
  delay(2000);
  
  readInitialFirebaseData();

  String streamPath = "/devices/" + deviceID;
  Serial.println("Stream path: " + streamPath);
  
  if (!Firebase.RTDB.beginStream(&fbdoStream, streamPath.c_str())) {
    Serial.printf("Stream begin error: %s\n", fbdoStream.errorReason().c_str());
  }
  
  Firebase.RTDB.setStreamCallback(&fbdoStream, streamCallback, streamTimeoutCallback);
  Serial.println("Firebase initialized with stream and initial data reading");
}

// ============================================================================
// FIREBASE DATA READING
// ============================================================================

void readInitialFirebaseData() {
  Serial.println("\nReading initial Firebase data for device: " + deviceID);
  
  String effectPath = basePath + "/effect";
  if (Firebase.RTDB.getInt(&fbdoUpload, effectPath.c_str())) {
    currentEffect = fbdoUpload.intData();
    Serial.printf("Initial effect: %d\n", currentEffect);
  } else {
    currentEffect = 0;
    Firebase.RTDB.setInt(&fbdoUpload, effectPath.c_str(), currentEffect);
  }
  
  String speedPath = basePath + "/speed";
  if (Firebase.RTDB.getInt(&fbdoUpload, speedPath.c_str())) {
    effectSpeed = fbdoUpload.intData();
    Serial.printf("Initial speed: %d\n", effectSpeed);
  } else {
    effectSpeed = 50;
    Firebase.RTDB.setInt(&fbdoUpload, speedPath.c_str(), effectSpeed);
  }
  
  String colorPath = basePath + "/color";
  if (Firebase.RTDB.getString(&fbdoUpload, colorPath.c_str())) {
    String colorStr = fbdoUpload.stringData();
    if (colorStr.length() == 6) {
      effectColor = strtoul(colorStr.c_str(), NULL, 16);
      Serial.printf("Initial color: %s\n", colorStr.c_str());
    }
  } else {
    effectColor = 0xFF0000;
    Firebase.RTDB.setString(&fbdoUpload, colorPath.c_str(), "FF0000");
  }
  
  String enabledPath = basePath + "/enabled";
  if (Firebase.RTDB.getBool(&fbdoUpload, enabledPath.c_str())) {
    stripEnabled = fbdoUpload.boolData();
    Serial.printf("Initial enabled state: %s\n", stripEnabled ? "true" : "false");
    if (!stripEnabled) {
      strip.clear();
      strip.show();
    }
  } else {
    stripEnabled = true;
    Firebase.RTDB.setBool(&fbdoUpload, enabledPath.c_str(), stripEnabled);
  }
  
  String autoDarknessPath = basePath + "/auto_darkness_control";
  if (Firebase.RTDB.getBool(&fbdoUpload, autoDarknessPath.c_str())) {
    autoDarknessControl = fbdoUpload.boolData();
    Serial.printf("Initial auto darkness control: %s\n", autoDarknessControl ? "true" : "false");
  } else {
    autoDarknessControl = true;
    Firebase.RTDB.setBool(&fbdoUpload, autoDarknessPath.c_str(), autoDarknessControl);
  }
  
  String luxThresholdPath = basePath + "/lux_threshold";
  if (Firebase.RTDB.getFloat(&fbdoUpload, luxThresholdPath.c_str())) {
    luxThreshold = fbdoUpload.floatData();
    Serial.printf("Initial lux threshold: %.2f\n", luxThreshold);
  } else {
    luxThreshold = 1.0;
    Firebase.RTDB.setFloat(&fbdoUpload, luxThresholdPath.c_str(), luxThreshold);
  }
  
  String timerOnPath = basePath + "/timer_on";
  if (Firebase.RTDB.getString(&fbdoUpload, timerOnPath.c_str())) {
    String timeStr = fbdoUpload.stringData();
    if (timeStr.length() == 5) {
      strncpy(timerOnTime, timeStr.c_str(), sizeof(timerOnTime));
      Serial.printf("Initial timer on time: %s\n", timerOnTime);
    }
  } else {
    strcpy(timerOnTime, "09:00");
    Firebase.RTDB.setString(&fbdoUpload, timerOnPath.c_str(), timerOnTime);
  }
  
  String timerOffPath = basePath + "/timer_off";
  if (Firebase.RTDB.getString(&fbdoUpload, timerOffPath.c_str())) {
    String timeStr = fbdoUpload.stringData();
    if (timeStr.length() == 5) {
      strncpy(timerOffTime, timeStr.c_str(), sizeof(timerOffTime));
      Serial.printf("Initial timer off time: %s\n", timerOffTime);
    }
  } else {
    strcpy(timerOffTime, "17:00");
    Firebase.RTDB.setString(&fbdoUpload, timerOffPath.c_str(), timerOffTime);
  }
  
  String timerEnabledPath = basePath + "/timer_enabled";
  if (Firebase.RTDB.getBool(&fbdoUpload, timerEnabledPath.c_str())) {
    timerEnabled = fbdoUpload.boolData();
    Serial.printf("Initial timer enabled: %s\n", timerEnabled ? "true" : "false");
  } else {
    timerEnabled = true;
    Firebase.RTDB.setBool(&fbdoUpload, timerEnabledPath.c_str(), timerEnabled);
  }
  
  String micTypePath = basePath + "/microphone_type";
  if (Firebase.RTDB.getInt(&fbdoUpload, micTypePath.c_str())) {
    int micType = fbdoUpload.intData();
    selectMicrophone(micType);
    activeMicrophone = micType;
    Serial.printf("Microphone type: %d\n", micType);
  } else {
    selectMicrophone(MIC_I2S_ICS43434);
    Firebase.RTDB.setInt(&fbdoUpload, micTypePath.c_str(), (int)MIC_I2S_ICS43434);
  }
  
  String autoCalibrationPath = basePath + "/auto_calibration_enabled";
  if (Firebase.RTDB.getBool(&fbdoUpload, autoCalibrationPath.c_str())) {
    autoCalibrationEnabled = fbdoUpload.boolData();
    Serial.printf("Auto-calibration: %s\n", autoCalibrationEnabled ? "enabled" : "disabled");
  } else {
    autoCalibrationEnabled = true;
    Firebase.RTDB.setBool(&fbdoUpload, autoCalibrationPath.c_str(), autoCalibrationEnabled);
  }
  
  String resetPath = basePath + "/reset";
  Firebase.RTDB.setBool(&fbdoUpload, resetPath.c_str(), false);
  
  String versionPath = basePath + "/version";
  if (Firebase.RTDB.setString(&fbdoUpload, versionPath.c_str(), currentFirmwareVersion)) {
    Serial.printf("Firmware version published: %s\n", currentFirmwareVersion);
  }
  
  Serial.println("Initial data read complete.\n");
}

// ============================================================================
// FIREBASE STREAM CALLBACK
// ============================================================================

void streamCallback(FirebaseStream data) {
  Serial.printf("Stream: %s = %s\n", data.dataPath().c_str(), data.stringData().c_str());

  String dataPath = data.dataPath().c_str();

  if (dataPath == "/effect") {
    currentEffect = data.intData();
    Serial.printf("Effect changed to: %d\n", currentEffect);
  }
  else if (dataPath == "/speed") {
    effectSpeed = data.intData();
    Serial.printf("Speed changed to: %d\n", effectSpeed);
  }
  else if (dataPath == "/color") {
    String colorStr = data.stringData();
    if (colorStr.length() == 6) {
      effectColor = strtoul(colorStr.c_str(), NULL, 16);
      Serial.printf("Color changed to: %s\n", colorStr.c_str());
    }
  }
  else if (dataPath == "/lux_threshold") {
    luxThreshold = data.floatData();
    Serial.printf("Lux threshold changed to: %.2f\n", luxThreshold);
  }
  else if (dataPath == "/enabled") {
    stripEnabled = data.boolData();
    turnedOffByDarkness = false;
    Serial.printf("Strip %s\n", stripEnabled ? "enabled" : "disabled");
    if (!stripEnabled) {
      strip.clear();
      strip.show();
    }
  }
  else if (dataPath == "/timer_enabled") {
    timerEnabled = data.boolData();
    Serial.printf("Timer %s\n", timerEnabled ? "enabled" : "disabled");
  }
  else if (dataPath == "/auto_darkness_control") {
    autoDarknessControl = data.boolData();
    Serial.printf("Auto darkness control %s\n", autoDarknessControl ? "enabled" : "disabled");
  }
  else if (dataPath == "/timer_on") {
    String timeStr = data.stringData();
    if (timeStr.length() == 5) {
      strncpy(timerOnTime, timeStr.c_str(), sizeof(timerOnTime));
      Serial.printf("Timer ON time: %s\n", timerOnTime);
    }
  }
  else if (dataPath == "/timer_off") {
    String timeStr = data.stringData();
    if (timeStr.length() == 5) {
      strncpy(timerOffTime, timeStr.c_str(), sizeof(timerOffTime));
      Serial.printf("Timer OFF time: %s\n", timerOffTime);
    }
  }
  else if (dataPath == "/microphone_type") {
    int micType = data.intData();
    selectMicrophone(micType);
    activeMicrophone = micType;
    Serial.printf("Microphone switched to: %d\n", micType);
  }
  else if (dataPath == "/auto_calibration_enabled") {
    autoCalibrationEnabled = data.boolData();
    Serial.printf("Auto-calibration: %s\n", autoCalibrationEnabled ? "enabled" : "disabled");
  }
  else if (dataPath == "/request_calibration" && data.boolData() == true) {
    Serial.println("Calibration requested via Firebase");
    calibrationRequested = true;
    Firebase.RTDB.setBool(&fbdoUpload, (basePath + "/request_calibration").c_str(), false);
  }
  else if (dataPath == "/reset" && data.boolData() == true) {
    Serial.println("Reset command received - restarting...");
    if (SPIFFS.exists("/config.json")) {
      SPIFFS.remove("/config.json");
    }
    ESP.restart();
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
// FREERTOS TASKS - IMPROVED WITH BETTER ERROR HANDLING
// ============================================================================

void firebaseTask(void *parameter) {
  unsigned long lastReconnectAttempt = 0;
  
  for(;;) {
    esp_task_wdt_reset();
    
    if (WiFi.status() == WL_CONNECTED) {
      ArduinoOTA.handle();
      
      if (!Firebase.ready()) {
        if (millis() - lastReconnectAttempt > FIREBASE_RECONNECT_INTERVAL) {
          Serial.println("Firebase not ready, checking connection...");
          firebaseConnected = false;
          lastReconnectAttempt = millis();
          Firebase.reconnectNetwork(true);
          delay(500);
        }
      } else {
        firebaseConnected = true;
        lastSuccessfulFirebaseTime = millis();
      }
    } else {
      firebaseConnected = false;
      
      if (millis() - lastWiFiAttempt > WIFI_RECONNECT_INTERVAL) {
        Serial.println("WiFi disconnected, attempting reconnect...");
        WiFi.reconnect();
        lastWiFiAttempt = millis();
      }
    }
    
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}

void ledTask(void *parameter) {
  for(;;) {
    updateLEDs();
    vTaskDelay(effectSpeed / portTICK_PERIOD_MS);
  }
}

void automationtask(void *parameter) {
  const TickType_t xDelay = 100 / portTICK_PERIOD_MS;

  for(;;) {
    esp_task_wdt_reset();
    
    if (sensorAvailable) {
      updateSensorData();

      if (autoDarknessControl && shouldTurnOffDueToDarkness()) {
        if (stripEnabled) {
          stripEnabled = false;
          turnedOffByDarkness = true;
          strip.clear();
          strip.show();
          Serial.println("Darkness detected - turning LEDs off automatically");
        }
      } 
      else if (autoDarknessControl && turnedOffByDarkness && !shouldTurnOffDueToDarkness()) {
        stripEnabled = true;
        turnedOffByDarkness = false;
        Serial.println("Light detected - turning LEDs back on automatically");
      }
    }

    vTaskDelay(xDelay);
  }
}

void sensorDataTask(void *parameter) {
  const TickType_t xDelay = 2000 / portTICK_PERIOD_MS;
  
  for(;;) {
    esp_task_wdt_reset();
    
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
      
      esp_task_wdt_reset();
      
      String noiseFloorPath = basePath + "/audio/noise_floor";
      Firebase.RTDB.setFloat(&fbdoUpload, noiseFloorPath.c_str(), noiseFloor);
      
      String peakLevelPath = basePath + "/audio/peak_level";
      Firebase.RTDB.setFloat(&fbdoUpload, peakLevelPath.c_str(), peakLevel);
      
      String dynamicRangePath = basePath + "/audio/dynamic_range";
      Firebase.RTDB.setFloat(&fbdoUpload, dynamicRangePath.c_str(), dynamicRange);
    }
    
    vTaskDelay(xDelay);
  }
}

void timerTask(void *parameter) {
  const TickType_t xDelay = 1000 / portTICK_PERIOD_MS;
  bool lastOnTriggered = false;
  bool lastOffTriggered = false;
  
  for(;;) {
    esp_task_wdt_reset();
    
    if (firebaseConnected && timerEnabled) {
      if (checkTimeMatch(timerOnTime)) {
        if (!lastOnTriggered) {
          Serial.println("Timer ON triggered");
          updateTimerState(true);
          lastOnTriggered = true;
        }
      } else {
        lastOnTriggered = false;
      }
      
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

void calibrationTask(void *parameter) {
  const TickType_t xDelay = 100 / portTICK_PERIOD_MS;
  
  for(;;) {
    esp_task_wdt_reset();
    
    if (autoCalibrationEnabled && firebaseConnected) {
      if (calibrationRequested) {
        startAutoCalibration();
        calibrationRequested = false;
      }
      
      if (millis() - lastCalibrationTime > AUTO_RECALIBRATION_INTERVAL) {
        Serial.println("Performing periodic re-calibration...");
        startAutoCalibration();
        lastCalibrationTime = millis();
      }
      
      String calibStatusPath = basePath + "/audio/calibration_active";
      Firebase.RTDB.setBool(&fbdoUpload, calibStatusPath.c_str(), calibrationActive);
      
      String calibCompletePath = basePath + "/audio/calibration_complete";
      Firebase.RTDB.setBool(&fbdoUpload, calibCompletePath.c_str(), isCalibrationComplete());
      
      String micSensPath = basePath + "/audio/mic_sensitivity";
      Firebase.RTDB.setFloat(&fbdoUpload, micSensPath.c_str(), micSensitivity);
      
      String freqThreshPath = basePath + "/audio/frequency_threshold";
      Firebase.RTDB.setFloat(&fbdoUpload, freqThreshPath.c_str(), frequencyThreshold);
      
      String beatThreshPath = basePath + "/audio/beat_threshold";
      Firebase.RTDB.setFloat(&fbdoUpload, beatThreshPath.c_str(), beatThreshold);
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
  if (autoDarknessControl && sensorAvailable && shouldTurnOffDueToDarkness()) {
    strip.clear();
    strip.show();
    return;
  }
  
  if (!stripEnabled) {
    strip.clear();
    strip.show();
    return;
  }
  
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