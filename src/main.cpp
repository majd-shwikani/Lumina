#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <Adafruit_VEML7700.h>
#include "effects.h"
#include <esp_task_wdt.h>  // Add this line for watchdog functions

// Include configuration file
#include "config.h"

// Provide the token generation process info.
#include <addons/TokenHelper.h>

// VEML7700 configuration
volatile float luxThreshold = 1.0;  // Default value, will be controlled via Firebase

// Global variables
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_VEML7700 veml = Adafruit_VEML7700();
FirebaseData fbdoStream; // used only for stream and callbacks
FirebaseData fbdoUpload; // used for setFloat, setBool and all write ops

// Timer variables
char timerOnTime[6] = "09:00";  // HH:MM format
char timerOffTime[6] = "17:00"; // HH:MM format
bool timerEnabled = true;  // Timer enabled by default

FirebaseAuth auth;
FirebaseConfig config;

// Animation control variables
volatile int currentEffect = 0;
volatile uint32_t effectSpeed = 50;
volatile uint32_t effectColor = 0xFF0000;
volatile bool updateEffect = false;
volatile bool firebaseConnected = false;
volatile bool stripEnabled = true;  // Manual on/off control
volatile bool autoDarknessControl = true;  // Auto turn off in darkness
volatile bool turnedOffByDarkness = false;

// Sensor data
volatile float currentLux = 0;
volatile bool sensorAvailable = false;

// Function declarations
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
  
  // Initialize NeoPixel strip
  strip.begin();
  strip.show();
  strip.setBrightness(100);
  esp_task_wdt_init(30, true); // 30 seconds timeout, panic on trigger
  // Initialize I2C for VEML7700
  Wire.begin();
  
  // Connect to WiFi
  connectToWiFi();
  
  // Setup time
  setupTime();
  
  // Setup OTA
  setupOTA();
  
  // Setup VEML7700 sensor
  setupVEML7700();
  
  // Setup Firebase
  setupFirebase();
  
  xTaskCreatePinnedToCore(
    firebaseTask,
    "FirebaseTask",
    15000,  // Increased from 10000
    NULL,
    1,
    NULL,
    0
  );

  xTaskCreatePinnedToCore(
    ledTask,
    "LEDTask",
    15000,  // Increased from 10000
    NULL,
    1,
    NULL,
    1
  );

  xTaskCreatePinnedToCore(
    sensorDataTask,
    "SensorDataTask",
    15000,   // Increased from 4000
    NULL,
    1,
    NULL,
    0
  );

  xTaskCreatePinnedToCore(
    automationtask,
    "AutomationTask",
    4000,
    NULL,
    0,
    NULL,
    0
  );

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
  // Empty - everything is handled by FreeRTOS tasks
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}

void connectToWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
}

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

void setupOTA() {
  ArduinoOTA.setHostname("esp32-neopixel-controller");
  
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
  Serial.println("OTA Ready");
}

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

void setupFirebase() {
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_SECRET;
  
  // Optional, set AP reconnection
  config.timeout.serverResponse = 10 * 1000;
  
  fbdoStream.setResponseSize(2048);
  
  // Connect to Firebase
  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);
  
  // Wait for Firebase connection
  Serial.println("Connecting to Firebase...");
  delay(1000);
  
  // Read initial values from Firebase instead of creating defaults
  readInitialFirebaseData();

  //createDefaultFirebaseData();
  
  // Start stream listener for real-time updates
  if (!Firebase.RTDB.beginStream(&fbdoStream, "/")) {
    Serial.printf("Stream begin error: %s\n", fbdoStream.errorReason().c_str());
  }
  
  Firebase.RTDB.setStreamCallback(&fbdoStream, streamCallback, streamTimeoutCallback);
  Serial.println("Firebase initialized with stream and initial data reading");
}

void readInitialFirebaseData() {
  Serial.println("Reading initial Firebase data...");
  
  // Read effect
  if (Firebase.RTDB.getInt(&fbdoUpload, "/effect")) {
    currentEffect = fbdoUpload.intData();
    Serial.printf("Initial effect: %d\n", currentEffect);
  } else {
    Serial.printf("Failed to read effect, using default: %s\n", fbdoUpload.errorReason().c_str());
    currentEffect = 0;
  }
  
  // Read speed
  if (Firebase.RTDB.getInt(&fbdoUpload, "/speed")) {
    effectSpeed = fbdoUpload.intData();
    Serial.printf("Initial speed: %d\n", effectSpeed);
  } else {
    Serial.printf("Failed to read speed, using default: %s\n", fbdoUpload.errorReason().c_str());
    effectSpeed = 50;
  }
  
  // Read color
  if (Firebase.RTDB.getString(&fbdoUpload, "/color")) {
    String colorStr = fbdoUpload.stringData();
    if (colorStr.length() == 6) {
      effectColor = strtoul(colorStr.c_str(), NULL, 16);
      Serial.printf("Initial color: %s\n", colorStr.c_str());
    }
  } else {
    Serial.printf("Failed to read color, using default: %s\n", fbdoUpload.errorReason().c_str());
    effectColor = 0xFF0000;
  }
  
  // Read enabled state
  if (Firebase.RTDB.getBool(&fbdoUpload, "/enabled")) {
    stripEnabled = fbdoUpload.boolData();
    Serial.printf("Initial enabled state: %s\n", stripEnabled ? "true" : "false");
    
    // If disabled, turn off LEDs immediately
    if (!stripEnabled) {
      strip.clear();
      strip.show();
    }
  } else {
    Serial.printf("Failed to read enabled state, using default: %s\n", fbdoUpload.errorReason().c_str());
    stripEnabled = true;
  }
  
  // Read auto darkness control
  if (Firebase.RTDB.getBool(&fbdoUpload, "/auto_darkness_control")) {
    autoDarknessControl = fbdoUpload.boolData();
    Serial.printf("Initial auto darkness control: %s\n", autoDarknessControl ? "true" : "false");
  } else {
    Serial.printf("Failed to read auto darkness control, using default: %s\n", fbdoUpload.errorReason().c_str());
    autoDarknessControl = true;
  }
  
  // Read lux threshold
  if (Firebase.RTDB.getFloat(&fbdoUpload, "/lux_threshold")) {
    luxThreshold = fbdoUpload.floatData();
    Serial.printf("Initial lux threshold: %.2f\n", luxThreshold);
  } else {
    Serial.printf("Failed to read lux threshold, using default: %s\n", fbdoUpload.errorReason().c_str());
    luxThreshold = 1.0;
  }
  
  // Read timer on time
  if (Firebase.RTDB.getString(&fbdoUpload, "/timer_on")) {
    String timeStr = fbdoUpload.stringData();
    if (timeStr.length() == 5) {
      strncpy(timerOnTime, timeStr.c_str(), sizeof(timerOnTime));
      Serial.printf("Initial timer on time: %s\n", timerOnTime);
    }
  } else {
    Serial.printf("Failed to read timer on time, using default: %s\n", fbdoUpload.errorReason().c_str());
    strcpy(timerOnTime, "09:00");
  }
  
  // Read timer off time
  if (Firebase.RTDB.getString(&fbdoUpload, "/timer_off")) {
    String timeStr = fbdoUpload.stringData();
    if (timeStr.length() == 5) {
      strncpy(timerOffTime, timeStr.c_str(), sizeof(timerOffTime));
      Serial.printf("Initial timer off time: %s\n", timerOffTime);
    }
  } else {
    Serial.printf("Failed to read timer off time, using default: %s\n", fbdoUpload.errorReason().c_str());
    strcpy(timerOffTime, "17:00");
  }
  // Read timer enabled state
if (Firebase.RTDB.getBool(&fbdoUpload, "/timer_enabled")) {
  timerEnabled = fbdoUpload.boolData();
  Serial.printf("Initial timer enabled: %s\n", timerEnabled ? "true" : "false");
} else {
  Serial.printf("Failed to read timer enabled, using default: %s\n", fbdoUpload.errorReason().c_str());
  timerEnabled = true;
}
}

void firebaseTask(void *parameter) {
  for(;;) {
    if (WiFi.status() == WL_CONNECTED) {
      ArduinoOTA.handle();
      
      // Keep Firebase connection alive
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

void ledTask(void *parameter) {
  for(;;) {
    updateLEDs();
    vTaskDelay(effectSpeed / portTICK_PERIOD_MS);
  }
}

void automationtask(void *parameter) {
  const TickType_t xDelay = 100 / portTICK_PERIOD_MS;

  for(;;) {
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
  const TickType_t xDelay = 2000 / portTICK_PERIOD_MS; // 2 second delay
  
  for(;;) {
    if (firebaseConnected) {
      // Update all sensor data first
      if (sensorAvailable) {
        updateSensorData();
        
        // Send lux data to Firebase
        if (Firebase.RTDB.setFloat(&fbdoUpload, "/lux", currentLux)) {
          Serial.printf("Lux data sent: %.2f\n", currentLux);
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

void timerTask(void *parameter) {
  const TickType_t xDelay = 1000 / portTICK_PERIOD_MS; // Check every second
  bool lastOnTriggered = false;
  bool lastOffTriggered = false;
  
  for(;;) {
    if (firebaseConnected && timerEnabled) {
      // Check if it's time to turn on
      if (checkTimeMatch(timerOnTime)) {
        if (!stripEnabled && !lastOnTriggered) {
          Serial.println("Timer ON triggered");
          updateTimerState(true);
          lastOnTriggered = true;
        }
      } else {
        lastOnTriggered = false;
      }
      
      // Check if it's time to turn off
      if (checkTimeMatch(timerOffTime)) {
        if (stripEnabled && !lastOffTriggered) {
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

void updateTimerState(bool state) {
  esp_task_wdt_reset();
  if (Firebase.RTDB.setBool(&fbdoUpload, "/enabled", state)) {
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

void updateSensorData() {
  currentLux = veml.readLux();
}

bool shouldTurnOffDueToDarkness() {
  return currentLux < luxThreshold;
}

void streamCallback(FirebaseStream data) {
  Serial.printf("Stream data path: %s, event: %s, type: %s, value: %s\n",
                data.streamPath().c_str(),
                data.dataType().c_str(),
                data.eventType().c_str(),
                data.stringData().c_str());

  // Handle effect changes
  if (data.dataPath() == "/effect") {
    currentEffect = data.intData();
    Serial.printf("Effect changed to: %d\n", currentEffect);
  }
  // Handle speed changes
  else if (data.dataPath() == "/speed") {
    effectSpeed = data.intData();
    Serial.printf("Speed changed to: %d\n", effectSpeed);
  }
  // Handle color changes
  else if (data.dataPath() == "/color") {
    String colorStr = data.stringData();
    if (colorStr.length() == 6) {
      effectColor = strtoul(colorStr.c_str(), NULL, 16);
      Serial.printf("Color changed to: %s\n", colorStr.c_str());
    }
  }
  else if (data.dataPath() == "/lux_threshold") {
    luxThreshold = data.floatData();
    Serial.printf("Lux threshold changed to: %.2f\n", luxThreshold);
  }
  else if (data.dataPath() == "/enabled") {
    bool newState = data.boolData();
    
    stripEnabled = newState;
    turnedOffByDarkness = false;  // Reset when user manually toggles
    Serial.printf("Strip %s\n", stripEnabled ? "enabled" : "disabled");
    if (!stripEnabled) {
      strip.clear();
      strip.show();
    }
  }
  // Handle timer enabled changes
  else if (data.dataPath() == "/timer_enabled") {
    timerEnabled = data.boolData();
    Serial.printf("Timer %s\n", timerEnabled ? "enabled" : "disabled");
  }
  // Handle auto darkness control
  else if (data.dataPath() == "/auto_darkness_control") {
    autoDarknessControl = data.boolData();
    Serial.printf("Auto darkness control %s\n", autoDarknessControl ? "enabled" : "disabled");
  }
  // Handle timer on time changes
  else if (data.dataPath() == "/timer_on") {
    String timeStr = data.stringData();
    if (timeStr.length() == 5) {
      strncpy(timerOnTime, timeStr.c_str(), sizeof(timerOnTime));
      Serial.printf("Timer ON time changed to: %s\n", timerOnTime);
    }
  }
  // Handle timer off time changes
  else if (data.dataPath() == "/timer_off") {
    String timeStr = data.stringData();
    if (timeStr.length() == 5) {
      strncpy(timerOffTime, timeStr.c_str(), sizeof(timerOffTime));
      Serial.printf("Timer OFF time changed to: %s\n", timerOffTime);
    }
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

void updateLEDs() {
  // Check if we should turn off due to darkness (auto control)
  if (autoDarknessControl && sensorAvailable && shouldTurnOffDueToDarkness()) {
    strip.clear();
    strip.show();
    return;
  }
  
  // Manual control check
  if (!stripEnabled) {
    strip.clear();
    strip.show();
    return;
  }
  
  // Run the selected animation effect
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

void createDefaultFirebaseData() {
  Serial.println("Creating default Firebase data structure...");
  
  // Set default effect
  if (Firebase.RTDB.setInt(&fbdoUpload, "/effect", 0)) {
    Serial.println("Effect set to default: 0");
  } else {
    Serial.printf("Failed to set effect: %s\n", fbdoUpload.errorReason().c_str());
  }
  
  // Set default speed
  if (Firebase.RTDB.setInt(&fbdoUpload, "/speed", 50)) {
    Serial.println("Speed set to default: 50");
  } else {
    Serial.printf("Failed to set speed: %s\n", fbdoUpload.errorReason().c_str());
  }
  
  // Set default color
  if (Firebase.RTDB.setString(&fbdoUpload, "/color", "FF0000")) {
    Serial.println("Color set to default: FF0000");
  } else {
    Serial.printf("Failed to set color: %s\n", fbdoUpload.errorReason().c_str());
  }
  
  // Set default enabled state
  if (Firebase.RTDB.setBool(&fbdoUpload, "/enabled", true)) {
    Serial.println("Enabled set to default: true");
  } else {
    Serial.printf("Failed to set enabled: %s\n", fbdoUpload.errorReason().c_str());
  }
  
  // Set default auto darkness control
  if (Firebase.RTDB.setBool(&fbdoUpload, "/auto_darkness_control", true)) {
    Serial.println("Auto darkness control set to default: true");
  } else {
    Serial.printf("Failed to set auto darkness control: %s\n", fbdoUpload.errorReason().c_str());
  }
  
  if (Firebase.RTDB.setFloat(&fbdoUpload, "/lux_threshold", 1.0)) {
    Serial.println("Lux threshold set to default: 1.0");
  } else {
    Serial.printf("Failed to set lux threshold: %s\n", fbdoUpload.errorReason().c_str());
  }
  
  // Set default timer on time
  if (Firebase.RTDB.setString(&fbdoUpload, "/timer_on", "09:00")) {
    Serial.println("Timer ON set to default: 09:00");
  } else {
    Serial.printf("Failed to set timer ON: %s\n", fbdoUpload.errorReason().c_str());
  }
  
  // Set default timer off time
  if (Firebase.RTDB.setString(&fbdoUpload, "/timer_off", "17:00")) {
    Serial.println("Timer OFF set to default: 17:00");
  } else {
    Serial.printf("Failed to set timer OFF: %s\n", fbdoUpload.errorReason().c_str());
  }
  // Set default timer enabled
  if (Firebase.RTDB.setBool(&fbdoUpload, "/timer_enabled", true)) {
  Serial.println("Timer enabled set to default: true");
  } else {
  Serial.printf("Failed to set timer enabled: %s\n", fbdoUpload.errorReason().c_str());
  }
}