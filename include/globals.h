#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <WiFi.h>
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
// CRITICAL FIX 1: Thread synchronization primitives
// ============================================================================
extern portMUX_TYPE stripMux;

// ============================================================================
// DEBUGGING AND SYSTEM MONITORING
// ============================================================================
extern unsigned long lastSystemStatsReport;
extern const unsigned long SYSTEM_STATS_INTERVAL;
extern UBaseType_t ledTaskStack;
extern UBaseType_t otaTaskStack;

extern TaskHandle_t ledTaskHandle;
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
// DEVICE CONFIGURATION FROM SPIFFS
// ============================================================================
extern String deviceID;
extern String wifiSSID;
extern String wifiPassword;
extern int ledCount;
extern String basePath;

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
void connectToWiFi();
void setupTime();
void setupOTA();
void updateLEDs();
void checkForGitHubUpdate();
String fetchLatestVersion();
bool startGitHubOTAUpdate(WiFiClient* client, int contentLength);
void downloadAndApplyFirmware();
void ledTask(void *parameter);
void otaUpdateTask(void *parameter);

#endif // GLOBALS_H
