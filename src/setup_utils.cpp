#include "globals.h"

// ============================================================================
// GLOBAL VARIABLE DEFINITIONS
// ============================================================================
portMUX_TYPE stripMux = portMUX_INITIALIZER_UNLOCKED;
SemaphoreHandle_t i2sMutex = NULL;
unsigned long lastSystemStatsReport = 0;
const unsigned long SYSTEM_STATS_INTERVAL = 30000;
UBaseType_t cloudTaskStack = 0;
UBaseType_t ledTaskStack = 0;
UBaseType_t ioTaskStack = 0;
UBaseType_t systemTaskStack = 0;
TaskHandle_t cloudTaskHandle = NULL;
TaskHandle_t ledTaskHandle = NULL;
TaskHandle_t ioTaskHandle = NULL;
TaskHandle_t systemTaskHandle = NULL;
unsigned long lastLoopTime = 0;
unsigned long loopCounter = 0;
bool systemHealthy = true;
bool systemInitialized = false;
volatile bool configPortalActive = false;
CRGB *leds = nullptr;
CRGB onboardLed[1];
FirebaseData fbdoStream;
FirebaseData fbdoUpload;
FirebaseAuth auth;
FirebaseConfig config;
volatile int currentEffect = 0;
volatile uint32_t effectSpeed = 50;
volatile uint32_t effectColor = 0xFF0000;
volatile uint8_t globalBrightness = 255;
volatile bool updateEffect = false;
volatile bool firebaseConnected = false;
volatile bool stripEnabled = true;
volatile bool autoDarknessControl = false;
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
char timerOnTime[6] = "09:00";
char timerOffTime[6] = "17:00";
bool timerEnabled = true;
String deviceID;
String wifiSSID;
String wifiPassword;
int ledCount;
String basePath;
const char* GITHUB_FIRMWARE_URL = "https://github.com/majd-shwikani/Lumina-bin/releases/download/Lumina/firmwareS3.bin";
const char* GITHUB_VERSION_URL = "https://raw.githubusercontent.com/majd-shwikani/Lumina-bin/refs/heads/main/versionS3.txt";
const char* currentFirmwareVersion = "2.0.0";
const unsigned long UPDATE_CHECK_INTERVAL = 10 * 60 * 1000;
unsigned long lastUpdateCheck = 0;
unsigned long buttonPressStart = 0;
bool buttonActive = false;

// ============================================================================
// ESP-NOW GATEWAY LOGIC
// ============================================================================
volatile bool registryChanged = false;

void onDataRecvGateway(const uint8_t *mac, const uint8_t *data, int len) {
  if (len < sizeof(LuminaMessage)) return;
  
  LuminaMessage *incoming = (LuminaMessage *)data;
  
  // 1. Respond to Discovery requests
  if (strcmp(incoming->msgType, "LUMINA_DISCOVERY") == 0) {
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
             
    // Check if already in registry
    bool found = false;
    for (int i = 0; i < receiverCount; i++) {
      if (receivers[i].macStr == String(macStr)) {
        found = true;
        break;
      }
    }
    
    if (!found && receiverCount < 10) {
      memcpy(receivers[receiverCount].mac, mac, 6);
      receivers[receiverCount].macStr = String(macStr);
      receivers[receiverCount].registered = true;
      receivers[receiverCount].isMirror = true;
      receivers[receiverCount].effect = 0;
      receivers[receiverCount].speed = 50;
      receivers[receiverCount].color = 0xFF0000;
      receivers[receiverCount].enabled = true;
      receivers[receiverCount].needsFirebaseSync = true; // Mark for task sync
      receiverCount++;
      registryChanged = true; // Signal the firebase task
      Serial.printf("🆕 [Gateway] Discovered new receiver: %s\n", macStr);
    } else {
      // If already found, just log it occasionally
      // Serial.printf("📡 [Gateway] Discovery heartbeat from known node: %s\n", macStr);
    }
    
    if (!esp_now_is_peer_exist(mac)) {
      esp_now_peer_info_t peerInfo = {};
      memcpy(peerInfo.peer_addr, mac, 6);
      peerInfo.channel = 0;
      peerInfo.encrypt = false;
      esp_now_add_peer(&peerInfo);
    }
    
    LuminaMessage offer;
    strcpy(offer.msgType, "LUMINA_OFFER");
    memset(offer.targetMac, 0, 6);
    esp_now_send(mac, (uint8_t *)&offer, sizeof(offer));
  }
  // 2. Log if we see commands from others
  else if (strcmp(incoming->msgType, "LUMINA_CMD") == 0) {
    Serial.printf("ℹ️ [Gateway] Observed CMD packet from %02X:%02X... (Effect=%d)\n", 
                  mac[0], mac[1], incoming->effect);
  }
}

void onDataSentGateway(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Optional: log failures
  if (status != ESP_NOW_SEND_SUCCESS) {
    // Serial.printf("⚠️ [Gateway] Delivery Fail to %02X:%02X:%02X:%02X:%02X:%02X\n", 
    //               mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  }
}

void setupEspNowGateway() {
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW Init Failed");
    return;
  }
  
  esp_now_register_recv_cb(onDataRecvGateway);
  esp_now_register_send_cb(onDataSentGateway);
  
  // Register broadcast peer
  esp_now_peer_info_t peerInfo = {};
  uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
  
  Serial.println("✅ ESP-NOW Gateway Ready");
}

void routeCommandToReceiver(int index) {
  if (index < 0 || index >= receiverCount) return;
  
  LuminaMessage msg;
  strcpy(msg.msgType, "LUMINA_CMD");
  memcpy(msg.targetMac, receivers[index].mac, 6);
  
  portENTER_CRITICAL(&stripMux);
  if (receivers[index].isMirror) {
    msg.effect = currentEffect;
    msg.speed = effectSpeed;
    msg.color = effectColor;
    msg.brightness = globalBrightness;
    msg.enabled = stripEnabled;
  } else {
    msg.effect = receivers[index].effect;
    msg.speed = receivers[index].speed;
    msg.color = receivers[index].color;
    msg.brightness = receivers[index].brightness;
    msg.enabled = receivers[index].enabled;
  }
  portEXIT_CRITICAL(&stripMux);
  
  // Retry mechanism for robustness
  int maxRetries = 3;
  esp_err_t result;
  for (int i = 0; i < maxRetries; i++) {
    result = esp_now_send(receivers[index].mac, (uint8_t *)&msg, sizeof(msg));
    if (result == ESP_OK) break;
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }

  if (result == ESP_OK) {
    // Serial.printf("📡 [Gateway] Sent CMD to %s: Eff=%d, En=%s\n", 
    //               receivers[index].macStr.c_str(), msg.effect, msg.enabled ? "YES" : "NO");
  }
}

void syncAllMirrors() {
  for (int i = 0; i < receiverCount; i++) {
    if (receivers[i].isMirror) {
      routeCommandToReceiver(i);
    }
  }
}

void updateActiveNodesInFirebase() {
  FirebaseJson json;
  for (int i = 0; i < receiverCount; i++) {
    json.set(receivers[i].macStr.c_str(), "online");
  }
  
  String path = basePath + "/active_nodes";
  if (!Firebase.RTDB.setJSON(&fbdoUpload, path.c_str(), &json)) {
    Serial.printf("❌ [Gateway] Failed to update active_nodes: %s\n", fbdoUpload.errorReason().c_str());
  }
}

void broadcastGatewayState() {
  // 1. Sync Mirrors (Forces them to match Gateway state)
  syncAllMirrors();

  // 2. Heartbeat for Standalone Nodes
  // We send them their own current registered state as a "Keep-Alive"
  // This ensures they reset their lastGatewayContact timer without switching modes.
  for (int i = 0; i < receiverCount; i++) {
    if (!receivers[i].isMirror) {
      LuminaMessage msg;
      strcpy(msg.msgType, "LUMINA_CMD");
      memcpy(msg.targetMac, receivers[i].mac, 6);
      
      msg.effect = receivers[i].effect;
      msg.speed = receivers[i].speed;
      msg.color = receivers[i].color;
      msg.enabled = receivers[i].enabled;

      esp_now_send(receivers[i].mac, (uint8_t *)&msg, sizeof(msg));
      // Serial.printf("💓 [Gateway] Heartbeat sent to Standalone: %s\n", receivers[i].macStr.c_str());
    }
  }
}

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
  uint32_t freeHeap   = ESP.getFreeHeap();
  uint32_t totalHeap  = ESP.getHeapSize();
  uint32_t usedHeap   = totalHeap - freeHeap;
  uint32_t freePsram  = ESP.getFreePsram();
  uint32_t totalPsram = ESP.getPsramSize();
  uint32_t usedPsram  = totalPsram - freePsram;

  // Stack sizes match the byte values passed to xTaskCreatePinnedToCore.
  // uxTaskGetStackHighWaterMark returns remaining words; multiply by
  // sizeof(StackType_t) (4 on Xtensa/RISC-V) to get bytes, then /1024 for KB.
  struct TaskInfo {
    const char*  name;
    TaskHandle_t handle;
    uint32_t     allocBytes; // byte value passed to xTaskCreatePinnedToCore
  };

  TaskInfo tasks[] = {
    { "CloudSync",  cloudTaskHandle,  12000 },
    { "LEDAnim",    ledTaskHandle,     4000 },
    { "IOTask",     ioTaskHandle,     12000 },
    { "SystemTask", systemTaskHandle,  8000 },
  };
  const int taskCount = sizeof(tasks) / sizeof(tasks[0]);

  unsigned long uptimeSec = millis() / 1000;
  unsigned int  days      = uptimeSec / 86400;
  unsigned int  hrs       = (uptimeSec % 86400) / 3600;
  unsigned int  mins      = (uptimeSec % 3600)  / 60;
  unsigned int  secs      = uptimeSec % 60;

  char uptimeBuf[20];
  if (days > 0) snprintf(uptimeBuf, sizeof(uptimeBuf), "%ud %02u:%02u:%02u", days, hrs, mins, secs);
  else          snprintf(uptimeBuf, sizeof(uptimeBuf), "%02u:%02u:%02u", hrs, mins, secs);

  // Each content row: "  ║  " + 49 chars + "  ║"
  // Total line width: 57 chars
  #define P(fmt, ...) Serial.printf("  ║  " fmt "\n", ##__VA_ARGS__)
  #define DIV          Serial.println("  ╠═══════════════════════════════════════════════════════╣")

  Serial.println();
  Serial.println(   "  ╔═══════════════════════════════════════════════════════╗");
  Serial.println(   "  ║             LUMINA  —  SYSTEM STATUS                  ║");
  DIV;
  P("Uptime    %-16s   Firmware  v%s", uptimeBuf, currentFirmwareVersion);
  P("WiFi      ch%-2d  %d dBm   IP  %s", WiFi.channel(), WiFi.RSSI(), WiFi.localIP().toString().c_str());
  DIV;
  P("RAM       %u / %u KB         CPU  %.1f C", usedHeap/1024, totalHeap/1024, currentCpuTemp);
  if (totalPsram > 0)
    P("PSRAM     %u / %u KB", usedPsram/1024, totalPsram/1024);
  if (ina219Available)
    P("Power     %.0f mW   %.0f mA   %.2f V", currentPower, currentCurrent, currentVoltage);
  DIV;
  P("Effect    %-4d   Speed  %-4lu ms   Strip  %-3s   Bright  %u",
    currentEffect, effectSpeed, stripEnabled ? "ON" : "OFF", (unsigned)globalBrightness);
  DIV;
  P("%-12s   %6s   %6s   %6s   %s", "Task", "Alloc", "Used", "Free", "Use%");
  DIV;

  bool anyWarning = false;
  for (int i = 0; i < taskCount; i++) {
    TaskInfo& t = tasks[i];
    if (t.handle == NULL) {
      P("%-12s   %5uB   n/a      n/a       --", t.name, t.allocBytes);
      continue;
    }
    uint32_t freeBytes = (uint32_t)uxTaskGetStackHighWaterMark(t.handle) * sizeof(StackType_t);
    uint32_t usedBytes = t.allocBytes - freeBytes;
    uint32_t usedPct   = (usedBytes * 100) / t.allocBytes;
    const char* flag   = "";
    if      (usedPct >= 90) { flag = "  !! CRIT"; anyWarning = true; }
    else if (usedPct >= 75) { flag = "  ! HIGH";  anyWarning = true; }
    P("%-12s   %5uB   %5uB   %5uB   %2u%%%s", t.name, t.allocBytes, usedBytes, freeBytes, usedPct, flag);
  }

  systemHealthy = !anyWarning;
  DIV;
  P("%s", systemHealthy ? "Health    ALL SYSTEMS NOMINAL" : "Health    WARNINGS PRESENT");
  Serial.println("  ╚═══════════════════════════════════════════════════════╝");
  Serial.println();

  #undef P
  #undef DIV
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

void checkBootCount() {
  int bootCount = 0;
  if (SPIFFS.exists("/boot_count")) {
    File f = SPIFFS.open("/boot_count", "r");
    if (f) {
      String val = f.readString();
      bootCount = val.toInt();
      f.close();
    }
  }
  
  bootCount++;
  Serial.printf("🔄 [Boot] Boot counter: %d/3\n", bootCount);
  
  if (bootCount >= 3) {
    Serial.println("⚠️ [Boot] Reset threshold reached! Deleting configuration...");
    if (SPIFFS.exists("/config.json")) SPIFFS.remove("/config.json");
    if (SPIFFS.exists("/boot_count")) SPIFFS.remove("/boot_count");
    Serial.println("✅ Configuration wiped. Proceeding to config portal.");
    // No need to restart, setup() will call shouldStartConfigPortal()
  } else {
    File f = SPIFFS.open("/boot_count", "w");
    if (f) {
      f.print(bootCount);
      f.close();
      Serial.println("📂 [Boot] Boot counter saved to SPIFFS");
    }
    
    // Create a task to clear the boot counter after 10 seconds
    xTaskCreate(clearBootCount, "ClearBootTask", 2048, NULL, 1, NULL);
  }
}

void clearBootCount(void *parameter) {
  vTaskDelay(10000 / portTICK_PERIOD_MS);
  if (SPIFFS.exists("/boot_count")) {
    SPIFFS.remove("/boot_count");
    Serial.println("🧹 [Boot] 10 seconds passed. Boot counter cleared from SPIFFS.");
  }
  vTaskDelete(NULL);
}

bool loadConfig() {
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) return false;
  
  size_t size = configFile.size();
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  configFile.close();
  
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, buf.get());
  if (error) return false;
  
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
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 30000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("   ✅ Connected! IP: %s | Channel: %d\n", WiFi.localIP().toString().c_str(), WiFi.channel());
  } else {
    Serial.println("   ❌ Failed to connect to WiFi");
  }
}

void setupTime() {
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    Serial.print("   ✅ Time: ");
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  }
}

void setupOTA() {
  String hostname = "lumina-gateway-" + deviceID;
  ArduinoOTA.setHostname(hostname.c_str());
  ArduinoOTA.begin();
}