#include "globals.h"
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <Update.h>

// ============================================================================
// GLOBAL VARIABLE DEFINITIONS (from setup_utils.cpp)
// ============================================================================
WebServer server(80);
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

// Global MQTT objects (Defined in main.cpp)
WiFiClient espClient;
PubSubClient mqttClient(espClient);
MQTTConfig mqttConfig = {0};
volatile bool mqttConnected = false;
String deviceTopic = "";

// MQTT state tracking (Defined in main.cpp)
unsigned long lastMQTTStatePublish = 0;
unsigned long MQTT_STATE_PUBLISH_INTERVAL = 5000;
unsigned long lastMQTTSensorPublish = 0;
unsigned long MQTT_SENSOR_PUBLISH_INTERVAL = 2000;

// Centralized Effect List (Defined in main.cpp)
const char* EFFECT_NAMES[] = {
    "Rainbow", "Meteor Shower", "Digital Rain", "Pulsing Spheres", "Binary Clock",
    "Vortex", "DNA Helix", "Audio Visualizer", "Lava Lamp", "Radar Sweep",
    "Quantum Particles", "Neural Network", "Galaxy Spin", "Crystal Growth",
    "Lightning Storm", "Ocean Depth", "Northern Lights", "Time Tunnel",
    "Cyber City", "Solar Flare", "Fire Simulation", "Solid Color",
    "Frequency Spectrum", "Reactive Waveform", "Beat Pulse", "Frequency Bloom",
    "Audio Reactive Fire", "Musical Rainbow", "Reactive Strobe", "Guitar Visualizer",
    "Cascading Frequency", "Energy Orbits", "Audio Ripples"
};
const int NUM_EFFECTS = sizeof(EFFECT_NAMES) / sizeof(EFFECT_NAMES[0]);

unsigned long lastLoopTime = 0;
unsigned long loopCounter = 0;
bool systemHealthy = true;
CRGB *leds = nullptr;
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

// Light Sensor (from sensors.cpp)
Adafruit_VEML7700 veml = Adafruit_VEML7700();
volatile float currentLux = 0;
volatile bool sensorAvailable = false;
volatile float luxThreshold = 1.0;

// Audio Sensor (from sensors.cpp)
volatile int activeMicrophone = 0; // MIC_I2S_ICS43434
volatile bool calibrationComplete = false;
volatile double noiseFloor = 100.0;
volatile double gainMultiplier = 5.0; // INITIAL_GAIN_MULTIPLIER
volatile bool triggerMicCalibration = false;
volatile bool isCalibrating = false;

// FFT & Audio Analysis (from sensors.cpp)
int16_t raw_samples[256]; // BUFFER_LEN
ArduinoFFT<double> FFT = ArduinoFFT<double>();
double vReal[256];
double vImag[256];
double bandMagnitudes[8] = {0}; // NUM_FREQ_BANDS
double bandMaxima[8] = {0};
double frequencyResponse[256] = {0};
volatile double detectedFrequency = 0;
volatile double frequencyMagnitude = 0;
volatile double globalAudioLevel = 0;
volatile double bassLevel = 0;
volatile double midLevel = 0;
volatile double trebleLevel = 0;
volatile bool beatDetected = false;
volatile float beatEnergy = 0;
double smoothedBandMagnitudes[8] = {0};
unsigned long lastAudioUpdate = 0;

// Calibration buffers
double calibrationSamples[256] = {0};
int calibrationSampleCount = 0;
unsigned long calibrationStartTime = 0;
unsigned long lastCalibrationTime = 0;

// Audio Smoothing
const unsigned long AUDIO_UPDATE_INTERVAL = 20;

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
const char* currentFirmwareVersion = "1.2.0";
const unsigned long UPDATE_CHECK_INTERVAL = 10 * 60 * 1000;
unsigned long lastUpdateCheck = 0;
unsigned long buttonPressStart = 0;
bool buttonActive = false;

// ============================================================================
// SYSTEM MONITORING & UTILS (from setup_utils.cpp)
// ============================================================================
const char* getResetReason(int cpu) {
  RESET_REASON reason = rtc_get_reset_reason(cpu);
  switch (reason) {
    case 1:  return "POWERON_RESET"; case 3:  return "SW_RESET"; case 4:  return "OWDT_RESET";
    case 5:  return "DEEPSLEEP_RESET"; case 6:  return "SDIO_RESET"; case 7:  return "TG0WDT_SYS_RESET";
    case 8:  return "TG1WDT_SYS_RESET"; case 9:  return "RTCWDT_SYS_RESET"; case 10: return "INTRUSION_RESET";
    case 11: return "TGWDT_CPU_RESET"; case 12: return "SW_CPU_RESET"; case 13: return "RTCWDT_CPU_RESET";
    case 14: return "EXT_CPU_RESET"; case 15: return "RTCWDT_BROWN_OUT_RESET"; case 16: return "RTCWDT_RTC_RESET";
    default: return "UNKNOWN";
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
  Serial.printf("║   Brightness:      %-45d ║\n", FastLED.getBrightness());
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

  Serial.println("║ TASK STATUS:                                                         ║");
  Serial.printf("║   Firebase Task:   %-45s ║\n", firebaseTaskHandle ? "RUNNING" : "STOPPED");
  Serial.printf("║   LED Task:        %-45s ║\n", ledTaskHandle ? "RUNNING" : "STOPPED");
  Serial.printf("║   Automation Task: %-45s ║\n", automationTaskHandle ? "RUNNING" : "STOPPED");
  Serial.printf("║   Sensor Task:     %-45s ║\n", sensorTaskHandle ? "RUNNING" : "STOPPED");
  Serial.printf("║   Timer Task:      %-45s ║\n", timerTaskHandle ? "RUNNING" : "STOPPED");
  Serial.printf("║   MQTT Task:       %-45s ║\n", mqttTaskHandle ? "RUNNING" : "STOPPED");
  Serial.printf("║   OTA Task:        %-45s ║\n", otaTaskHandle ? "RUNNING" : "STOPPED");
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
  if (!firebaseTaskHandle) {
    Serial.println("║   ⚠️  WARNING: Firebase task not running!                            ║");
    warningsFound = true;
  }
  if (!ledTaskHandle) {
    Serial.println("║   ⚠️  WARNING: LED task not running!                                 ║");
    warningsFound = true;
  }
  if (!sensorTaskHandle) {
    Serial.println("║   ⚠️  WARNING: Sensor task not running!                              ║");
    warningsFound = true;
  }
  if (!automationTaskHandle) {
    Serial.println("║   ⚠️  WARNING: Automation task not running!                          ║");
    warningsFound = true;
  }
  if (!timerTaskHandle) {
    Serial.println("║   ⚠️  WARNING: Timer task not running!                               ║");
    warningsFound = true;
  }
  if (!mqttTaskHandle) {
    Serial.println("║   ⚠️  WARNING: MQTT task not running!                                ║");
    warningsFound = true;
  }
  if (!otaTaskHandle) {
    Serial.println("║   ⚠️  WARNING: OTA task not running!                                 ║");
    warningsFound = true;
  }
  if (!warningsFound) {
    Serial.println("║   ✅ All systems nominal                                             ║");
  }

  Serial.println("╚══════════════════════════════════════════════════════════════════════╝\n");
}

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

bool shouldStartConfigPortal() { return !SPIFFS.exists("/config.json"); }

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
      FastLED.clear();
      FastLED.show();
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
// TASKS & CORE LOGIC (from tasks.cpp)
// ============================================================================
void updateLEDs() {
  static bool lastStripEnabled = true;
  bool currentEnabled;
  portENTER_CRITICAL(&stripMux); currentEnabled = stripEnabled; portEXIT_CRITICAL(&stripMux);
  if (!currentEnabled) { if (lastStripEnabled) { FastLED.clear(); FastLED.show(); lastStripEnabled = false; } return; }
  lastStripEnabled = true;
  switch(currentEffect) {
    case 0: effectRainbow(); break; case 1: effectMeteorShower(); break; case 2: effectDigitalRain(); break;
    case 3: effectPulsingSpheres(); break; case 4: effectBinaryClock(); break; case 5: effectVortex(); break;
    case 6: effectDNAHelix(); break; case 7: effectAudioVisualizer(); break; case 8: effectLavaLamp(); break;
    case 9: effectRadarSweep(); break; case 10: effectQuantumParticles(); break; case 11: effectNeuralNetwork(); break;
    case 12: effectGalaxySpin(); break; case 13: effectCrystalGrowth(); break; case 14: effectLightningStorm(); break;
    case 15: effectOceanDepth(); break; case 16: effectNorthernLights(); break; case 17: effectTimeTunnel(); break;
    case 18: effectCyberCity(); break; case 19: effectSolarFlare(); break; case 20: effectFireSimulation(); break;
    case 21: effectSolidColor(); break; case 22: effectFrequencySpectrum(); break; case 23: effectReactiveWaveform(); break;
    case 24: effectBeatPulse(); break; case 25: effectFrequencyBloom(); break; case 26: effectAudioReactiveFire(); break;
    case 27: effectMusicalRainbow(); break; case 28: effectReactiveStrobe(); break; case 29: effectGuitarVisualizer(); break;
    case 30: effectCascadingFrequency(); break; case 31: effectEnergyOrbits(); break; case 32: effectAudioRipples(); break;
    default: effectRainbow(); break;
  }
  FastLED.show();
}

bool checkTimeMatch(const char* scheduledTime) {
  struct tm timeinfo; if (!getLocalTime(&timeinfo)) return false;
  char currentTime[6]; strftime(currentTime, sizeof(currentTime), "%H:%M", &timeinfo);
  return strcmp(currentTime, scheduledTime) == 0;
}

void updateTimerState(bool state) {
  if (Firebase.RTDB.setBool(&fbdoUpload, (basePath + "/enabled").c_str(), state)) {
    portENTER_CRITICAL(&stripMux); stripEnabled = state; manuallyTurnedOff = !state; portEXIT_CRITICAL(&stripMux);
  }
}

String formatUptime(unsigned long milliseconds) {
  unsigned long seconds = milliseconds / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  unsigned long days = hours / 24;
  
  if (days > 0) {
    return String(days) + "d " + String(hours % 24) + "h";
  } else if (hours > 0) {
    return String(hours) + "h " + String(minutes % 60) + "m";
  } else if (minutes > 0) {
    return String(minutes) + "m " + String(seconds % 60) + "s";
  } else {
    return String(seconds) + "s";
  }
}

void firebaseTask(void *parameter) {
  for(;;) {
    esp_task_wdt_reset();
    if (WiFi.status() == WL_CONNECTED) {
      ArduinoOTA.handle();
      firebaseConnected = Firebase.ready();
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void ledTask(void *parameter) {
  for(;;) { esp_task_wdt_reset(); updateLEDs(); vTaskDelay(effectSpeed / portTICK_PERIOD_MS); }
}

void automationtask(void *parameter) {
  Serial.println("⚙️  Automation Task started on Core " + String(xPortGetCoreID()));
  
  const TickType_t xDelay = 100 / portTICK_PERIOD_MS;
  static unsigned long lastStateChangeTime = 0;
  const unsigned long CHANGE_LOCKOUT_MS = 3000;
  
  static bool lastPresenceDetected = false;
  static bool lastTargetState = false;
  static unsigned long lastLogTime = 0;
  
  vTaskDelay(3000 / portTICK_PERIOD_MS);

  for(;;) {
    esp_task_wdt_reset();
    
    if (sensorAvailable) {
      updateSensorData();
    }
    bool presenceDetected = (digitalRead(RADAR_OUTPUT) == HIGH);
    lastPresence = presenceDetected;

    if (manuallyTurnedOff) {
      bool currentEnabled;
      portENTER_CRITICAL(&stripMux);
      currentEnabled = stripEnabled;
      portEXIT_CRITICAL(&stripMux);
      
      if (currentEnabled) {
        portENTER_CRITICAL(&stripMux);
        stripEnabled = false;
        portEXIT_CRITICAL(&stripMux);
      }
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
      darknessCondition = (currentLux < luxThreshold);
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

    bool currentEnabled;
    portENTER_CRITICAL(&stripMux);
    currentEnabled = stripEnabled;
    portEXIT_CRITICAL(&stripMux);

    if (presenceDetected != lastPresenceDetected || targetState != lastTargetState || (millis() - lastLogTime > 5000)) {
      Serial.printf("[Automation] State: presence: %d, presenceEnabled: %d, darknessEnabled: %d, lux: %.2f | Target: %d, Current: %d, Reason: %s\n",
          presenceDetected, presenceDetectionEnabled, autoDarknessControl, currentLux, targetState, currentEnabled, reason.c_str());
      lastPresenceDetected = presenceDetected;
      lastTargetState = targetState;
      lastLogTime = millis();
    }

    if (targetState != currentEnabled) {
      if (millis() - lastStateChangeTime > CHANGE_LOCKOUT_MS) {
        portENTER_CRITICAL(&stripMux);
        stripEnabled = targetState;
        portEXIT_CRITICAL(&stripMux);
        
        lastStateChangeTime = millis();
        
        if (targetState) {
          Serial.printf("\n💡 LEDs ON: %s (Presence: %s, Lux: %.2f)\n", 
                       reason.c_str(), presenceDetected ? "Yes" : "No", currentLux);
        } else {
          Serial.printf("\n🌙 LEDs OFF: %s (Presence: %s, Lux: %.2f)\n", 
                       reason.c_str(), presenceDetected ? "Yes" : "No", currentLux);
        }
      }
    }

    vTaskDelay(xDelay);
  }
}

void sensorDataTask(void *parameter) {
  Serial.println("📊 Sensor+Stats Task started on Core " + String(xPortGetCoreID()));
  Serial.println("   📡 Sending combined data every 2 seconds to /sensor_stats");
  
  const TickType_t xDelay = 2000 / portTICK_PERIOD_MS; // Every 2 seconds
  const unsigned long FIREBASE_OPERATION_TIMEOUT = 10000;
  const unsigned long NETWORK_CHECK_TIMEOUT = 5000;
  
  static unsigned long lastNetworkCheck = 0;
  static unsigned long taskStartTime = millis(); // For uptime calculation
  
  unsigned long operationStart = 0;
  unsigned long operationDuration = 0;
  
  int consecutiveFailures = 0;
  const int MAX_CONSECUTIVE_FAILURES = 5;
  
  for(;;) {
    esp_task_wdt_reset();
    
    if (millis() - lastNetworkCheck > NETWORK_CHECK_TIMEOUT) {
      lastNetworkCheck = millis();
      
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("⚠️  [SensorTask] WiFi disconnected - skipping Firebase operations");
        firebaseConnected = false;
        esp_task_wdt_reset();
        vTaskDelay(xDelay);
        continue;
      }
      
      if (!Firebase.ready()) {
        Serial.println("⚠️  [SensorTask] Firebase not ready - skipping operations");
        firebaseConnected = false;
        esp_task_wdt_reset();
        vTaskDelay(xDelay);
        continue;
      }
    }
    
    if (sensorAvailable) {
      esp_task_wdt_reset();
      updateSensorData(); 
    }
    
    esp_task_wdt_reset();
    bool present = (digitalRead(RADAR_OUTPUT) == HIGH);
    lastPresence = present; 
    
    if (firebaseConnected && WiFi.status() == WL_CONNECTED) {
      esp_task_wdt_reset();
      operationStart = millis();
      
      uint32_t freeHeap = ESP.getFreeHeap();
      uint32_t totalHeap = ESP.getHeapSize();
      uint32_t minFreeHeap = ESP.getMinFreeHeap();
      uint32_t usedHeap = totalHeap - freeHeap;
      float heapUsagePercent = (totalHeap > 0) ? (usedHeap * 100.0) / totalHeap : 0;
      
      #ifdef BOARD_HAS_PSRAM
        uint32_t psramSize = ESP.getPsramSize();
        uint32_t freePsram = ESP.getFreePsram();
      #else
        uint32_t psramSize = 0;
        uint32_t freePsram = 0;
      #endif

      FirebaseJson json;
      FirebaseJson sensors;
      FirebaseJson stats;
      
      sensors.set("lux", currentLux);
      sensors.set("presence", present);
      
      stats.set("uptime", formatUptime(millis() - taskStartTime));
      stats.set("status", systemHealthy ? "ALL SYSTEMS NOMINAL" : "SYSTEM WARNINGS");
      stats.set("heap_usage_percent", heapUsagePercent);
      stats.set("free_heap_kb", freeHeap / 1024);
      stats.set("min_free_heap", minFreeHeap);
      stats.set("total_heap_kb", totalHeap / 1024);
      stats.set("wifi_rssi", WiFi.RSSI());
      stats.set("loop_counter", loopCounter);
      stats.set("firmware_version", currentFirmwareVersion);
      
      if (psramSize > 0) {
        stats.set("psram_size_kb", psramSize / 1024);
        stats.set("free_psram_kb", freePsram / 1024);
        stats.set("min_free_psram", ESP.getMinFreePsram());
      }
      
      json.set("sensors", sensors);
      json.set("stats", stats);
      
      Firebase.RTDB.setJSON(&fbdoUpload, (basePath + "/sensor_stats").c_str(), &json);
    }
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}

void timerTask(void *parameter) {
  bool lastOn = false, lastOff = false;
  for(;;) {
    esp_task_wdt_reset();
    if (firebaseConnected && timerEnabled) {
      if (checkTimeMatch(timerOnTime)) { if (!lastOn) { updateTimerState(true); lastOn = true; } } else lastOn = false;
      if (checkTimeMatch(timerOffTime)) { if (!lastOff) { updateTimerState(false); lastOff = true; } } else lastOff = false;
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void otaUpdateTask(void *parameter) {
  for(;;) {
    esp_task_wdt_reset();
    if (WiFi.status() == WL_CONNECTED && firebaseConnected) checkForGitHubUpdate();
    vTaskDelay(UPDATE_CHECK_INTERVAL / portTICK_PERIOD_MS);
  }
}

// ============================================================================

// MAIN SETUP & LOOP

// ============================================================================

void setup() {

  Serial.begin(115200);

  delay(1000);

  

  Serial.println("\n\n");

  Serial.println("╔═══════════════════════════════════════════════════════════════╗");

  Serial.println("║                    LUMINA LED CONTROLLER                       ║");

  Serial.println("║                     Firmware v" + String(currentFirmwareVersion) + "                          ║");

  Serial.println("╚═══════════════════════════════════════════════════════════════╝");

  Serial.println();

  

  Serial.println("🔄 BOOT INFORMATION:");

  Serial.printf("   CPU0 Reset Reason: %s\n", getResetReason(0));

  Serial.printf("   CPU1 Reset Reason: %s\n", getResetReason(1));

  Serial.println();

  

  Serial.println("📁 Initializing SPIFFS...");

  initSPIFFS();

  

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.println("🔘 Button configured on pin " + String(BUTTON_PIN));



  if (shouldStartConfigPortal()) {

    Serial.println("⚙️  No configuration found. Starting config portal...");

    startConfigPortal();

    return;

  }



  Serial.println("📖 Loading configuration...");

  if (!loadConfig()) {

    Serial.println("❌ Failed to load config, restarting...");

    delay(3000);

    ESP.restart();

    return;

  }



  Serial.println("\n╔═══════════════════════════════════════════════════════════════╗");

  Serial.println("║                  DEVICE CONFIGURATION                          ║");

  Serial.println("╠═══════════════════════════════════════════════════════════════╣");

  Serial.printf("║   Device ID: %-48s ║\n", deviceID.c_str());

  Serial.printf("║   Base Path: %-47s ║\n", basePath.c_str());

  Serial.printf("║   LED Count: %-47d ║\n", ledCount);

  Serial.printf("║   WiFi SSID: %-47s ║\n", wifiSSID.c_str());

  Serial.println("╚═══════════════════════════════════════════════════════════════╝\n");



  Serial.println("💡 Initializing LED strip...");

  leds = new CRGB[ledCount];

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, ledCount);

  FastLED.setBrightness(255);

  FastLED.show();

  Serial.printf("   ✅ %d LEDs initialized\n", ledCount);



  Serial.println("⏱️  Initializing watchdog timer (30s)...");

  esp_task_wdt_init(30, true);

  Serial.println("   ✅ Watchdog configured");



  Serial.println("🔌 Initializing I2C bus...");

  Wire.begin();

  Serial.println("   ✅ I2C ready");



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

  pinMode(RADAR_OUTPUT, INPUT);

  Serial.println("      ✅ LD2410 radar presence detection on GPIO " + String(RADAR_OUTPUT));



  Serial.println("⚙️  [9/9] Creating FreeRTOS tasks...");



  xTaskCreatePinnedToCore(firebaseTask, "FBTask", 4000, NULL, 1, &firebaseTaskHandle, 0);

  Serial.println("      ✅ Firebase task created (Core 0, 4KB stack)");



  xTaskCreatePinnedToCore(ledTask, "LEDTask", 4000, NULL, 1, &ledTaskHandle, 1);

  Serial.println("      ✅ LED task created (Core 1, 4KB stack)");



  xTaskCreatePinnedToCore(sensorDataTask, "SensTask", 12000, NULL, 1, &sensorTaskHandle, 0);        

  Serial.println("      ✅ Sensor task created (Core 0, 12KB stack)");



  xTaskCreatePinnedToCore(automationtask, "AutoTask", 4000, NULL, 0, &automationTaskHandle, 0);     

  Serial.println("      ✅ Automation task created (Core 0, 4KB stack)");



  xTaskCreatePinnedToCore(timerTask, "TimeTask", 4000, NULL, 1, &timerTaskHandle, 0);

  Serial.println("      ✅ Timer task created (Core 0, 4KB stack)");



  xTaskCreatePinnedToCore(mqttTask, "MQTTTask", 8000, NULL, 1, &mqttTaskHandle, 0);

  Serial.println("      ✅ MQTT task created (Core 0, 4KB stack)");



  xTaskCreatePinnedToCore(otaUpdateTask, "OTATask", 7000, NULL, 0, &otaTaskHandle, 0);

  Serial.println("      ✅ OTA Update task created (Core 0, 7KB stack)");



  Serial.println("\n╔═══════════════════════════════════════════════════════════════╗");

  Serial.println("║              🎉 ALL SYSTEMS INITIALIZED 🎉                     ║");

  Serial.println("║       Combined sensor+stats data sent every 2 seconds          ║");

  Serial.println("╚═══════════════════════════════════════════════════════════════╝\n");

}



void loop() {

  unsigned long loopStart = millis();

  loopCounter++;



  if (digitalRead(BUTTON_PIN) == LOW) {

    if (!buttonActive) {

      buttonActive = true;

      buttonPressStart = millis();

      Serial.println("\n🔘 Button pressed - hold for 7 seconds to reset config");

    }



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



  if (millis() - lastSystemStatsReport > SYSTEM_STATS_INTERVAL) {

    lastSystemStatsReport = millis();



    if (firebaseTaskHandle != NULL) firebaseTaskStack = uxTaskGetStackHighWaterMark(firebaseTaskHandle);

    if (ledTaskHandle != NULL) ledTaskStack = uxTaskGetStackHighWaterMark(ledTaskHandle);

    if (automationTaskHandle != NULL) automationTaskStack = uxTaskGetStackHighWaterMark(automationTaskHandle);

    if (sensorTaskHandle != NULL) sensorTaskStack = uxTaskGetStackHighWaterMark(sensorTaskHandle);

    if (timerTaskHandle != NULL) timerTaskStack = uxTaskGetStackHighWaterMark(timerTaskHandle);

    if (mqttTaskHandle != NULL) mqttTaskStack = uxTaskGetStackHighWaterMark(mqttTaskHandle);

    if (otaTaskHandle != NULL) otaTaskStack = uxTaskGetStackHighWaterMark(otaTaskHandle);



    printSystemStats();

  }



  lastLoopTime = millis();

  unsigned long loopDuration = lastLoopTime - loopStart;



  if (loopDuration > 1000) {

    Serial.printf("⚠️  WARNING: Loop took %lu ms (expected <100ms)\n", loopDuration);

  }



  vTaskDelay(100 / portTICK_PERIOD_MS);

}


