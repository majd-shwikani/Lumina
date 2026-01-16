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
#include "mqtt_integration.h"
#include <ld2410.h>
#include <esp_system.h>
#include <rom/rtc.h>

// Firebase token helper
#include <addons/TokenHelper.h>

// Configuration
#include "config.h"

// Configuration portal
#include "web_config_portal.h"

// ============================================================================
// DEBUGGING AND SYSTEM MONITORING
// ============================================================================

unsigned long lastSystemStatsReport = 0;
const unsigned long SYSTEM_STATS_INTERVAL = 10000; // 10 seconds

// Task stack high water marks
UBaseType_t firebaseTaskStack = 0;
UBaseType_t ledTaskStack = 0;
UBaseType_t automationTaskStack = 0;
UBaseType_t sensorTaskStack = 0;
UBaseType_t timerTaskStack = 0;
UBaseType_t mqttTaskStack = 0;

// Crash detection variables
unsigned long lastLoopTime = 0;
unsigned long loopCounter = 0;
bool systemHealthy = true;

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
volatile bool manuallyTurnedOff = false;

// ============================================================================
// AUTOMATION CONTROL VARIABLES
// ============================================================================

unsigned long lastStateChangeTime = 0;
const unsigned long STATE_CHANGE_DEBOUNCE = 5000;
float luxHysteresis = 5.0;
unsigned long luxReadDelayAfterChange = 0;

// ============================================================================
// PRESENCE DETECTION VARIABLES
// ============================================================================

ld2410 radar;
volatile bool lastPresence = false;
volatile bool presenceDetectionEnabled = true;
volatile bool lastPresenceState = false;
unsigned long lastPresenceReport = 0;
static const int RADAR_RX_PIN = 16;
static const int RADAR_TX_PIN = 17;

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

const char* currentFirmwareVersion = "1.1.4";
const unsigned long UPDATE_CHECK_INTERVAL = 10 * 60 * 1000;
unsigned long lastUpdateCheck = 0;

// ============================================================================
// BUTTON PRESS DETECTION
// ============================================================================

#define BUTTON_PIN 19
unsigned long buttonPressStart = 0;
bool buttonActive = false;

// ============================================================================
// SYSTEM MONITORING FUNCTIONS
// ============================================================================

const char* getResetReason(int cpu) {
  RESET_REASON reason = rtc_get_reset_reason(cpu);
  
  switch (reason) {
    case 1:  return "POWERON_RESET - Vbat power on reset";
    case 3:  return "SW_RESET - Software reset digital core";
    case 4:  return "OWDT_RESET - Legacy watch dog reset digital core";
    case 5:  return "DEEPSLEEP_RESET - Deep Sleep reset digital core";
    case 6:  return "SDIO_RESET - Reset by SLC module, reset digital core";
    case 7:  return "TG0WDT_SYS_RESET - Timer Group0 Watch dog reset digital core";
    case 8:  return "TG1WDT_SYS_RESET - Timer Group1 Watch dog reset digital core";
    case 9:  return "RTCWDT_SYS_RESET - RTC Watch dog Reset digital core";
    case 10: return "INTRUSION_RESET - Instrusion tested to reset CPU";
    case 11: return "TGWDT_CPU_RESET - Time Group reset CPU";
    case 12: return "SW_CPU_RESET - Software reset CPU";
    case 13: return "RTCWDT_CPU_RESET - RTC Watch dog Reset CPU";
    case 14: return "EXT_CPU_RESET - for APP CPU, reseted by PRO CPU";
    case 15: return "RTCWDT_BROWN_OUT_RESET - Reset when the vdd voltage is not stable";
    case 16: return "RTCWDT_RTC_RESET - RTC Watch dog reset digital core and rtc module";
    default: return "NO_MEAN - Unknown reset reason";
  }
}

void printSystemStats() {
  Serial.println("\n╔════════════════════════════════════════════════════════════════╗");
  Serial.println("║              SYSTEM HEALTH & DIAGNOSTICS REPORT                ║");
  Serial.println("╠════════════════════════════════════════════════════════════════╣");
  
  // Reset Reason
  Serial.println("║ RESET INFORMATION:");
  Serial.printf("║   CPU0 Reset Reason: %s\n", getResetReason(0));
  Serial.printf("║   CPU1 Reset Reason: %s\n", getResetReason(1));
  Serial.println("║");
  
  // Memory Stats
  Serial.println("║ MEMORY STATUS:");
  Serial.printf("║   Free Heap: %d bytes (%.1f KB)\n", ESP.getFreeHeap(), ESP.getFreeHeap() / 1024.0);
  Serial.printf("║   Heap Size: %d bytes (%.1f KB)\n", ESP.getHeapSize(), ESP.getHeapSize() / 1024.0);
  Serial.printf("║   Min Free Heap: %d bytes (%.1f KB)\n", ESP.getMinFreeHeap(), ESP.getMinFreeHeap() / 1024.0);
  Serial.printf("║   Free PSRAM: %d bytes (%.1f KB)\n", ESP.getFreePsram(), ESP.getFreePsram() / 1024.0);
  Serial.printf("║   Heap Usage: %.1f%%\n", 100.0 - (ESP.getFreeHeap() * 100.0 / ESP.getHeapSize()));
  Serial.println("║");
  
  // Task Stack Usage
  Serial.println("║ TASK STACK HIGH WATER MARKS (bytes remaining):");
  Serial.printf("║   Firebase Task: %d bytes\n", firebaseTaskStack * sizeof(StackType_t));
  Serial.printf("║   LED Task: %d bytes\n", ledTaskStack * sizeof(StackType_t));
  Serial.printf("║   Automation Task: %d bytes\n", automationTaskStack * sizeof(StackType_t));
  Serial.printf("║   Sensor Task: %d bytes\n", sensorTaskStack * sizeof(StackType_t));
  Serial.printf("║   Timer Task: %d bytes\n", timerTaskStack * sizeof(StackType_t));
  Serial.printf("║   MQTT Task: %d bytes\n", mqttTaskStack * sizeof(StackType_t));
  Serial.println("║");
  
  // System Status
  Serial.println("║ SYSTEM STATUS:");
  Serial.printf("║   Uptime: %lu seconds (%.1f minutes)\n", millis() / 1000, millis() / 60000.0);
  Serial.printf("║   CPU Frequency: %d MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("║   Flash Size: %d bytes (%.1f MB)\n", ESP.getFlashChipSize(), ESP.getFlashChipSize() / (1024.0 * 1024.0));
  Serial.printf("║   Flash Speed: %d Hz\n", ESP.getFlashChipSpeed());
  Serial.printf("║   SDK Version: %s\n", ESP.getSdkVersion());
  Serial.printf("║   Chip Model: %s\n", ESP.getChipModel());
  Serial.printf("║   Chip Revision: %d\n", ESP.getChipRevision());
  Serial.printf("║   Loop Counter: %lu\n", loopCounter);
  Serial.println("║");
  
  // Network Status
  Serial.println("║ NETWORK STATUS:");
  Serial.printf("║   WiFi Connected: %s\n", WiFi.status() == WL_CONNECTED ? "YES" : "NO");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("║   IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("║   RSSI: %d dBm\n", WiFi.RSSI());
    Serial.printf("║   MAC Address: %s\n", WiFi.macAddress().c_str());
  }
  Serial.printf("║   Firebase Connected: %s\n", firebaseConnected ? "YES" : "NO");
  Serial.printf("║   MQTT Connected: %s\n", mqttConnected ? "YES" : "NO");
  Serial.println("║");
  
  // Device Status
  Serial.println("║ DEVICE STATUS:");
  Serial.printf("║   Device ID: %s\n", deviceID.c_str());
  Serial.printf("║   LED Count: %d\n", ledCount);
  Serial.printf("║   Strip Enabled: %s\n", stripEnabled ? "YES" : "NO");
  Serial.printf("║   Current Effect: %d\n", currentEffect);
  Serial.printf("║   Effect Speed: %d ms\n", effectSpeed);
  Serial.printf("║   Brightness: %d\n", strip.getBrightness());
  Serial.printf("║   Manually Off: %s\n", manuallyTurnedOff ? "YES" : "NO");
  Serial.println("║");
  
  // Sensor Status
  Serial.println("║ SENSOR STATUS:");
  Serial.printf("║   Light Sensor Available: %s\n", sensorAvailable ? "YES" : "NO");
  if (sensorAvailable) {
    Serial.printf("║   Current Lux: %.2f\n", currentLux);
    Serial.printf("║   Lux Threshold: %.2f\n", luxThreshold);
  }
  Serial.printf("║   Presence Detection: %s\n", presenceDetectionEnabled ? "ENABLED" : "DISABLED");
  Serial.printf("║   Presence Detected: %s\n", lastPresence ? "YES" : "NO");
  Serial.printf("║   Auto Darkness Control: %s\n", autoDarknessControl ? "ENABLED" : "DISABLED");
  Serial.println("║");
  
  // Audio Status
  Serial.println("║ AUDIO STATUS:");
  Serial.printf("║   Calibration Complete: %s\n", calibrationComplete ? "YES" : "NO");
  Serial.printf("║   Noise Floor: %.2f\n", noiseFloor);
  Serial.printf("║   Gain Multiplier: %.2f\n", gainMultiplier);
  Serial.printf("║   Global Audio Level: %.2f\n", globalAudioLevel);
  Serial.printf("║   Beat Detected: %s\n", beatDetected ? "YES" : "NO");
  Serial.println("║");
  
  // Warning Checks
  Serial.println("║ HEALTH WARNINGS:");
  bool warningsFound = false;
  
  if (ESP.getFreeHeap() < 30000) {
    Serial.println("║   ⚠️  WARNING: Low heap memory!");
    warningsFound = true;
  }
  if (firebaseTaskStack < 500) {
    Serial.println("║   ⚠️  WARNING: Firebase task stack critically low!");
    warningsFound = true;
  }
  if (ledTaskStack < 500) {
    Serial.println("║   ⚠️  WARNING: LED task stack critically low!");
    warningsFound = true;
  }
  if (automationTaskStack < 200) {
    Serial.println("║   ⚠️  WARNING: Automation task stack critically low!");
    warningsFound = true;
  }
  if (sensorTaskStack < 500) {
    Serial.println("║   ⚠️  WARNING: Sensor task stack critically low!");
    warningsFound = true;
  }
  if (timerTaskStack < 300) {
    Serial.println("║   ⚠️  WARNING: Timer task stack critically low!");
    warningsFound = true;
  }
  if (mqttTaskStack < 500) {
    Serial.println("║   ⚠️  WARNING: MQTT task stack critically low!");
    warningsFound = true;
  }
  if (!firebaseConnected) {
    Serial.println("║   ⚠️  WARNING: Firebase disconnected!");
    warningsFound = true;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("║   ⚠️  WARNING: WiFi disconnected!");
    warningsFound = true;
  }
  if (!warningsFound) {
    Serial.println("║   ✅ All systems nominal");
  }
  
  Serial.println("╚════════════════════════════════════════════════════════════════╝\n");
}

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
void createDefaultFirebaseData();
void handleFirebaseData();
void streamCallback(FirebaseStream data);
void streamTimeoutCallback(bool timeout);
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

// ============================================================================
// SETUP FUNCTION
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════════════════════════════════════╗");
  Serial.println("║                    LUMINA LED CONTROLLER                       ║");
  Serial.println("║                     Firmware v" + String(currentFirmwareVersion) + "                          ║");
  Serial.println("╚════════════════════════════════════════════════════════════════╝");
  Serial.println();
  
  // Print reset reason immediately
  Serial.println("🔄 BOOT INFORMATION:");
  Serial.printf("   CPU0 Reset Reason: %s\n", getResetReason(0));
  Serial.printf("   CPU1 Reset Reason: %s\n", getResetReason(1));
  Serial.println();
  
  // Initialize SPIFFS
  Serial.println("📁 Initializing SPIFFS...");
  initSPIFFS();
  
  // Configure button input
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.println("🔘 Button configured on pin " + String(BUTTON_PIN));

  // Check if config portal is needed
  if (shouldStartConfigPortal()) {
    Serial.println("⚙️  No configuration found. Starting config portal...");
    startConfigPortal();
    return;
  }
  
  // Load configuration from SPIFFS
  Serial.println("📖 Loading configuration...");
  if (!loadConfig()) {
    Serial.println("❌ Failed to load config, restarting...");
    delay(3000);
    ESP.restart();
    return;
  }
  
  // Print loaded configuration
  Serial.println("\n╔════════════════════════════════════════════════════════════════╗");
  Serial.println("║                  DEVICE CONFIGURATION                          ║");
  Serial.println("╠════════════════════════════════════════════════════════════════╣");
  Serial.printf("║   Device ID: %-48s ║\n", deviceID.c_str());
  Serial.printf("║   Base Path: %-47s ║\n", basePath.c_str());
  Serial.printf("║   LED Count: %-47d ║\n", ledCount);
  Serial.printf("║   WiFi SSID: %-47s ║\n", wifiSSID.c_str());
  Serial.println("╚════════════════════════════════════════════════════════════════╝\n");
  
  // Initialize NeoPixel strip with configured count
  Serial.println("💡 Initializing LED strip...");
  strip.updateLength(ledCount);
  strip.begin();
  strip.show();
  strip.setBrightness(100);
  Serial.printf("   ✅ %d LEDs initialized\n", ledCount);
  
  // Initialize watchdog timer (30s timeout)
  Serial.println("⏱️  Initializing watchdog timer (30s)...");
  esp_task_wdt_init(30, true);
  Serial.println("   ✅ Watchdog configured");
  
  // Initialize I2C for light sensor
  Serial.println("🔌 Initializing I2C bus...");
  Wire.begin();
  Serial.println("   ✅ I2C ready");
  
  // ========== SETUP SEQUENCE ==========
  
  Serial.println("\n🚀 INITIALIZING SYSTEMS:\n");
  
  Serial.println("📡 [1/9] Connecting to WiFi...");
  connectToWiFi();
  Serial.println("      ✅ WiFi initialized");
  
  Serial.println("🕐 [2/9] Synchronizing time...");
  setupTime();
  Serial.println("      ✅ Time synchronized");
  
  Serial.println("🔄 [3/9] Enabling OTA updates...");
  setupOTA();
  Serial.println("      ✅ OTA enabled");
  
  Serial.println("☀️  [4/9] Setting up light sensor...");
  setupVEML7700();
  if (sensorAvailable) {
    Serial.println("      ✅ Light sensor initialized");
  } else {
    Serial.println("      ⚠️  Light sensor not found");
  }
  
  Serial.println("🔥 [5/9] Connecting to Firebase...");
  setupFirebase();
  Serial.println("      ✅ Firebase initialized");
  
  Serial.println("🎤 [6/9] Initializing audio processing...");
  setupFrequencyDetection();
  Serial.println("      ✅ Frequency detection initialized");
  Serial.println("      🎵 Starting microphone calibration...");
  
  Serial.println("📨 [7/9] Setting up MQTT...");
  setupMQTT();
  if (mqttConnected) {
    Serial.println("      ✅ MQTT initialized");
  } else {
    Serial.println("      ⚠️  MQTT not connected");
  }
  
  Serial.println("📡 [8/9] Initializing radar sensor...");
  Serial1.begin(256000, SERIAL_8N1, RADAR_RX_PIN, RADAR_TX_PIN);
  if (radar.begin(Serial1)) {
    Serial.println("      ✅ LD2410 radar initialized");
  } else {
    Serial.println("      ⚠️  LD2410 radar initialization failed");
  }
  radar.debug(Serial);
  
  Serial.println("⚙️  [9/9] Creating FreeRTOS tasks...");
  
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
  Serial.println("      ✅ Firebase task created (Core 0, 15KB stack)");

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
  Serial.println("      ✅ LED task created (Core 1, 15KB stack)");

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
  Serial.println("      ✅ Sensor task created (Core 0, 15KB stack)");

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
  Serial.println("      ✅ Automation task created (Core 0, 4KB stack)");

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
  Serial.println("      ✅ Timer task created (Core 0, 8KB stack)");

  // MQTT management task (core 0)
  xTaskCreatePinnedToCore(
    mqttTask,
    "MQTTTask",
    15000,
    NULL,
    1,
    NULL,
    0
  );
  Serial.println("      ✅ MQTT task created (Core 0, 15KB stack)");
  
  Serial.println("\n╔════════════════════════════════════════════════════════════════╗");
  Serial.println("║              🎉 ALL SYSTEMS INITIALIZED 🎉                     ║");
  Serial.println("║         System monitoring active every 10 seconds              ║");
  Serial.println("╚════════════════════════════════════════════════════════════════╝\n");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  unsigned long loopStart = millis();
  loopCounter++;
  
  // Check for 7-second button press to reset config
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (!buttonActive) {
      buttonActive = true;
      buttonPressStart = millis();
      Serial.println("\n🔘 Button pressed - hold for 7 seconds to reset config");
    }
    
    // Check if held for 7 seconds
    if (millis() - buttonPressStart > 7000) {
      Serial.println("⚠️  7-second button press detected - resetting configuration...");
      
      if (SPIFFS.exists("/config.json")) {
        SPIFFS.remove("/config.json");
        Serial.println("✅ Configuration file deleted");
      }
      
      Serial.println("🔄 Restarting to enter config portal...");
      delay(1000);
      ESP.restart();
    }
  } else {
    buttonActive = false;
  }
  
  // Print system stats every 10 seconds
  if (millis() - lastSystemStatsReport > SYSTEM_STATS_INTERVAL) {
    lastSystemStatsReport = millis();
    
    // Update stack high water marks for all tasks
    firebaseTaskStack = uxTaskGetStackHighWaterMark(NULL);
    
    printSystemStats();
  }
  
  // Check for GitHub OTA updates periodically
  if (WiFi.status() == WL_CONNECTED && millis() - lastUpdateCheck > UPDATE_CHECK_INTERVAL) {
    lastUpdateCheck = millis();
    Serial.println("🔍 Checking for firmware updates...");
    checkForGitHubUpdate();
  }

  // Monitor loop health
  lastLoopTime = millis();
  unsigned long loopDuration = lastLoopTime - loopStart;
  
  if (loopDuration > 1000) {
    Serial.printf("⚠️  WARNING: Loop took %lu ms (expected <100ms)\n", loopDuration);
  }
  
  vTaskDelay(100 / portTICK_PERIOD_MS);
}

// ============================================================================
// SPIFFS & CONFIGURATION FUNCTIONS
// ============================================================================

void initSPIFFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println("❌ SPIFFS mount failed");
    return;
  }
  Serial.println("✅ SPIFFS mounted successfully");
}

bool loadConfig() {
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("❌ Failed to open config file");
    return false;
  }
  
  size_t size = configFile.size();
  Serial.printf("📄 Config file size: %d bytes\n", size);
  
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  configFile.close();
  
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, buf.get());
  if (error) {
    Serial.printf("❌ Failed to parse config file: %s\n", error.c_str());
    return false;
  }
  
  wifiSSID = doc["wifi_ssid"].as<String>();
  wifiPassword = doc["wifi_password"].as<String>();
  deviceID = doc["device_id"].as<String>();
  ledCount = doc["num_leds"];
  
  basePath = "/devices/" + deviceID;
  
  Serial.println("✅ Configuration loaded successfully");
  
  return true;
}

bool shouldStartConfigPortal() {
  return !SPIFFS.exists("/config.json");
}

// ============================================================================
// WIFI & NETWORK FUNCTIONS
// ============================================================================

void connectToWiFi() {
  Serial.printf("   Connecting to: %s\n", wifiSSID.c_str());
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  
  unsigned long startAttemptTime = millis();
  int dots = 0;
  
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 30000) {
    delay(500);
    Serial.print(".");
    dots++;
    if (dots % 60 == 0) Serial.println();
  }
  
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("   ✅ Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("   Signal Strength: %d dBm\n", WiFi.RSSI());
  } else {
    Serial.println("   ❌ Failed to connect to WiFi");
  }
}

void setupTime() {
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  
  Serial.print("   Synchronizing");
  struct tm timeinfo;
  unsigned long startTime = millis();
  int dots = 0;
  
  while (!getLocalTime(&timeinfo) && millis() - startTime < 10000) {
    Serial.print(".");
    dots++;
    if (dots % 60 == 0) Serial.println();
    delay(1000);
  }
  
  Serial.println();
  
  if (getLocalTime(&timeinfo)) {
    Serial.print("   ✅ Time: ");
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  } else {
    Serial.println("   ⚠️  Failed to synchronize time");
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
      Serial.println("🔄 OTA Update Start: " + type);
      strip.clear();
      strip.show();
    })
    .onEnd([]() {
      Serial.println("\n✅ OTA Update Complete");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("   Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("❌ OTA Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();
  Serial.printf("   Hostname: %s\n", hostname.c_str());
}

// ============================================================================
// FIREBASE SETUP & CONFIGURATION
// ============================================================================

void setupFirebase() {
  Serial.println("   Configuring Firebase...");
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_SECRET;
  config.timeout.serverResponse = 10 * 1000;
  
  fbdoStream.setResponseSize(2048);
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);
  
  Serial.println("   Waiting for connection...");
  delay(1000);
  
  readInitialFirebaseData();

  String streamPath = "/devices/" + deviceID;
  Serial.printf("   Stream path: %s\n", streamPath.c_str());
  
  if (!Firebase.RTDB.beginStream(&fbdoStream, streamPath.c_str())) {
    Serial.printf("   ❌ Stream begin error: %s\n", fbdoStream.errorReason().c_str());
  } else {
    Serial.println("   ✅ Stream initialized");
  }
  
  Firebase.RTDB.setStreamCallback(&fbdoStream, streamCallback, streamTimeoutCallback);
}

void readInitialFirebaseData() {
  Serial.println("   📖 Reading initial Firebase data...");
  
  String effectPath = basePath + "/effect";
  if (Firebase.RTDB.getInt(&fbdoUpload, effectPath.c_str())) {
    currentEffect = fbdoUpload.intData();
    Serial.printf("      Effect: %d\n", currentEffect);
  } else {
    Serial.printf("      ⚠️  Failed to read effect: %s\n", fbdoUpload.errorReason().c_str());
    currentEffect = 0;
    Firebase.RTDB.setInt(&fbdoUpload, effectPath.c_str(), currentEffect);
  }
  
  String speedPath = basePath + "/speed";
  if (Firebase.RTDB.getInt(&fbdoUpload, speedPath.c_str())) {
    effectSpeed = fbdoUpload.intData();
    Serial.printf("      Speed: %d\n", effectSpeed);
  } else {
    Serial.printf("      ⚠️  Failed to read speed: %s\n", fbdoUpload.errorReason().c_str());
    effectSpeed = 50;
    Firebase.RTDB.setInt(&fbdoUpload, speedPath.c_str(), effectSpeed);
  }
  
  String colorPath = basePath + "/color";
  if (Firebase.RTDB.getString(&fbdoUpload, colorPath.c_str())) {
    String colorStr = fbdoUpload.stringData();
    if (colorStr.length() == 6) {
      effectColor = strtoul(colorStr.c_str(), NULL, 16);
      Serial.printf("      Color: %s\n", colorStr.c_str());
    }
  } else {
    Serial.printf("      ⚠️  Failed to read color: %s\n", fbdoUpload.errorReason().c_str());
    effectColor = 0xFF0000;
    Firebase.RTDB.setString(&fbdoUpload, colorPath.c_str(), "FF0000");
  }
  
String enabledPath = basePath + "/enabled";
if (Firebase.RTDB.getBool(&fbdoUpload, enabledPath.c_str())) {
    stripEnabled = fbdoUpload.boolData();
    Serial.printf("      Enabled: %s\n", stripEnabled ? "true" : "false");
    
    // CRITICAL FIX: If Firebase says disabled, set manuallyTurnedOff
    if (!stripEnabled) {
        manuallyTurnedOff = true;  // Add this line
        strip.clear();
        strip.show();
    }
} else {
    Serial.printf("      ⚠️  Failed to read enabled state: %s\n", fbdoUpload.errorReason().c_str());
    stripEnabled = true;
    Firebase.RTDB.setBool(&fbdoUpload, enabledPath.c_str(), stripEnabled);
}
  
  String autoDarknessPath = basePath + "/auto_darkness_control";
  if (Firebase.RTDB.getBool(&fbdoUpload, autoDarknessPath.c_str())) {
    autoDarknessControl = fbdoUpload.boolData();
    Serial.printf("      Auto Darkness: %s\n", autoDarknessControl ? "true" : "false");
  } else {
    autoDarknessControl = true;
    Firebase.RTDB.setBool(&fbdoUpload, autoDarknessPath.c_str(), autoDarknessControl);
  }
  
  String presenceEnabledPath = basePath + "/presence_detection_enabled";
  if (Firebase.RTDB.getBool(&fbdoUpload, presenceEnabledPath.c_str())) {
    presenceDetectionEnabled = fbdoUpload.boolData();
    Serial.printf("      Presence Detection: %s\n", presenceDetectionEnabled ? "true" : "false");
  } else {
    presenceDetectionEnabled = true;
    Firebase.RTDB.setBool(&fbdoUpload, presenceEnabledPath.c_str(), presenceDetectionEnabled);
  }
  
  String luxThresholdPath = basePath + "/lux_threshold";
  if (Firebase.RTDB.getFloat(&fbdoUpload, luxThresholdPath.c_str())) {
    luxThreshold = fbdoUpload.floatData();
    Serial.printf("      Lux Threshold: %.2f\n", luxThreshold);
  } else {
    luxThreshold = 1.0;
    Firebase.RTDB.setFloat(&fbdoUpload, luxThresholdPath.c_str(), luxThreshold);
  }
  
  String timerOnPath = basePath + "/timer_on";
  if (Firebase.RTDB.getString(&fbdoUpload, timerOnPath.c_str())) {
    String timeStr = fbdoUpload.stringData();
    if (timeStr.length() == 5) {
      strncpy(timerOnTime, timeStr.c_str(), sizeof(timerOnTime));
      Serial.printf("      Timer ON: %s\n", timerOnTime);
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
      Serial.printf("      Timer OFF: %s\n", timerOffTime);
    }
  } else {
    strcpy(timerOffTime, "17:00");
    Firebase.RTDB.setString(&fbdoUpload, timerOffPath.c_str(), timerOffTime);
  }
  
  String timerEnabledPath = basePath + "/timer_enabled";
  if (Firebase.RTDB.getBool(&fbdoUpload, timerEnabledPath.c_str())) {
    timerEnabled = fbdoUpload.boolData();
    Serial.printf("      Timer Enabled: %s\n", timerEnabled ? "true" : "false");
  } else {
    timerEnabled = true;
    Firebase.RTDB.setBool(&fbdoUpload, timerEnabledPath.c_str(), timerEnabled);
  }
  
  String resetPath = basePath + "/reset";
  Firebase.RTDB.setBool(&fbdoUpload, resetPath.c_str(), false);
  
  String micCalibPath = basePath + "/mic_calibration";
  Firebase.RTDB.setBool(&fbdoUpload, micCalibPath.c_str(), false);
  
  String mqttEnabledPath = basePath + "/mqtt/enabled";
  if (!Firebase.RTDB.getBool(&fbdoUpload, mqttEnabledPath.c_str())) {
    Serial.println("      Creating default MQTT configuration...");
    
    String mqttBasePath = basePath + "/mqtt";
    delay(500);
    
    Firebase.RTDB.setString(&fbdoUpload, (mqttBasePath + "/broker_address").c_str(), "192.168.1.100");
    delay(200);
    Firebase.RTDB.setInt(&fbdoUpload, (mqttBasePath + "/broker_port").c_str(), 1883);
    delay(200);
    Firebase.RTDB.setString(&fbdoUpload, (mqttBasePath + "/username").c_str(), "mqtt_user");
    delay(200);
    Firebase.RTDB.setString(&fbdoUpload, (mqttBasePath + "/password").c_str(), "mqtt_password");
    delay(200);
    Firebase.RTDB.setBool(&fbdoUpload, (mqttBasePath + "/enabled").c_str(), false);
    delay(200);
    Firebase.RTDB.setString(&fbdoUpload, (mqttBasePath + "/device_name").c_str(), "Lumina");
    
    Serial.println("      ✅ MQTT config created");
    updateMQTTConfigFromFirebase();
  } else {
    updateMQTTConfigFromFirebase();
  }
  
  String versionPath = basePath + "/version";
  if (Firebase.RTDB.setString(&fbdoUpload, versionPath.c_str(), currentFirmwareVersion)) {
    Serial.printf("      ✅ Version published: %s\n", currentFirmwareVersion);
  }
  
  Serial.println("   ✅ Initial data loaded");
}

void streamCallback(FirebaseStream data) {
  Serial.printf("🔔 Firebase Stream - Path: %s, Type: %s, Value: %s\n",
                data.dataPath().c_str(),
                data.dataType().c_str(),
                data.stringData().c_str());

  String dataPath = data.dataPath().c_str();

  if (dataPath == "/effect") {
    currentEffect = data.intData();
    Serial.printf("   Effect changed to: %d\n", currentEffect);
  }
  else if (dataPath == "/speed") {
    effectSpeed = data.intData();
    Serial.printf("   Speed changed to: %d\n", effectSpeed);
  }
  else if (dataPath == "/color") {
    String colorStr = data.stringData();
    if (colorStr.length() == 6) {
      effectColor = strtoul(colorStr.c_str(), NULL, 16);
      Serial.printf("   Color changed to: %s\n", colorStr.c_str());
    }
  }
  else if (dataPath == "/lux_threshold") {
    luxThreshold = data.floatData();
    Serial.printf("   Lux threshold changed to: %.2f\n", luxThreshold);
  }
// In streamCallback(), around line 750, modify the "/enabled" handler:
else if (dataPath == "/enabled") {
    bool newState = data.boolData();
    stripEnabled = newState;
    turnedOffByDarkness = false;
    
    if (newState == false) {
        manuallyTurnedOff = true;  // Ensure this is set
        Serial.println("   ⚠️  MANUAL OFF: LEDs locked off until manually re-enabled");
    } else {
        manuallyTurnedOff = false;
        Serial.println("   ✅ Manual ON: Automation can now control LEDs");
    }
    
    Serial.printf("   Strip %s\n", stripEnabled ? "enabled" : "disabled");
    
    if (!stripEnabled) {
        strip.clear();
        strip.show();
        Serial.println("   LEDs turned off");
    }
}
  else if (dataPath == "/timer_enabled") {
    timerEnabled = data.boolData();
    Serial.printf("   Timer %s\n", timerEnabled ? "enabled" : "disabled");
  }
  else if (dataPath == "/auto_darkness_control") {
    autoDarknessControl = data.boolData();
    Serial.printf("   Auto darkness control %s\n", autoDarknessControl ? "enabled" : "disabled");
  }
  else if (dataPath == "/presence_detection_enabled") {
    presenceDetectionEnabled = data.boolData();
    Serial.printf("   Presence detection %s\n", presenceDetectionEnabled ? "enabled" : "disabled");
  }
  else if (dataPath == "/timer_on") {
    String timeStr = data.stringData();
    if (timeStr.length() == 5) {
      strncpy(timerOnTime, timeStr.c_str(), sizeof(timerOnTime));
      Serial.printf("   Timer ON time changed to: %s\n", timerOnTime);
    }
  }
  else if (dataPath == "/timer_off") {
    String timeStr = data.stringData();
    if (timeStr.length() == 5) {
      strncpy(timerOffTime, timeStr.c_str(), sizeof(timerOffTime));
      Serial.printf("   Timer OFF time changed to: %s\n", timerOffTime);
    }
  }
  else if (dataPath == "/mqtt/broker_address" || 
           dataPath == "/mqtt/broker_port" ||
           dataPath == "/mqtt/username" ||
           dataPath == "/mqtt/password" ||
           dataPath == "/mqtt/enabled" ||
           dataPath == "/mqtt/device_name") {
    updateMQTTConfigFromFirebase();
    Serial.println("   MQTT configuration updated from Firebase");
  }
  else if (dataPath == "/reset" && data.boolData() == true) {
    Serial.println("🔄 Reset command received - deleting config and restarting...");
    
    if (SPIFFS.exists("/config.json")) {
      SPIFFS.remove("/config.json");
    }
    
    ESP.restart();
    return;
  }
  else if (dataPath == "/mic_calibration" && data.boolData() == true) {
    Serial.println("\n🎤 MICROPHONE CALIBRATION TRIGGERED VIA FIREBASE");
    triggerMicCalibration = true;
  }
}

void streamTimeoutCallback(bool timeout) {
  if (timeout) {
    Serial.println("⚠️  Stream timeout, resuming...");
  }
  if (!fbdoStream.httpConnected()) {
    Serial.printf("❌ Stream error: %s\n", fbdoStream.errorReason().c_str());
  }
}

void createDefaultFirebaseData() {
  String testPath = basePath + "/effect";
  
  if (Firebase.RTDB.getInt(&fbdoUpload, testPath.c_str())) {
    Serial.println("Firebase data already exists, skipping default data creation");
    defaultDataCreated = true;
    return;
  }
  
  Serial.println("Creating default Firebase structure for device: " + deviceID);
  
  bool allSuccess = true;
  
  String effectPath = basePath + "/effect";
  if (Firebase.RTDB.setInt(&fbdoUpload, effectPath.c_str(), 0)) {
    Serial.println("Effect set to default: 0");
  } else {
    Serial.printf("Failed to set effect: %s\n", fbdoUpload.errorReason().c_str());
    allSuccess = false;
  }
  
  String speedPath = basePath + "/speed";
  if (Firebase.RTDB.setInt(&fbdoUpload, speedPath.c_str(), 50)) {
    Serial.println("Speed set to default: 50");
  } else {
    Serial.printf("Failed to set speed: %s\n", fbdoUpload.errorReason().c_str());
    allSuccess = false;
  }
  
  String colorPath = basePath + "/color";
  if (Firebase.RTDB.setString(&fbdoUpload, colorPath.c_str(), "FF0000")) {
    Serial.println("Color set to default: FF0000");
  } else {
    Serial.printf("Failed to set color: %s\n", fbdoUpload.errorReason().c_str());
    allSuccess = false;
  }
  
  String enabledPath = basePath + "/enabled";
  if (Firebase.RTDB.setBool(&fbdoUpload, enabledPath.c_str(), true)) {
    Serial.println("Enabled set to default: true");
  } else {
    Serial.printf("Failed to set enabled: %s\n", fbdoUpload.errorReason().c_str());
    allSuccess = false;
  }
  
  String autoDarknessPath = basePath + "/auto_darkness_control";
  if (Firebase.RTDB.setBool(&fbdoUpload, autoDarknessPath.c_str(), true)) {
    Serial.println("Auto darkness control set to default: true");
  } else {
    Serial.printf("Failed to set auto darkness control: %s\n", fbdoUpload.errorReason().c_str());
    allSuccess = false;
  }
  
  String presenceEnabledPath = basePath + "/presence_detection_enabled";
  if (Firebase.RTDB.setBool(&fbdoUpload, presenceEnabledPath.c_str(), true)) {
    Serial.println("Presence detection enabled set to default: true");
  } else {
    Serial.printf("Failed to set presence detection enabled: %s\n", fbdoUpload.errorReason().c_str());
    allSuccess = false;
  }
  
  String luxThresholdPath = basePath + "/lux_threshold";
  if (Firebase.RTDB.setFloat(&fbdoUpload, luxThresholdPath.c_str(), 1.0)) {
    Serial.println("Lux threshold set to default: 1.0");
  } else {
    Serial.printf("Failed to set lux threshold: %s\n", fbdoUpload.errorReason().c_str());
    allSuccess = false;
  }
  
  String timerOnPath = basePath + "/timer_on";
  if (Firebase.RTDB.setString(&fbdoUpload, timerOnPath.c_str(), "09:00")) {
    Serial.println("Timer ON set to default: 09:00");
  } else {
    Serial.printf("Failed to set timer ON: %s\n", fbdoUpload.errorReason().c_str());
    allSuccess = false;
  }
  
  String timerOffPath = basePath + "/timer_off";
  if (Firebase.RTDB.setString(&fbdoUpload, timerOffPath.c_str(), "17:00")) {
    Serial.println("Timer OFF set to default: 17:00");
  } else {
    Serial.printf("Failed to set timer OFF: %s\n", fbdoUpload.errorReason().c_str());
    allSuccess = false;
  }
  
  String timerEnabledPath = basePath + "/timer_enabled";
  if (Firebase.RTDB.setBool(&fbdoUpload, timerEnabledPath.c_str(), true)) {
    Serial.println("Timer enabled set to default: true");
  } else {
    Serial.printf("Failed to set timer enabled: %s\n", fbdoUpload.errorReason().c_str());
    allSuccess = false;
  }
  
  String resetPath = basePath + "/reset";
  if (Firebase.RTDB.setBool(&fbdoUpload, resetPath.c_str(), false)) {
    Serial.println("Reset flag set to default: false");
  } else {
    Serial.printf("Failed to set reset flag: %s\n", fbdoUpload.errorReason().c_str());
    allSuccess = false;
  }
  
  String micCalibPath = basePath + "/mic_calibration";
  if (Firebase.RTDB.setBool(&fbdoUpload, micCalibPath.c_str(), false)) {
    Serial.println("Mic calibration flag set to default: false");
  } else {
    Serial.printf("Failed to set mic calibration flag: %s\n", fbdoUpload.errorReason().c_str());
    allSuccess = false;
  }
  
  defaultDataCreated = allSuccess;
  
  if (allSuccess) {
    Serial.println("Default Firebase data structure created successfully");
  } else {
    Serial.println("Some default values failed to set. Check Firebase rules.");
  }
}

void firebaseTask(void *parameter) {
  Serial.println("🔥 Firebase Task started on Core " + String(xPortGetCoreID()));
  
  for(;;) {
    esp_task_wdt_reset();
    
    if (WiFi.status() == WL_CONNECTED) {
      ArduinoOTA.handle();
      
      if (!Firebase.ready()) {
        Serial.println("⚠️  Firebase not ready, reconnecting...");
        firebaseConnected = false;
        delay(1000);
      } else {
        firebaseConnected = true;
      }
    } else {
      firebaseConnected = false;
      Serial.println("⚠️  WiFi disconnected in Firebase task, attempting reconnect...");
      connectToWiFi();
    }
    
    firebaseTaskStack = uxTaskGetStackHighWaterMark(NULL);
    
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void ledTask(void *parameter) {
  Serial.println("💡 LED Task started on Core " + String(xPortGetCoreID()));
  
  for(;;) {
    esp_task_wdt_reset();
    updateLEDs();
    ledTaskStack = uxTaskGetStackHighWaterMark(NULL);
    vTaskDelay(effectSpeed / portTICK_PERIOD_MS);
  }
}

void automationtask(void *parameter) {
  Serial.println("⚙️  Automation Task started on Core " + String(xPortGetCoreID()));
  
  const TickType_t xDelay = 100 / portTICK_PERIOD_MS;
  static unsigned long lastStateChangeTime = 0;
  const unsigned long CHANGE_LOCKOUT_MS = 3000;
  
  vTaskDelay(3000 / portTICK_PERIOD_MS);

  for(;;) {
    esp_task_wdt_reset();
    
    if (sensorAvailable) {
      updateSensorData();
    }
    radar.read();
    bool presenceDetected = radar.presenceDetected();
    lastPresence = presenceDetected;

    if (manuallyTurnedOff) {
      if (stripEnabled) {
        stripEnabled = false;
        strip.clear();
        strip.show();
      }
      automationTaskStack = uxTaskGetStackHighWaterMark(NULL);
      vTaskDelay(xDelay);
      continue;
    }

    bool targetState = false;
    String reason = "";

    bool presenceCondition = true;
    bool darknessCondition = true;
    
    if (presenceDetectionEnabled) {
      presenceCondition = presenceDetected;
    }
    
    if (autoDarknessControl) {
      if (stripEnabled) {
        darknessCondition = (currentLux < luxThreshold);
      } else {
        darknessCondition = (currentLux < luxThreshold);
      }
    }
    
    if (presenceDetectionEnabled && autoDarknessControl) {
      targetState = presenceCondition && darknessCondition;
      if (targetState) {
        reason = "Presence + Darkness";
      } else if (!presenceCondition) {
        reason = "No presence";
      } else {
        reason = "Too bright";
      }
    }
    else if (presenceDetectionEnabled && !autoDarknessControl) {
      targetState = presenceCondition;
      reason = presenceCondition ? "Presence detected" : "No presence";
    }
    else if (!presenceDetectionEnabled && autoDarknessControl) {
      targetState = darknessCondition;
      reason = darknessCondition ? "Dark enough" : "Too bright";
    }
    else {
      targetState = true;
      reason = "Always ON";
    }

    if (targetState != stripEnabled) {
      if (millis() - lastStateChangeTime > CHANGE_LOCKOUT_MS) {
        stripEnabled = targetState;
        lastStateChangeTime = millis();
        
        if (stripEnabled) {
          Serial.printf("\n💡 LEDs ON: %s (Presence: %s, Lux: %.2f)\n", 
                       reason.c_str(), presenceDetected ? "Yes" : "No", currentLux);
        } else {
          strip.clear();
          strip.show();
          Serial.printf("\n🌙 LEDs OFF: %s (Presence: %s, Lux: %.2f)\n", 
                       reason.c_str(), presenceDetected ? "Yes" : "No", currentLux);
        }
      }
    }

    automationTaskStack = uxTaskGetStackHighWaterMark(NULL);
    vTaskDelay(xDelay);
  }
}

void sensorDataTask(void *parameter) {
  Serial.println("📊 Sensor Data Task started on Core " + String(xPortGetCoreID()));
  
  const TickType_t xDelay = 2000 / portTICK_PERIOD_MS;
  
  for(;;) {
    esp_task_wdt_reset();
    
    if (firebaseConnected) {
      if (sensorAvailable) {
        updateSensorData();
        
        String luxPath = basePath + "/lux";
        
        if (Firebase.RTDB.setFloat(&fbdoUpload, luxPath.c_str(), currentLux)) {
          // Silent success
        } else {
          Serial.printf("⚠️  Failed to send lux data: %s\n", fbdoUpload.errorReason().c_str());
        }
      }
      
      radar.read();
      static bool lastPresenceSent = false;
      static unsigned long lastPresenceSend = 0;
      bool present = radar.presenceDetected();
      
      if (present != lastPresenceSent || (millis() - lastPresenceSend) > 5000) {
        String presencePath = basePath + "/presence";
        if (Firebase.RTDB.setBool(&fbdoUpload, presencePath.c_str(), present)) {
          lastPresenceSent = present;
          lastPresenceSend = millis();
        } else {
          Serial.printf("⚠️  Failed to send presence: %s\n", fbdoUpload.errorReason().c_str());
        }
      }
    }
    
    sensorTaskStack = uxTaskGetStackHighWaterMark(NULL);
    vTaskDelay(xDelay);
  }
}

void timerTask(void *parameter) {
  Serial.println("⏰ Timer Task started on Core " + String(xPortGetCoreID()));
  
  const TickType_t xDelay = 1000 / portTICK_PERIOD_MS;
  bool lastOnTriggered = false;
  bool lastOffTriggered = false;
  
  for(;;) {
    esp_task_wdt_reset();
    
    if (firebaseConnected && timerEnabled) {
      if (checkTimeMatch(timerOnTime)) {
        if (!lastOnTriggered) {
          Serial.println("⏰ Timer ON triggered");
          updateTimerState(true);
          lastOnTriggered = true;
        }
      } else {
        lastOnTriggered = false;
      }
      
      if (checkTimeMatch(timerOffTime)) {
        if (!lastOffTriggered) {
          Serial.println("⏰ Timer OFF triggered");
          updateTimerState(false);
          lastOffTriggered = true;
        }
      } else {
        lastOffTriggered = false;
      }
    }
    
    timerTaskStack = uxTaskGetStackHighWaterMark(NULL);
    vTaskDelay(xDelay);
  }
}

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
  
  if (manuallyTurnedOff) {
    Serial.println("⚠️  Timer cannot turn on LEDs - manually locked off");
    return;
  }
  
  String enabledPath = basePath + "/enabled";
  if (Firebase.RTDB.setBool(&fbdoUpload, enabledPath.c_str(), state)) {
    stripEnabled = state;
    
    if (state) {
      manuallyTurnedOff = false;
      Serial.println("Timer turned ON LEDs - manual lock cleared");
    }
    
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

void updateLEDs() {
  static bool lastStripEnabled = true;
  
  if (!stripEnabled && lastStripEnabled) {
    strip.clear();
    strip.show();
    lastStripEnabled = false;
    return;
  }
  
  if (!stripEnabled) {
    return;
  }
  
  if (stripEnabled && !lastStripEnabled) {
    lastStripEnabled = true;
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

void checkForGitHubUpdate() {
  Serial.println("🔍 Checking for GitHub firmware update...");
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("   ⚠️  WiFi not connected for GitHub OTA check");
    return;
  }

  String latestVersion = fetchLatestVersion();
  if (latestVersion == "") {
    Serial.println("   ❌ Failed to fetch latest version from GitHub");
    return;
  }

  Serial.printf("   Current: %s\n", currentFirmwareVersion);
  Serial.printf("   Latest: %s\n", latestVersion.c_str());

  if (latestVersion != currentFirmwareVersion) {
    int curMajor = 0, curMinor = 0, curPatch = 0;
    int latMajor = 0, latMinor = 0, latPatch = 0;
    
    sscanf(currentFirmwareVersion, "%d.%d.%d", &curMajor, &curMinor, &curPatch);
    sscanf(latestVersion.c_str(), "%d.%d.%d", &latMajor, &latMinor, &latPatch);
    
    bool shouldUpdate = false;
    if (latMajor > curMajor) {
      shouldUpdate = true;
    } else if (latMajor == curMajor && latMinor > curMinor) {
      shouldUpdate = true;
    } else if (latMajor == curMajor && latMinor == curMinor && latPatch > curPatch) {
      shouldUpdate = true;
    }
    
    if (shouldUpdate) {
      Serial.println("   ✅ New firmware available. Starting OTA update...");
      downloadAndApplyFirmware();
    } else {
      Serial.println("   ℹ️  GitHub version is same or older. Skipping update.");
    }
  } else {
    Serial.println("   ✅ Device is up to date.");
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
    Serial.printf("   ❌ Failed to fetch version. HTTP code: %d\n", httpCode);
    http.end();
    return "";
  }
}

void downloadAndApplyFirmware() {
  Serial.println("📥 Downloading firmware from GitHub...");
  
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(GITHUB_FIRMWARE_URL);

  int httpCode = http.GET();
  Serial.printf("   HTTP GET code: %d\n", httpCode);

  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    Serial.printf("   Firmware size: %d bytes (%.2f KB)\n", contentLength, contentLength / 1024.0);

    if (contentLength > 0) {
      strip.clear();
      strip.show();
      Serial.println("   💡 LEDs turned off for OTA process...");

      WiFiClient* stream = http.getStreamPtr();
      if (startGitHubOTAUpdate(stream, contentLength)) {
        Serial.println("   ✅ GitHub OTA update successful, restarting...");
        delay(2000);
        ESP.restart();
      } else {
        Serial.println("   ❌ GitHub OTA update failed");
      }
    } else {
      Serial.println("   ❌ Invalid firmware size for GitHub OTA");
    }
  } else {
    Serial.printf("   ❌ Failed to fetch firmware from GitHub. HTTP code: %d\n", httpCode);
  }
  http.end();
}

bool startGitHubOTAUpdate(WiFiClient* client, int contentLength) {
  Serial.println("   🔄 Initializing GitHub update...");
  if (!Update.begin(contentLength)) {
    Serial.printf("   ❌ Update begin failed: %s\n", Update.errorString());
    return false;
  }

  Serial.println("   📝 Writing firmware...");
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
          Serial.printf("      Progress: %d%%\r", progress);
          lastProgress = progress;
        }
      }
    }
    
    if (millis() - lastDataTime > timeoutDuration) {
      Serial.println("\n   ❌ Timeout: No data received. Aborting update...");
      Update.abort();
      return false;
    }

    yield();
  }
  Serial.println("\n   ✅ Writing complete");

  if (written != contentLength) {
    Serial.printf("   ❌ Error: Write incomplete. Expected %d but got %d bytes\n", contentLength, written);
    Update.abort();
    return false;
  }

  if (!Update.end()) {
    Serial.printf("   ❌ Error: Update end failed: %s\n", Update.errorString());
    return false;
  }

  Serial.println("   ✅ Update successfully completed");
  return true;
}