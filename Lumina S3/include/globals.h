#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Firebase_ESP_Client.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <FastLED.h>
#include <Adafruit_VEML7700.h>
#include <esp_task_wdt.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <driver/i2s.h>
#include <ld2410.h>
#include <esp_system.h>
#include <rom/rtc.h>
#include <addons/TokenHelper.h>

#include "config.h"
#include "effects.h"
#include "sensors.h"
#include "mqtt_integration.h"

// ============================================================================
// FIREBASE MESSAGING STRUCTURES
// ============================================================================
enum FirebaseMethod {
    FB_SET_BOOL,
    FB_SET_INT,
    FB_SET_FLOAT,
    FB_SET_STRING,
    FB_SET_JSON
};

struct FirebaseRequest {
    FirebaseMethod method;
    char path[128];
    char payload[128]; // Increased size for JSON payloads if needed
};

extern QueueHandle_t firebaseQueue;
bool enqueueFirebaseRequest(FirebaseMethod method, const String& path, const String& payload);

// Flags for centralized processing in cloudTask
extern volatile bool pendingMQTTConfigUpdate;
extern volatile bool pendingTimerUpdate;
extern volatile bool targetTimerState;

// ============================================================================
// ESP-NOW STRUCTURES (Must match Receiver exactly)
// ============================================================================
typedef struct {
    char msgType[20];   // "LUMINA_DISCOVERY", "LUMINA_OFFER", "LUMINA_CMD"
    uint8_t targetMac[6]; // [0,0,0,0,0,0] for broadcast
    int effect;
    uint32_t speed;
    uint32_t color;
    uint8_t brightness;
    bool enabled;
} LuminaMessage;

typedef struct {
    uint8_t mac[6];
    String macStr;
    int effect;
    uint32_t speed;
    uint32_t color;
    uint8_t brightness;
    bool enabled;
    bool isMirror;
    bool registered;
    bool needsFirebaseSync; // Flag to signal cloud update
    bool mqttDiscoveryPublished; // Flag to signal MQTT discovery publish
} Receiver;

extern Receiver receivers[10];
extern int receiverCount;
extern volatile bool registryChanged; // Global flag for firebase task
void routeCommandToReceiver(int index);
void syncAllMirrors();
void updateActiveNodesInFirebase();

// ============================================================================
// CRITICAL FIX 1: Thread synchronization primitives
// ============================================================================
extern portMUX_TYPE stripMux;
extern SemaphoreHandle_t i2sMutex;

// ============================================================================
// DEBUGGING AND SYSTEM MONITORING
// ============================================================================
extern unsigned long lastSystemStatsReport;
extern const unsigned long SYSTEM_STATS_INTERVAL;

// Task Stacks (Now tracking high water marks for consolidated tasks)
extern UBaseType_t cloudTaskStack;
extern UBaseType_t ledTaskStack;
extern UBaseType_t ioTaskStack;
extern UBaseType_t systemTaskStack;

// Task Handles
extern TaskHandle_t cloudTaskHandle;
extern TaskHandle_t ledTaskHandle;
extern TaskHandle_t ioTaskHandle;
extern TaskHandle_t systemTaskHandle;
extern TaskHandle_t usbDataTaskHandle;
extern TaskHandle_t audioTaskHandle;

extern unsigned long lastLoopTime;
extern unsigned long loopCounter;
extern bool systemHealthy;
extern bool systemInitialized;
extern volatile bool configPortalActive;
extern volatile bool usbMirrorActive;
extern volatile bool usbPixelStreamActive;
extern volatile bool audioMirrorMode;
extern volatile uint32_t lastUsbAudioTime;
extern volatile uint32_t lastUsbPixelTime;

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================
extern CRGB *leds;
extern CRGB onboardLed[1];
extern FirebaseData fbdoStream;
extern FirebaseData fbdoSender;
extern FirebaseAuth auth;
extern FirebaseConfig config;

// ============================================================================
// LED ANIMATION CONTROL VARIABLES
// ============================================================================
extern volatile int currentEffect;
extern volatile uint32_t effectSpeed;
extern volatile uint32_t effectColor;
extern volatile uint8_t globalBrightness;
extern volatile bool updateEffect;
extern volatile bool firebaseConnected;
extern volatile bool stripEnabled;
extern volatile bool autoDarknessControl;
extern volatile bool turnedOffByDarkness;
extern bool defaultDataCreated;
extern volatile bool manuallyTurnedOff;

// ============================================================================
// AUTOMATION CONTROL VARIABLES
// ============================================================================
extern unsigned long lastStateChangeTime;
extern const unsigned long STATE_CHANGE_DEBOUNCE;
extern float luxHysteresis;
extern unsigned long luxReadDelayAfterChange;

// ============================================================================
// PRESENCE DETECTION VARIABLES
// ============================================================================
extern ld2410 radar;
extern volatile bool lastPresence;
extern volatile bool presenceDetectionEnabled;
extern volatile bool lastPresenceState;
extern unsigned long lastPresenceReport;

// ============================================================================
// TIMER SETTINGS
// ============================================================================
extern char timerOnTime[6];
extern char timerOffTime[6];
extern bool timerEnabled;

// ============================================================================
// DEVICE CONFIGURATION FROM SPIFFS
// ============================================================================
extern String deviceID;
extern String wifiSSID;
extern String wifiPassword;
extern int ledCount;
extern String basePath;

// Sinric Pro Credentials
extern String sinricAppKey;
extern String sinricAppSecret;
extern String sinricLightID;

// ============================================================================
// GITHUB OTA UPDATE CONFIGURATION
// ============================================================================
extern const char* GITHUB_FIRMWARE_URL;
extern const char* GITHUB_VERSION_URL;
extern const char* currentFirmwareVersion;
extern const unsigned long UPDATE_CHECK_INTERVAL;
extern unsigned long lastUpdateCheck;

// ============================================================================
// BUTTON PRESS DETECTION
// ============================================================================
extern unsigned long buttonPressStart;
extern bool buttonActive;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================
const char* getResetReason(int cpu);
void printSystemStats();
void initSPIFFS();
void checkBootCount();
void clearBootCount(void *parameter);
bool loadConfig();
bool shouldStartConfigPortal();
void startConfigPortal();
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
void cloudTask(void *parameter);
void ledTask(void *parameter);
void systemTask(void *parameter);
void ioTask(void *parameter);
void usbDataTask(void *parameter);
void toggleUsbMirror(bool enable, bool isAudio);
void audioProcessingTask(void *parameter);
String formatUptime(unsigned long milliseconds);

// ESP-NOW Gateway Functions
void setupEspNowGateway();
void broadcastGatewayState();
void onDataRecvGateway(const uint8_t *mac, const uint8_t *data, int len);

#endif // GLOBALS_H
