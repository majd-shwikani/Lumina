#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include "globals.h"

// OTA Function Declarations
void handleOTAUpdate();
void checkForGitHubUpdate();
String fetchLatestVersion();
void downloadAndApplyFirmware();
bool startGitHubOTAUpdate(WiFiClient* client, int contentLength);

#endif
