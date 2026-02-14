#include "globals.h"

// ============================================================================
// GLOBAL VARIABLE DEFINITIONS
// ============================================================================
portMUX_TYPE stripMux = portMUX_INITIALIZER_UNLOCKED;
unsigned long lastSystemStatsReport = 0;
const unsigned long SYSTEM_STATS_INTERVAL = 30000;
UBaseType_t ledTaskStack = 0;
UBaseType_t otaTaskStack = 0;

TaskHandle_t ledTaskHandle = NULL;
TaskHandle_t otaTaskHandle = NULL;

unsigned long lastLoopTime = 0;
unsigned long loopCounter = 0;
bool systemHealthy = true;
CRGB *leds = nullptr;

volatile int currentEffect = 0;
volatile uint32_t effectSpeed = 50;
volatile uint32_t effectColor = 0xFF0000;
volatile bool updateEffect = false;
volatile bool stripEnabled = true;

String deviceID;
String wifiSSID;
String wifiPassword;
int ledCount;
String basePath;

const char* GITHUB_FIRMWARE_URL = "https://github.com/majd-shwikani/Lumina-bin/releases/download/Lumina/firmware.bin";
const char* GITHUB_VERSION_URL = "https://raw.githubusercontent.com/majd-shwikani/Lumina-bin/refs/heads/main/version.txt";
const char* currentFirmwareVersion = "1.2.0";
const unsigned long UPDATE_CHECK_INTERVAL = 10 * 60 * 1000;
unsigned long lastUpdateCheck = 0;

// ============================================================================
// SYSTEM MONITORING FUNCTIONS
// ============================================================================

const char* getResetReason(int cpu) {
  RESET_REASON reason = rtc_get_reset_reason(cpu);
  
  switch (reason) {
    case 1:  return "POWERON_RESET";
    case 3:  return "SW_RESET";
    case 4:  return "OWDT_RESET";
    case 5:  return "DEEPSLEEP_RESET";
    case 6:  return "SDIO_RESET";
    case 7:  return "TG0WDT_SYS_RESET";
    case 8:  return "TG1WDT_SYS_RESET";
    case 9:  return "RTCWDT_SYS_RESET";
    case 10: return "INTRUSION_RESET";
    case 11: return "TGWDT_CPU_RESET";
    case 12: return "SW_CPU_RESET";
    case 13: return "RTCWDT_CPU_RESET";
    case 14: return "EXT_CPU_RESET";
    case 15: return "RTCWDT_BROWN_OUT_RESET";
    case 16: return "RTCWDT_RTC_RESET";
    default: return "UNKNOWN";
  }
}

void printSystemStats() {
  Serial.println("\n╔══════════════════════════════════════════════════════════════════════╗");
  Serial.println("║              SYSTEM HEALTH & DIAGNOSTICS REPORT                      ║");
  Serial.println("╠══════════════════════════════════════════════════════════════════════╣");
  
  Serial.printf("║ Free Heap: %7d bytes | Uptime: %lu s                                ║\n", 
                ESP.getFreeHeap(), millis() / 1000);
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("║ WiFi: CONNECTED | IP: %-15s | RSSI: %d dBm                  ║\n", 
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else {
    Serial.println("║ WiFi: DISCONNECTED                                                   ║");
  }
  
  Serial.printf("║ Current Effect: %-2d | Speed: %-4d ms | Strip Enabled: %-3s          ║\n", 
                currentEffect, effectSpeed, stripEnabled ? "YES" : "NO");
  
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

// ============================================================================
// WIFI & NETWORK FUNCTIONS
// ============================================================================

void connectToWiFi() {
  Serial.printf("   Connecting to: %s\n", wifiSSID.c_str());
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 20000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("   ✅ Connected! IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("   ❌ Failed to connect to WiFi");
  }
}

void setupTime() {
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    Serial.print("   ✅ Time synchronized");
  } else {
    Serial.println("   ⚠️  Failed to synchronize time");
  }
}

void setupOTA() {
  String hostname = "lumina-" + deviceID;
  ArduinoOTA.setHostname(hostname.c_str());
  ArduinoOTA.begin();
  Serial.printf("   Hostname: %s\n", hostname.c_str());
}
