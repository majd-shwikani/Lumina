#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <FastLED.h>
#include <esp_task_wdt.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <esp_system.h>
#include <rom/rtc.h>

#include "config.h"
#include "effects.h"

// ============================================================================
// ESP-NOW STRUCTURES
// ============================================================================
typedef struct {
    char msgType[20];   // "LUMINA_DISCOVERY", "LUMINA_OFFER", "LUMINA_CMD"
    uint8_t targetMac[6]; // [0,0,0,0,0,0] for broadcast
    int effect;
    uint32_t speed;
    uint32_t color;
    bool enabled;
} LuminaMessage;

enum OperatingMode {
    MODE_MIRROR,
    MODE_STANDALONE
};

extern OperatingMode currentMode;

// ============================================================================
// CRITICAL FIX 1: Thread synchronization primitives
// ============================================================================
extern portMUX_TYPE stripMux;

// ============================================================================
// DEBUGGING AND SYSTEM MONITORING
// ============================================================================
extern unsigned long lastSystemStatsReport;
extern const unsigned long SYSTEM_STATS_INTERVAL;
extern UBaseType_t ledTaskStack;
extern UBaseType_t discoveryTaskStack;

extern TaskHandle_t ledTaskHandle;
extern TaskHandle_t discoveryTaskHandle;
extern TaskHandle_t otaTaskHandle;

extern unsigned long lastLoopTime;
extern unsigned long loopCounter;
extern bool systemHealthy;

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================
extern CRGB *leds;

// ============================================================================
// LED ANIMATION CONTROL VARIABLES
// ============================================================================
extern volatile int currentEffect;
extern volatile uint32_t effectSpeed;
extern volatile uint32_t effectColor;
extern volatile bool updateEffect;
extern volatile bool stripEnabled;

// ============================================================================
// ESP-NOW & DISCOVERY VARIABLES
// ============================================================================
extern uint8_t gatewayMAC[6];
extern bool gatewayFound;
extern unsigned long lastGatewayContact;
extern const unsigned long GATEWAY_TIMEOUT;
extern int currentWifiChannel;

// ============================================================================
// DEVICE CONFIGURATION FROM SPIFFS
// ============================================================================
extern String deviceID;
extern String wifiSSID;
extern String wifiPassword;
extern int ledCount;

// ============================================================================
// GITHUB OTA UPDATE CONFIGURATION
// ============================================================================
extern const char* GITHUB_FIRMWARE_URL;
extern const char* GITHUB_VERSION_URL;
extern const char* currentFirmwareVersion;
extern const unsigned long UPDATE_CHECK_INTERVAL;
extern unsigned long lastUpdateCheck;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================
const char* getResetReason(int cpu);
void printSystemStats();
void initSPIFFS();
bool loadConfig();
bool shouldStartConfigPortal();
void startConfigPortal();
void connectToWiFi();
void setupEspNow();
void discoveryTask(void *parameter);
void OnDataRecv(const uint8_t *mac, const uint8_t *data, int len);
void OnDataSent(const uint8_t *mac, esp_now_send_status_t status);

void checkForGitHubUpdate();
String fetchLatestVersion();
bool startGitHubOTAUpdate(WiFiClient* client, int contentLength);
void downloadAndApplyFirmware();

void updateLEDs();
void ledTask(void *parameter);
void otaUpdateTask(void *parameter);

#endif // GLOBALS_H
