#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <Adafruit_VEML7700.h>
#include "effects.h"
#include <esp_task_wdt.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <HTTPClient.h> // **NEW: Added for GitHub OTA**
#include <Update.h>     // **NEW: Added for GitHub OTA**
#include <driver/i2s.h>
#include <arduinoFFT.h>


// Project configuration (Firebase, time settings)
#include "config.h"

// Configuration portal
#include "web_config_portal.h"

// Firebase token helper for authentication
#include <addons/TokenHelper.h>

// Light sensor threshold (controlled via Firebase)
volatile float luxThreshold = 1.0;

// Global objects
Adafruit_NeoPixel strip(1, LED_PIN, NEO_GRB + NEO_KHZ800); // Temporary initial value
Adafruit_VEML7700 veml = Adafruit_VEML7700();
FirebaseData fbdoStream;
FirebaseData fbdoUpload;

// Timer settings (HH:MM format)
char timerOnTime[6] = "09:00";
char timerOffTime[6] = "17:00";
bool timerEnabled = true;

// Firebase authentication and configuration
FirebaseAuth auth;
FirebaseConfig config;
bool defaultDataCreated = false;

// LED animation control variables
volatile int currentEffect = 0;
volatile uint32_t effectSpeed = 50;
volatile uint32_t effectColor = 0xFF0000;
volatile bool updateEffect = false;
volatile bool firebaseConnected = false;
volatile bool stripEnabled = true;
volatile bool autoDarknessControl = true;
volatile bool turnedOffByDarkness = false;

// Sensor data
volatile float currentLux = 0;
volatile bool sensorAvailable = false;
#define BUTTON_PIN 0
// Button press detection
unsigned long buttonPressStart = 0;
bool buttonActive = false;

// Device configuration from SPIFFS
String deviceID;
String wifiSSID;
String wifiPassword;
int ledCount;
String basePath;

// **NEW: GitHub OTA Update Configuration**
// NOTE: I'm hardcoding these as per the example, but they should ideally be in config.h or read from Firebase/SPIFFS
const char* GITHUB_FIRMWARE_URL = "https://github.com/majd-shwikani/Lumina-bin/releases/download/Lumina/firmware.bin";
const char* GITHUB_VERSION_URL = "https://raw.githubusercontent.com/majd-shwikani/Lumina-bin/refs/heads/main/version.txt";

const char* currentFirmwareVersion = "1.0.1"; // **UPDATE THIS MANUALLY FOR NEW RELEASES**
//const unsigned long UPDATE_CHECK_INTERVAL = 3600000; // Check every hour (1 hour in ms)
const unsigned long UPDATE_CHECK_INTERVAL =  10*60*1000; // Check every hour (1 hour in ms)
unsigned long lastUpdateCheck = 0;
// **END NEW**

// Add these pin definitions (adjust GPIO numbers as needed)
#define I2S_WS   25
#define I2S_SD   32  
#define I2S_SCK  33
#define I2S_PORT I2S_NUM_0
// Frequency detection settings
#define SAMPLE_RATE 16000
#define N_SAMPLES 256  // Increased for better frequency resolution
#define BUFFER_LEN N_SAMPLES

// Frequency detection variables
int16_t raw_samples[BUFFER_LEN];
ArduinoFFT<double> FFT = ArduinoFFT<double>();
double vReal[N_SAMPLES];
double vImag[N_SAMPLES];
\

extern const int NUM_FREQ_BANDS;
extern double bandMagnitudes[];
extern double frequencyThreshold;

// ADD THESE
 volatile double detectedFrequency; // <-- FIX: added 'volatile'
 volatile double frequencyMagnitude; // <-- FIX: added 'volatile'

// Multi-frequency analysis variables
extern const int NUM_FREQ_BANDS = 8;  // Number of frequency bands to analyze
double bandMagnitudes[NUM_FREQ_BANDS] = {0};
double frequencyThreshold = 1000.0;  // Minimum magnitude to consider a frequency present


// Function declarations
void initSPIFFS();
bool loadConfig();
bool shouldStartConfigPortal();
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

// **NEW: GitHub OTA Function Declarations**
void checkForGitHubUpdate();
String fetchLatestVersion();
bool startGitHubOTAUpdate(WiFiClient* client, int contentLength);
void downloadAndApplyFirmware();
void setupFrequencyDetection();
void updateFrequencyDetection();
uint32_t frequencyToColor(double freq);
// **END NEW**


void setup() {
  Serial.begin(115200);
  
  // Initialize SPIFFS
  initSPIFFS();
  
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Check if we need to start config portal
  if (shouldStartConfigPortal()) {
    Serial.println("No configuration found. Starting config portal...");
    startConfigPortal();
    // This will restart after config is saved
    return;
  }
  
  // Load configuration from SPIFFS
  if (!loadConfig()) {
    Serial.println("Failed to load config, restarting...");
    ESP.restart();
    return;
  }
  
  // Print device ID for verification
  Serial.println("Device ID: " + deviceID);
  Serial.println("Base path: " + basePath);
  Serial.println("LED Count: " + String(ledCount));
  
  // Initialize NeoPixel strip with configured count
  strip.updateLength(ledCount);
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
setupFrequencyDetection();
  
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
  // Check for 7-second button press
  if (digitalRead(BUTTON_PIN) == LOW) { // Button pressed (LOW due to INPUT_PULLUP)
    if (!buttonActive) {
      buttonActive = true;
      buttonPressStart = millis();
      Serial.println("Button pressed - hold for 7 seconds to reset config");
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
  
  // **NEW: Check for GitHub OTA Update**
  if (WiFi.status() == WL_CONNECTED && millis() - lastUpdateCheck > UPDATE_CHECK_INTERVAL) {
    lastUpdateCheck = millis();
    checkForGitHubUpdate();
  }
  // **END NEW**

  vTaskDelay(100 / portTICK_PERIOD_MS); // Small delay to prevent watchdog timeout
}

// Initialize SPIFFS
void initSPIFFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
    return;
  }
  Serial.println("SPIFFS mounted successfully");
}

// Load configuration from SPIFFS
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
  Serial.println("SSID: " + wifiSSID);
  Serial.println("Device ID: " + deviceID);
  Serial.println("LED Count: " + String(ledCount));
  
  return true;
}

// Check if config portal should start
bool shouldStartConfigPortal() {
  return !SPIFFS.exists("/config.json");
}

// Connect to WiFi network using configured credentials
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
    // Continue anyway, might reconnect in task
  }
}

// Synchronize with NTP server for accurate time
void setupTime() {
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  
  Serial.print("Waiting for time synchronization");
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

// Setup Over-The-Air updates (local network)
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

  //createDefaultFirebaseData();
  
  // Stream from the device path
  String streamPath = "/devices/" + deviceID;
  Serial.println("Stream path: " + streamPath);
  
  if (!Firebase.RTDB.beginStream(&fbdoStream, streamPath.c_str())) {
    Serial.printf("Stream begin error: %s\n", fbdoStream.errorReason().c_str());
  }
  
  Firebase.RTDB.setStreamCallback(&fbdoStream, streamCallback, streamTimeoutCallback);
  Serial.println("Firebase initialized with stream and initial data reading");
}

// Read initial values from Firebase on startup
void readInitialFirebaseData() {
  Serial.println("Reading initial Firebase data for device: " + deviceID);
  
  // Read current effect
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
  
  // Read animation speed
  String speedPath = basePath + "/speed";
  if (Firebase.RTDB.getInt(&fbdoUpload, speedPath.c_str())) {
    effectSpeed = fbdoUpload.intData();
    Serial.printf("Initial speed: %d\n", effectSpeed);
  } else {
    Serial.printf("Failed to read speed, using default: %s\n", fbdoUpload.errorReason().c_str());
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
    Serial.printf("Failed to read color, using default: %s\n", fbdoUpload.errorReason().c_str());
    effectColor = 0xFF0000;
    Firebase.RTDB.setString(&fbdoUpload, colorPath.c_str(), "FF0000");
  }
  
  // Read enabled state
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
  
  // Read auto darkness control setting
  String autoDarknessPath = basePath + "/auto_darkness_control";
  if (Firebase.RTDB.getBool(&fbdoUpload, autoDarknessPath.c_str())) {
    autoDarknessControl = fbdoUpload.boolData();
    Serial.printf("Initial auto darkness control: %s\n", autoDarknessControl ? "true" : "false");
  } else {
    Serial.printf("Failed to read auto darkness control, using default: %s\n", fbdoUpload.errorReason().c_str());
    autoDarknessControl = true;
    Firebase.RTDB.setBool(&fbdoUpload, autoDarknessPath.c_str(), autoDarknessControl);
  }
  
  // Read lux threshold for auto-off
  String luxThresholdPath = basePath + "/lux_threshold";
  if (Firebase.RTDB.getFloat(&fbdoUpload, luxThresholdPath.c_str())) {
    luxThreshold = fbdoUpload.floatData();
    Serial.printf("Initial lux threshold: %.2f\n", luxThreshold);
  } else {
    Serial.printf("Failed to read lux threshold, using default: %s\n", fbdoUpload.errorReason().c_str());
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
    Serial.printf("Failed to read timer on time, using default: %s\n", fbdoUpload.errorReason().c_str());
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
    Serial.printf("Failed to read timer off time, using default: %s\n", fbdoUpload.errorReason().c_str());
    strcpy(timerOffTime, "17:00");
    Firebase.RTDB.setString(&fbdoUpload, timerOffPath.c_str(), timerOffTime);
  }
  
  // Read timer enabled state
  String timerEnabledPath = basePath + "/timer_enabled";
  if (Firebase.RTDB.getBool(&fbdoUpload, timerEnabledPath.c_str())) {
    timerEnabled = fbdoUpload.boolData();
    Serial.printf("Initial timer enabled: %s\n", timerEnabled ? "true" : "false");
  } else {
    Serial.printf("Failed to read timer enabled, using default: %s\n", fbdoUpload.errorReason().c_str());
    timerEnabled = true;
    Firebase.RTDB.setBool(&fbdoUpload, timerEnabledPath.c_str(), timerEnabled);
  }
  // Read reset flag (create if doesn't exist)
  String resetPath = basePath + "/reset";
  Firebase.RTDB.setBool(&fbdoUpload, resetPath.c_str(), false);
// NEW: Publish firmware version to version stream
  String versionPath = basePath + "/version";
if (Firebase.RTDB.setString(&fbdoUpload, versionPath.c_str(), currentFirmwareVersion)) {
  Serial.printf("Firmware version published: %s\n", currentFirmwareVersion);
} else {
  Serial.printf("Failed to publish firmware version: %s\n", fbdoUpload.errorReason().c_str());
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

// Sensor data task - reads and reports light levels to Firebase
void sensorDataTask(void *parameter) {
  const TickType_t xDelay = 2000 / portTICK_PERIOD_MS; // 2 second interval
  
  for(;;) {
    if (firebaseConnected) {
      // Update sensor reading
      if (sensorAvailable) {
        updateSensorData();
        
        // Send lux data to Firebase
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

// Update timer state in Firebase and locally
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
   //reset handler
  if (dataPath == "/reset" && data.boolData() == true) {
    Serial.println("Reset command received - deleting config and restarting...");
    
    // Delete config file
    if (SPIFFS.exists("/config.json")) {
      SPIFFS.remove("/config.json");
    }
    
    // Restart to enter config portal
    ESP.restart();
    return;
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
    case 22: effectFrequencyResponse(); break;
    case 23: effectPianoTiles(); break;
    case 24: effectPianoTilesBars(); break;
    default: effectRainbow(); break;
  }
  strip.show();
}

// Create default data structure in Firebase only for new devices
void createDefaultFirebaseData() {
  // Check if device data already exists in Firebase
  String testPath = basePath + "/effect";
  
  if (Firebase.RTDB.getInt(&fbdoUpload, testPath.c_str())) {
    Serial.println("Firebase data already exists, skipping default data creation");
    defaultDataCreated = true;
    return;
  }
  
  Serial.println("No existing Firebase data found. Creating default structure for new device: " + deviceID);
  
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
  
  // Set default reset flag - this is the problematic one
  String resetPath = basePath + "/reset";
  if (Firebase.RTDB.setBool(&fbdoUpload, resetPath.c_str(), false)) {
    Serial.println("Reset flag set to default: false");
  } else {
    Serial.printf("Failed to set reset flag: %s\n", fbdoUpload.errorReason().c_str());
    Serial.printf("Full reset path: %s\n", resetPath.c_str());
    allSuccess = false;
  }
  
  defaultDataCreated = allSuccess;
  
  if (allSuccess) {
    Serial.println("Default Firebase data structure created successfully for new device");
  } else {
    Serial.println("Some default values failed to set. Check Firebase rules and connection.");
  }
}

// --- NEW GITHUB OTA FUNCTIONS ---

/**
 * @brief Checks the GitHub repository for a new firmware version.
 */
void checkForGitHubUpdate() {
  Serial.println("Checking for GitHub firmware update...");
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected for GitHub OTA check");
    return;
  }

  // Step 1: Fetch the latest version from GitHub
  String latestVersion = fetchLatestVersion();
  if (latestVersion == "") {
    Serial.println("Failed to fetch latest version from GitHub");
    return;
  }

  Serial.println("Current Firmware Version: " + String(currentFirmwareVersion));
  Serial.println("Latest Firmware Version: " + latestVersion);

  // Step 2: Compare versions
  if (latestVersion != currentFirmwareVersion) {
    Serial.println("New firmware available on GitHub. Starting OTA update...");
    downloadAndApplyFirmware();
  } else {
    Serial.println("Device is up to date (GitHub check).");
  }
}

/**
 * @brief Fetches the latest firmware version string from the GitHub raw file URL.
 * @return String The latest version string or an empty string on failure.
 */
String fetchLatestVersion() {
  HTTPClient http;
  http.begin(GITHUB_VERSION_URL);

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String latestVersion = http.getString();
    latestVersion.trim();  // Remove any extra whitespace
    http.end();
    return latestVersion;
  } else {
    Serial.printf("Failed to fetch version. HTTP code: %d\n", httpCode);
    http.end();
    return "";
  }
}

/**
 * @brief Downloads the firmware from GitHub and initiates the OTA update.
 */
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
      // Turn off LEDs for visual feedback during update
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

/**
 * @brief Performs the actual OTA update by writing the firmware stream.
 * @param client Pointer to the WiFiClient stream.
 * @param contentLength Expected size of the firmware.
 * @return true if update is successful, false otherwise.
 */
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

  // Reduced timeout duration for a more responsive failure
  const unsigned long timeoutDuration = 10 * 1000; // 10 seconds timeout
  unsigned long lastDataTime = millis();

  while (written < contentLength) {
    if (client->available()) {
      uint8_t buffer[128];
      size_t len = client->read(buffer, sizeof(buffer));
      if (len > 0) {
        Update.write(buffer, len);
        written += len;
        lastDataTime = millis(); // Reset timeout on data reception

        // Calculate and print progress
        progress = (written * 100) / contentLength;
        if (progress != lastProgress) {
          Serial.printf("Writing Progress: %d%%\r", progress);
          lastProgress = progress;
        }
      }
    }
    // Check for timeout
    if (millis() - lastDataTime > timeoutDuration) {
      Serial.println("\nTimeout: No data received for too long. Aborting update...");
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

// --- END NEW GITHUB OTA FUNCTIONS ---
void setupFrequencyDetection() {
  const i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = BUFFER_LEN,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  
  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = -1,
    .data_in_num = I2S_SD
  };
  
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
}
void updateFrequencyDetection() {
  size_t bytes_read = 0;
  i2s_read(I2S_PORT, raw_samples, sizeof(raw_samples), &bytes_read, 0);

  if (bytes_read == sizeof(raw_samples)) {
    // Convert samples to double for FFT
    for (int i = 0; i < N_SAMPLES; i++) {
      vReal[i] = (double)raw_samples[i];
      vImag[i] = 0.0;
    }
    
    // Perform FFT
    FFT.compute(vReal, vImag, N_SAMPLES, FFT_FORWARD);
    FFT.complexToMagnitude(vReal, vImag, N_SAMPLES);
    
    // Reset band magnitudes
    for (int band = 0; band < NUM_FREQ_BANDS; band++) {
      bandMagnitudes[band] = 0.0;
    }
    
    // Analyze frequency bands (divide spectrum into NUM_FREQ_BANDS bands)
    int samplesPerBand = (N_SAMPLES / 2) / NUM_FREQ_BANDS;
    
    for (int band = 0; band < NUM_FREQ_BANDS; band++) {
      double maxInBand = 0.0;
      int startBin = band * samplesPerBand;
      int endBin = startBin + samplesPerBand;
      
      // Find maximum magnitude in this frequency band
      for (int bin = startBin; bin < endBin && bin < N_SAMPLES/2; bin++) {
        if (vReal[bin] > maxInBand) {
          maxInBand = vReal[bin];
        }
      }
      
      bandMagnitudes[band] = maxInBand;
    }
  }
}