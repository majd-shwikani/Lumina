#include "globals.h"

// ============================================================================
// GLOBAL VARIABLE DEFINITIONS
// ============================================================================
portMUX_TYPE stripMux = portMUX_INITIALIZER_UNLOCKED;
unsigned long lastSystemStatsReport = 0;
const unsigned long SYSTEM_STATS_INTERVAL = 30000;
UBaseType_t firebaseTaskStack = 0;
UBaseType_t ledTaskStack = 0;
UBaseType_t automationTaskStack = 0;
UBaseType_t sensorTaskStack = 0;
UBaseType_t timerTaskStack = 0;
UBaseType_t mqttTaskStack = 0;
UBaseType_t otaTaskStack = 0;
TaskHandle_t firebaseTaskHandle = NULL;
TaskHandle_t ledTaskHandle = NULL;
TaskHandle_t automationTaskHandle = NULL;
TaskHandle_t sensorTaskHandle = NULL;
TaskHandle_t timerTaskHandle = NULL;
TaskHandle_t mqttTaskHandle = NULL;
TaskHandle_t otaTaskHandle = NULL;
unsigned long lastLoopTime = 0;
unsigned long loopCounter = 0;
bool systemHealthy = true;
Adafruit_NeoPixel strip(1, LED_PIN, NEO_GRB + NEO_KHZ800);
FirebaseData fbdoStream;
FirebaseData fbdoUpload;
FirebaseAuth auth;
FirebaseConfig config;
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
unsigned long lastStateChangeTime = 0;
const unsigned long STATE_CHANGE_DEBOUNCE = 5000;
float luxHysteresis = 5.0;
unsigned long luxReadDelayAfterChange = 0;
ld2410 radar;
volatile bool lastPresence = false;
volatile bool presenceDetectionEnabled = true;
volatile bool lastPresenceState = false;
unsigned long lastPresenceReport = 0;
const int RADAR_RX_PIN = 16;
const int RADAR_TX_PIN = 17;
char timerOnTime[6] = "09:00";
char timerOffTime[6] = "17:00";
bool timerEnabled = true;
String deviceID;
String wifiSSID;
String wifiPassword;
int ledCount;
String basePath;
const char* GITHUB_FIRMWARE_URL = "https://github.com/majd-shwikani/Lumina-bin/releases/download/Lumina/firmware.bin";
const char* GITHUB_VERSION_URL = "https://raw.githubusercontent.com/majd-shwikani/Lumina-bin/refs/heads/main/version.txt";
const char* currentFirmwareVersion = "1.1.5";
const unsigned long UPDATE_CHECK_INTERVAL = 10 * 60 * 1000;
unsigned long lastUpdateCheck = 0;
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
  Serial.println("\n╔══════════════════════════════════════════════════════════════════════╗");
  Serial.println("║              SYSTEM HEALTH & DIAGNOSTICS REPORT                      ║");
  Serial.println("╠══════════════════════════════════════════════════════════════════════╣");
  
  Serial.println("║ RESET INFORMATION:                                                   ║");
  Serial.printf("║   CPU0: %-59s ║\n", getResetReason(0));
  Serial.printf("║   CPU1: %-59s ║\n", getResetReason(1));
  Serial.println("║                                                                      ║");
  
  Serial.println("║ MEMORY STATUS:                                                       ║");
  Serial.printf("║   Free Heap:     %7d bytes (%6.1f KB)                          ║\n", 
                ESP.getFreeHeap(), ESP.getFreeHeap() / 1024.0);
  Serial.printf("║   Heap Size:     %7d bytes (%6.1f KB)                          ║\n", 
                ESP.getHeapSize(), ESP.getHeapSize() / 1024.0);
  Serial.printf("║   Min Free Heap: %7d bytes (%6.1f KB)                          ║\n", 
                ESP.getMinFreeHeap(), ESP.getMinFreeHeap() / 1024.0);
  Serial.printf("║   Free PSRAM:    %7d bytes (%6.1f KB)                          ║\n", 
                ESP.getFreePsram(), ESP.getFreePsram() / 1024.0);
  Serial.printf("║   Heap Usage:    %5.1f%%                                            ║\n", 
                100.0 - (ESP.getFreeHeap() * 100.0 / ESP.getHeapSize()));
  Serial.println("║                                                                      ║");
  
  Serial.println("║ TASK STACK HIGH WATER MARKS (bytes remaining):                       ║");
  Serial.printf("║   Firebase Task:   %5d bytes (%5.1f KB)                          ║\n", 
                firebaseTaskStack * sizeof(StackType_t), 
                (firebaseTaskStack * sizeof(StackType_t)) / 1024.0);
  Serial.printf("║   LED Task:        %5d bytes (%5.1f KB)                          ║\n", 
                ledTaskStack * sizeof(StackType_t), 
                (ledTaskStack * sizeof(StackType_t)) / 1024.0);
  Serial.printf("║   Automation Task: %5d bytes (%5.1f KB)                          ║\n", 
                automationTaskStack * sizeof(StackType_t), 
                (automationTaskStack * sizeof(StackType_t)) / 1024.0);
  Serial.printf("║   Sensor Task:     %5d bytes (%5.1f KB)                          ║\n", 
                sensorTaskStack * sizeof(StackType_t), 
                (sensorTaskStack * sizeof(StackType_t)) / 1024.0);
  Serial.printf("║   Timer Task:      %5d bytes (%5.1f KB)                          ║\n", 
                timerTaskStack * sizeof(StackType_t), 
                (timerTaskStack * sizeof(StackType_t)) / 1024.0);
  Serial.printf("║   MQTT Task:       %5d bytes (%5.1f KB)                          ║\n", 
                mqttTaskStack * sizeof(StackType_t), 
                (mqttTaskStack * sizeof(StackType_t)) / 1024.0);
  Serial.printf("║   OTA Task:        %5d bytes (%5.1f KB)                          ║\n", 
                otaTaskStack * sizeof(StackType_t), 
                (otaTaskStack * sizeof(StackType_t)) / 1024.0);
  Serial.println("║                                                                      ║");
  
  Serial.println("║ SYSTEM STATUS:                                                       ║");
  Serial.printf("║   Uptime:          %6lu seconds (%6.1f minutes)                  ║\n", 
                millis() / 1000, millis() / 60000.0);
  Serial.printf("║   CPU Frequency:   %d MHz                                           ║\n", 
                ESP.getCpuFreqMHz());
  Serial.printf("║   Flash Size:      %7d bytes (%5.1f MB)                       ║\n", 
                ESP.getFlashChipSize(), ESP.getFlashChipSize() / (1024.0 * 1024.0));
  Serial.printf("║   Flash Speed:     %d Hz                                      ║\n", 
                ESP.getFlashChipSpeed());
  Serial.printf("║   SDK Version:     %-45s ║\n", ESP.getSdkVersion());
  Serial.printf("║   Chip Model:      %-45s ║\n", ESP.getChipModel());
  Serial.printf("║   Chip Revision:   %d                                                ║\n", 
                ESP.getChipRevision());
  Serial.printf("║   Loop Counter:    %-45lu ║\n", loopCounter);
  Serial.println("║                                                                      ║");
  
  Serial.println("║ NETWORK STATUS:                                                      ║");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("║   WiFi:            CONNECTED                                         ║");
    Serial.printf("║   IP Address:      %-45s ║\n", WiFi.localIP().toString().c_str());
    Serial.printf("║   Signal (RSSI):   %d dBm                                           ║\n", 
                  WiFi.RSSI());
    Serial.printf("║   MAC Address:     %-45s ║\n", WiFi.macAddress().c_str());
  } else {
    Serial.println("║   WiFi:            DISCONNECTED                                      ║");
  }
  Serial.printf("║   Firebase:        %-45s ║\n", firebaseConnected ? "CONNECTED" : "DISCONNECTED");
  Serial.printf("║   MQTT:            %-45s ║\n", mqttConnected ? "CONNECTED" : "DISCONNECTED");
  Serial.println("║                                                                      ║");
  
  Serial.println("║ DEVICE STATUS:                                                       ║");
  Serial.printf("║   Device ID:       %-45s ║\n", deviceID.c_str());
  Serial.printf("║   LED Count:       %-45d ║\n", ledCount);
  Serial.printf("║   Strip Enabled:   %-45s ║\n", stripEnabled ? "YES" : "NO");
  Serial.printf("║   Current Effect:  %-45d ║\n", currentEffect);
  Serial.printf("║   Effect Speed:    %d ms                                             ║\n", 
                effectSpeed);
  Serial.printf("║   Brightness:      %-45d ║\n", strip.getBrightness());
  Serial.printf("║   Manually Off:    %-45s ║\n", manuallyTurnedOff ? "YES" : "NO");
  Serial.println("║                                                                      ║");
  
  Serial.println("║ SENSOR STATUS:                                                       ║");
  Serial.printf("║   Light Sensor:    %-45s ║\n", sensorAvailable ? "AVAILABLE" : "NOT AVAILABLE");
  if (sensorAvailable) {
    Serial.printf("║   Current Lux:     %-45.2f ║\n", currentLux);
    Serial.printf("║   Lux Threshold:   %-45.2f ║\n", luxThreshold);
  }
  Serial.printf("║   Presence Detect: %-45s ║\n", 
                presenceDetectionEnabled ? "ENABLED" : "DISABLED");
  Serial.printf("║   Presence Now:    %-45s ║\n", lastPresence ? "DETECTED" : "NOT DETECTED");
  Serial.printf("║   Auto Darkness:   %-45s ║\n", 
                autoDarknessControl ? "ENABLED" : "DISABLED");
  Serial.println("║                                                                      ║");
  
  Serial.println("║ AUDIO STATUS:                                                        ║");
  Serial.printf("║   Calibration:     %-45s ║\n", 
                calibrationComplete ? "COMPLETE" : "PENDING");
  Serial.printf("║   Noise Floor:     %-45.2f ║\n", noiseFloor);
  Serial.printf("║   Gain Multiplier: %-45.2f ║\n", gainMultiplier);
  Serial.printf("║   Audio Level:     %-45.2f ║\n", globalAudioLevel);
  Serial.printf("║   Beat Detected:   %-45s ║\n", beatDetected ? "YES" : "NO");
  Serial.println("║                                                                      ║");
  
  Serial.println("║ HEALTH WARNINGS:                                                     ║");
  bool warningsFound = false;
  
  if (ESP.getFreeHeap() < 30000) {
    Serial.println("║   ⚠️  WARNING: Low heap memory!                                      ║");
    warningsFound = true;
  }
  if (firebaseTaskStack < 500) {
    Serial.println("║   ⚠️  WARNING: Firebase task stack critically low!                   ║");
    warningsFound = true;
  }
  if (ledTaskStack < 500) {
    Serial.println("║   ⚠️  WARNING: LED task stack critically low!                        ║");
    warningsFound = true;
  }
  if (automationTaskStack < 200) {
    Serial.println("║   ⚠️  WARNING: Automation task stack critically low!                 ║");
    warningsFound = true;
  }
  if (sensorTaskStack < 500) {
    Serial.println("║   ⚠️  WARNING: Sensor task stack critically low!                     ║");
    warningsFound = true;
  }
  if (timerTaskStack < 300) {
    Serial.println("║   ⚠️  WARNING: Timer task stack critically low!                      ║");
    warningsFound = true;
  }
  if (mqttTaskStack < 500) {
    Serial.println("║   ⚠️  WARNING: MQTT task stack critically low!                       ║");
    warningsFound = true;
  }
  if (otaTaskStack < 500) {
    Serial.println("║   ⚠️  WARNING: OTA task stack critically low!                        ║");
    warningsFound = true;
  }
  if (!firebaseConnected) {
    Serial.println("║   ⚠️  WARNING: Firebase disconnected!                                ║");
    warningsFound = true;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("║   ⚠️  WARNING: WiFi disconnected!                                    ║");
    warningsFound = true;
  }
  if (!warningsFound) {
    Serial.println("║   ✅ All systems nominal                                             ║");
  }
  
  Serial.println("╚══════════════════════════════════════════════════════════════════════╝\n");
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