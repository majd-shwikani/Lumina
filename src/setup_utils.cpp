#include "globals.h"

// ============================================================================
// GLOBAL VARIABLE DEFINITIONS
// ============================================================================
portMUX_TYPE stripMux = portMUX_INITIALIZER_UNLOCKED;
unsigned long lastSystemStatsReport = 0;
const unsigned long SYSTEM_STATS_INTERVAL = 30000;
UBaseType_t ledTaskStack = 0;
UBaseType_t discoveryTaskStack = 0;

TaskHandle_t ledTaskHandle = NULL;
TaskHandle_t discoveryTaskHandle = NULL;
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
OperatingMode currentMode = MODE_MIRROR;

// ESP-NOW Variables
uint8_t gatewayMAC[6] = {0};
bool gatewayFound = false;
unsigned long lastGatewayContact = 0;
const unsigned long GATEWAY_TIMEOUT = 60000; // 60 seconds
int currentWifiChannel = 1;

// ESP-NOW Variables
uint8_t gatewayMAC[6] = {0};
bool gatewayFound = false;
unsigned long lastGatewayContact = 0;
const unsigned long GATEWAY_TIMEOUT = 15000; // 15 seconds
int currentWifiChannel = 1;

String deviceID;
String wifiSSID;
String wifiPassword;
int ledCount;

const char* GITHUB_FIRMWARE_URL = "https://github.com/majd-shwikani/Lumina-bin/releases/download/Lumina/firmware.bin";
const char* GITHUB_VERSION_URL = "https://raw.githubusercontent.com/majd-shwikani/Lumina-bin/refs/heads/main/version.txt";
const char* currentFirmwareVersion = "2.1.0-ESPNOW";
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
    case 12: return "SW_CPU_RESET";
    case 15: return "BROWN_OUT_RESET";
    default: return "OTHER";
  }
}

void printSystemStats() {
  Serial.println("\n╔══════════════════════════════════════════════════════════════════════╗");
  Serial.println("║              SYSTEM HEALTH & DIAGNOSTICS REPORT                      ║");
  Serial.println("╠══════════════════════════════════════════════════════════════════════╣");
  Serial.printf("║ Free Heap: %7d bytes | Uptime: %lu s                                ║\n", 
                ESP.getFreeHeap(), millis() / 1000);
  Serial.printf("║ ESP-NOW: %-12s | Channel: %-2d | Mode: %-24s ║\n", 
                gatewayFound ? "LOCKED" : "HUNTING", currentWifiChannel,
                currentMode == MODE_MIRROR ? "MIRROR" : "STANDALONE");
  Serial.printf("║ Gateway: %02X:%02X:%02X:%02X:%02X:%02X | Effect: %-2d | Strip Enabled: %-3s ║\n", 
                gatewayMAC[0], gatewayMAC[1], gatewayMAC[2], gatewayMAC[3], gatewayMAC[4], gatewayMAC[5],
                currentEffect, stripEnabled ? "YES" : "NO");
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

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, buf.get());
  if (error) {
    Serial.printf("❌ Failed to parse config file: %s\n", error.c_str());
    return false;
  }

  wifiSSID = doc["wifi_ssid"].as<String>();
  wifiPassword = doc["wifi_password"].as<String>();
  deviceID = doc["device_id"].as<String>();
  ledCount = doc["num_leds"];

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
  if (wifiSSID == "") return;
  
  Serial.printf("   Connecting to WiFi: %s\n", wifiSSID.c_str());
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("   ✅ Connected! IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("   ❌ Failed to connect to WiFi (Proceeding to ESP-NOW)");
  }
}

// ============================================================================
// ESP-NOW INITIALIZATION
// ============================================================================

void setupEspNow() {
  WiFi.mode(WIFI_AP_STA); // Use both for potential discovery while keeping AP if needed
  WiFi.disconnect();
  
  Serial.print("   Local MAC Address: ");
  Serial.println(WiFi.macAddress());
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ Error initializing ESP-NOW");
    return;
  }
  
  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);
  
  // Add broadcast peer for discovery
  esp_now_peer_info_t peerInfo = {};
  uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0; // Current channel
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("❌ Failed to add broadcast peer");
  }
  
  Serial.println("✅ ESP-NOW Initialized (Receiver Mode)");
}
