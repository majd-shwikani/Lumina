#include "globals.h"

// ============================================================================
// FIREBASE SETUP & CONFIGURATION
// ============================================================================

void setupFirebase() {
  Serial.println("   Configuring Firebase...");
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_SECRET;
  config.timeout.serverResponse = 15 * 1000; // Increase timeout to 15s
  
  fbdoStream.setResponseSize(2048);
  fbdoSender.setResponseSize(2048);
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  Firebase.reconnectNetwork(true);
  Firebase.setDoubleDigits(5);
  
  Serial.println("   Waiting for connection...");
  delay(1000);
  
  readInitialFirebaseData();

  String streamPath = "/devices/" + deviceID;
  Serial.printf("   Stream path: %s\n", streamPath.c_str());
  
  if (!Firebase.RTDB.beginStream(&fbdoStream, streamPath.c_str())) {
    Serial.printf("   ❌ Stream begin error: %s\n", fbdoStream.errorReason().c_str());
  } else {
    Serial.println("   ✅ Stream initialized");
  }
  
  Firebase.RTDB.setStreamCallback(&fbdoStream, streamCallback, streamTimeoutCallback);
}

void readInitialFirebaseData() {
  Serial.println("   📖 Reading initial Firebase data...");
  
  String localPath = basePath + "/local";
  
  String effectPath = localPath + "/effect";
  if (Firebase.RTDB.getInt(&fbdoSender, effectPath.c_str())) {
    currentEffect = fbdoSender.intData();
    Serial.printf("      Effect: %d\n", currentEffect);
  } else {
    Serial.printf("      ⚠️  Failed to read effect: %s\n", fbdoSender.errorReason().c_str());
    currentEffect = 0;
    Firebase.RTDB.setInt(&fbdoSender, effectPath.c_str(), currentEffect);
  }
  
  String speedPath = localPath + "/speed";
  if (Firebase.RTDB.getInt(&fbdoSender, speedPath.c_str())) {
    effectSpeed = fbdoSender.intData();
    Serial.printf("      Speed: %d\n", effectSpeed);
  } else {
    Serial.printf("      ⚠️  Failed to read speed: %s\n", fbdoSender.errorReason().c_str());
    effectSpeed = 50;
    Firebase.RTDB.setInt(&fbdoSender, speedPath.c_str(), effectSpeed);
  }
  
  String brightnessPath = localPath + "/brightness";
  if (Firebase.RTDB.getInt(&fbdoSender, brightnessPath.c_str())) {
    globalBrightness = fbdoSender.intData();
    FastLED.setBrightness(globalBrightness);
    Serial.printf("      Brightness: %d\n", globalBrightness);
  } else {
    globalBrightness = 255;
    Firebase.RTDB.setInt(&fbdoSender, brightnessPath.c_str(), globalBrightness);
  }
  
  String colorPath = localPath + "/color";
  if (Firebase.RTDB.getString(&fbdoSender, colorPath.c_str())) {
    String colorStr = fbdoSender.stringData();
    if (colorStr.length() == 6) {
      effectColor = strtoul(colorStr.c_str(), NULL, 16);
      Serial.printf("      Color: %s\n", colorStr.c_str());
    }
  } else {
    Serial.printf("      ⚠️  Failed to read color: %s\n", fbdoSender.errorReason().c_str());
    effectColor = 0xFF0000;
    Firebase.RTDB.setString(&fbdoSender, colorPath.c_str(), "FF0000");
  }
  
  String enabledPath = localPath + "/enabled";
  if (Firebase.RTDB.getBool(&fbdoSender, enabledPath.c_str())) {
    stripEnabled = fbdoSender.boolData();
    Serial.printf("      Enabled: %s\n", stripEnabled ? "true" : "false");
    
    if (!stripEnabled) {
      manuallyTurnedOff = true;
      Serial.println("      ⚠️  Device booted with LEDs disabled - manual lock active");
      FastLED.clear();
      FastLED.show();
    } else {
      manuallyTurnedOff = false;
    }
  } else {
    Serial.printf("      ⚠️  Failed to read enabled state: %s\n", fbdoSender.errorReason().c_str());
    stripEnabled = true;
    manuallyTurnedOff = false;
    Firebase.RTDB.setBool(&fbdoSender, enabledPath.c_str(), stripEnabled);
  }
  
  String autoDarknessPath = localPath + "/auto_darkness_control";
  if (Firebase.RTDB.getBool(&fbdoSender, autoDarknessPath.c_str())) {
    autoDarknessControl = fbdoSender.boolData();
    Serial.printf("      Auto Darkness: %s\n", autoDarknessControl ? "true" : "false");
  } else {
    autoDarknessControl = true;
    Firebase.RTDB.setBool(&fbdoSender, autoDarknessPath.c_str(), autoDarknessControl);
  }
  
  String presenceEnabledPath = localPath + "/presence_detection_enabled";
  if (Firebase.RTDB.getBool(&fbdoSender, presenceEnabledPath.c_str())) {
    presenceDetectionEnabled = fbdoSender.boolData();
    Serial.printf("      Presence Detection: %s\n", presenceDetectionEnabled ? "true" : "false");
  } else {
    presenceDetectionEnabled = true;
    Firebase.RTDB.setBool(&fbdoSender, presenceEnabledPath.c_str(), presenceDetectionEnabled);
  }
  
  String luxThresholdPath = localPath + "/lux_threshold";
  if (Firebase.RTDB.getFloat(&fbdoSender, luxThresholdPath.c_str())) {
    luxThreshold = fbdoSender.floatData();
    Serial.printf("      Lux Threshold: %.2f\n", luxThreshold);
  } else {
    luxThreshold = 1.0;
    Firebase.RTDB.setFloat(&fbdoSender, luxThresholdPath.c_str(), luxThreshold);
  }
  
  String timerOnPath = localPath + "/timer_on";
  if (Firebase.RTDB.getString(&fbdoSender, timerOnPath.c_str())) {
    String timeStr = fbdoSender.stringData();
    if (timeStr.length() == 5) {
      strncpy(timerOnTime, timeStr.c_str(), sizeof(timerOnTime));
      Serial.printf("      Timer ON: %s\n", timerOnTime);
    }
  } else {
    strcpy(timerOnTime, "09:00");
    Firebase.RTDB.setString(&fbdoSender, timerOnPath.c_str(), timerOnTime);
  }
  
  String timerOffPath = localPath + "/timer_off";
  if (Firebase.RTDB.getString(&fbdoSender, timerOffPath.c_str())) {
    String timeStr = fbdoSender.stringData();
    if (timeStr.length() == 5) {
      strncpy(timerOffTime, timeStr.c_str(), sizeof(timerOffTime));
      Serial.printf("      Timer OFF: %s\n", timerOffTime);
    }
  } else {
    strcpy(timerOffTime, "17:00");
    Firebase.RTDB.setString(&fbdoSender, timerOffPath.c_str(), timerOffTime);
  }
  
  String timerEnabledPath = localPath + "/timer_enabled";
  if (Firebase.RTDB.getBool(&fbdoSender, timerEnabledPath.c_str())) {
    timerEnabled = fbdoSender.boolData();
    Serial.printf("      Timer Enabled: %s\n", timerEnabled ? "true" : "false");
  } else {
    timerEnabled = true;
    Firebase.RTDB.setBool(&fbdoSender, timerEnabledPath.c_str(), timerEnabled);
  }

  String screenMirrorPath = localPath + "/screenMirror";
  if (Firebase.RTDB.getBool(&fbdoSender, screenMirrorPath.c_str())) {
    bool mirrorState = fbdoSender.boolData();
    Serial.printf("      Screen Mirror: %s\n", mirrorState ? "true" : "false");
    toggleUsbMirror(mirrorState, false);
  } else {
    Firebase.RTDB.setBool(&fbdoSender, screenMirrorPath.c_str(), false);
  }
  
  String resetPath = localPath + "/reset";
  Firebase.RTDB.setBool(&fbdoSender, resetPath.c_str(), false);
  
  String micCalibPath = localPath + "/mic_calibration";
  Firebase.RTDB.setBool(&fbdoSender, micCalibPath.c_str(), false);
  
  String versionPath = localPath + "/version";
  if (Firebase.RTDB.setString(&fbdoSender, versionPath.c_str(), currentFirmwareVersion)) {
    Serial.printf("      ✅ Version published: %s\n", currentFirmwareVersion);
  }
  
  Serial.println("   ✅ Initial data loaded");
  broadcastGatewayState();
}

void streamCallback(FirebaseStream data) {
  String dataPath = data.dataPath().c_str();
  
  // 1. HANDLE LOCAL SETTINGS
  if (dataPath.startsWith("/local")) {
    String subPath = dataPath.substring(6); // Remove "/local"
    
    if (subPath == "/effect") {
      currentEffect = data.intData();
      Serial.printf("🔥 [Firebase] Effect → %d\n", currentEffect);
      syncAllMirrors();
    }
    else if (subPath == "/speed") {
      effectSpeed = data.intData();
      Serial.printf("🔥 [Firebase] Speed → %d ms\n", effectSpeed);
      syncAllMirrors();
    }
    else if (subPath == "/brightness") {
      globalBrightness = data.intData();
      FastLED.setBrightness(globalBrightness);
      Serial.printf("🔥 [Firebase] Brightness → %d\n", globalBrightness);
      syncAllMirrors();
    }
    else if (subPath == "/color") {
      String colorStr = data.stringData();
      if (colorStr.length() == 6) {
        effectColor = strtoul(colorStr.c_str(), NULL, 16);
        Serial.printf("🔥 [Firebase] Color → #%s\n", colorStr.c_str());
        syncAllMirrors();
      }
    }
    else if (subPath == "/enabled") {
      bool newState = data.boolData();
      portENTER_CRITICAL(&stripMux);
      stripEnabled = newState;
      portEXIT_CRITICAL(&stripMux);
      manuallyTurnedOff = !newState;
      Serial.printf("🔥 [Firebase] Power → %s\n", newState ? "ON" : "OFF");
      syncAllMirrors();
    }
    else if (subPath == "/lux_threshold") {
      luxThreshold = data.floatData();
      Serial.printf("🔥 [Firebase] Lux Threshold → %.2f\n", luxThreshold);
    }
    else if (subPath == "/auto_darkness_control") {
      autoDarknessControl = data.boolData();
      Serial.printf("🔥 [Firebase] Auto Darkness → %s\n", autoDarknessControl ? "ON" : "OFF");
    }
    else if (subPath == "/presence_detection_enabled") {
      presenceDetectionEnabled = data.boolData();
      Serial.printf("🔥 [Firebase] Presence Detection → %s\n", presenceDetectionEnabled ? "ON" : "OFF");
    }
    else if (subPath == "/timer_enabled") {
      timerEnabled = data.boolData();
      Serial.printf("🔥 [Firebase] Timer → %s\n", timerEnabled ? "ON" : "OFF");
    }
    else if (subPath == "/timer_on") {
      strncpy(timerOnTime, data.stringData().c_str(), sizeof(timerOnTime));
      Serial.printf("🔥 [Firebase] Timer ON → %s\n", timerOnTime);
    }
    else if (subPath == "/timer_off") {
      strncpy(timerOffTime, data.stringData().c_str(), sizeof(timerOffTime));
      Serial.printf("🔥 [Firebase] Timer OFF → %s\n", timerOffTime);
    }
    else if (subPath == "/screenMirror") {
      bool newState = data.boolData();
      Serial.printf("🔥 [Firebase] Screen Mirror → %s\n", newState ? "ON" : "OFF");
      toggleUsbMirror(newState, false);
    }
    else if (subPath.startsWith("/mqtt")) {
      Serial.println("🔥 [Firebase] MQTT config changed → signalling update...");
      pendingMQTTConfigUpdate = true;
    }
    else if (subPath == "/reset" && data.boolData() == true) {
      Serial.println("🔥 [Firebase] RESET command received → restarting...");
      if (SPIFFS.exists("/config.json")) SPIFFS.remove("/config.json");
      ESP.restart();
    }

  }
  
  // 2. HANDLE RECEIVER SETTINGS
  else if (dataPath.startsWith("/receivers")) {
    // Expected path: /receivers/AA:BB:CC:DD:EE:FF/property
    String remaining = dataPath.substring(11); // Remove "/receivers/"
    int nextSlash = remaining.indexOf("/");
    
    if (nextSlash != -1) {
      String macStr = remaining.substring(0, nextSlash);
      String property = remaining.substring(nextSlash);
      
      // Find receiver in registry
      int idx = -1;
      for (int i = 0; i < receiverCount; i++) {
        if (receivers[i].macStr == macStr) {
          idx = i;
          break;
        }
      }
      
      if (idx != -1) {
        if (property == "/effect") receivers[idx].effect = data.intData();
        else if (property == "/speed") receivers[idx].speed = data.intData();
        else if (property == "/brightness") receivers[idx].brightness = data.intData();
        else if (property == "/color") {
          String colorStr = data.stringData();
          if (colorStr.length() == 6) receivers[idx].color = strtoul(colorStr.c_str(), NULL, 16);
        }
        else if (property == "/enabled") receivers[idx].enabled = data.boolData();
        else if (property == "/isMirror") receivers[idx].isMirror = data.boolData();
        
        Serial.printf("🔥 [Firebase] Receiver %s → %s\n", macStr.c_str(), property.c_str() + 1);
        // Route command if not mirroring (mirroring handled by local changes)
        if (!receivers[idx].isMirror) {
          routeCommandToReceiver(idx);
        } else {
          // If just switched to mirror, sync now
          syncAllMirrors();
        }
      }
    }
  }
}

void streamTimeoutCallback(bool timeout) {
  if (timeout) {
    Serial.println("⚠️  Stream timeout, resuming...");
  }
  if (!fbdoStream.httpConnected()) {
    Serial.printf("❌ Stream error: %s\n", fbdoStream.errorReason().c_str());
  }
}

void createDefaultFirebaseData() {
  String localPath = basePath + "/local";
  String testPath = localPath + "/effect";
  
  if (Firebase.RTDB.getInt(&fbdoSender, testPath.c_str())) {
    Serial.println("Firebase data already exists, skipping default data creation");
    defaultDataCreated = true;
    return;
  }
  
  Serial.println("Creating default Firebase structure for device: " + deviceID);
  
  bool allSuccess = true;
  
  if (Firebase.RTDB.setInt(&fbdoSender, (localPath + "/effect").c_str(), 0)) {
    Serial.println("Effect set to default: 0");
  } else {
    allSuccess = false;
  }
  
  if (Firebase.RTDB.setInt(&fbdoSender, (localPath + "/speed").c_str(), 50)) {
    Serial.println("Speed set to default: 50");
  } else {
    allSuccess = false;
  }
  
  if (Firebase.RTDB.setInt(&fbdoSender, (localPath + "/brightness").c_str(), 255)) {
    Serial.println("Brightness set to default: 255");
  } else {
    allSuccess = false;
  }
  
  if (Firebase.RTDB.setString(&fbdoSender, (localPath + "/color").c_str(), "FF0000")) {
    Serial.println("Color set to default: FF0000");
  } else {
    allSuccess = false;
  }
  
  if (Firebase.RTDB.setBool(&fbdoSender, (localPath + "/enabled").c_str(), true)) {
    Serial.println("Enabled set to default: true");
  } else {
    allSuccess = false;
  }
  
  if (Firebase.RTDB.setBool(&fbdoSender, (localPath + "/auto_darkness_control").c_str(), true)) {
    Serial.println("Auto darkness control set to default: true");
  } else {
    allSuccess = false;
  }
  
  if (Firebase.RTDB.setBool(&fbdoSender, (localPath + "/presence_detection_enabled").c_str(), true)) {
    Serial.println("Presence detection enabled set to default: true");
  } else {
    allSuccess = false;
  }
  
  if (Firebase.RTDB.setFloat(&fbdoSender, (localPath + "/lux_threshold").c_str(), 1.0)) {
    Serial.println("Lux threshold set to default: 1.0");
  } else {
    allSuccess = false;
  }
  
  if (Firebase.RTDB.setString(&fbdoSender, (localPath + "/timer_on").c_str(), "09:00")) {
    Serial.println("Timer ON set to default: 09:00");
  } else {
    allSuccess = false;
  }
  
  if (Firebase.RTDB.setString(&fbdoSender, (localPath + "/timer_off").c_str(), "17:00")) {
    Serial.println("Timer OFF set to default: 17:00");
  } else {
    allSuccess = false;
  }
  
  if (Firebase.RTDB.setBool(&fbdoSender, (localPath + "/timer_enabled").c_str(), true)) {
    Serial.println("Timer enabled set to default: true");
  } else {
    allSuccess = false;
  }
  
  if (Firebase.RTDB.setBool(&fbdoSender, (localPath + "/reset").c_str(), false)) {
    Serial.println("Reset flag set to default: false");
  } else {
    allSuccess = false;
  }
  
  if (Firebase.RTDB.setBool(&fbdoSender, (localPath + "/mic_calibration").c_str(), false)) {
    Serial.println("Mic calibration flag set to default: false");
  } else {
    allSuccess = false;
  }
  
  defaultDataCreated = allSuccess;
  
  if (allSuccess) {
    Serial.println("Default Firebase data structure created successfully");
  } else {
    Serial.println("Some default values failed to set. Check Firebase rules.");
  }
}

// ============================================================================
// GITHUB OTA UPDATE FUNCTIONS
// ============================================================================

void checkForGitHubUpdate() {
  Serial.println("🔍 Checking for GitHub firmware update...");
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("   ⚠️  WiFi not connected for GitHub OTA check");
    return;
  }

  esp_task_wdt_reset();

  String latestVersion = fetchLatestVersion();
  if (latestVersion == "" || latestVersion.length() == 0) {
    Serial.println("   ❌ Failed to fetch latest version from GitHub");
    return;
  }

  if (currentFirmwareVersion == NULL || strlen(currentFirmwareVersion) == 0) {
    Serial.println("   ❌ Current firmware version is invalid");
    return;
  }

  Serial.printf("   Current: %s\n", currentFirmwareVersion);
  Serial.printf("   Latest: %s\n", latestVersion.c_str());

  if (latestVersion != currentFirmwareVersion) {
    int curMajor = 0, curMinor = 0, curPatch = 0;
    int latMajor = 0, latMinor = 0, latPatch = 0;
    
    sscanf(currentFirmwareVersion, "%d.%d.%d", &curMajor, &curMinor, &curPatch);
    sscanf(latestVersion.c_str(), "%d.%d.%d", &latMajor, &latMinor, &latPatch);
    
    bool shouldUpdate = false;
    if (latMajor > curMajor) {
      shouldUpdate = true;
    } else if (latMajor == curMajor && latMinor > curMinor) {
      shouldUpdate = true;
    } else if (latMajor == curMajor && latMinor == curMinor && latPatch > curPatch) {
      shouldUpdate = true;
    }
    
    if (shouldUpdate) {
      Serial.println("   ✅ New firmware available. Starting OTA update...");
      downloadAndApplyFirmware();
    } else {
      Serial.println("   ℹ️  GitHub version is same or older. Skipping update.");
    }
  } else {
    Serial.println("   ✅ Device is up to date.");
  }
}

String fetchLatestVersion() {
  HTTPClient http;
  http.begin(GITHUB_VERSION_URL);

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String latestVersion = http.getString();
    latestVersion.trim();
    http.end();
    return latestVersion;
  } else {
    Serial.printf("   ❌ Failed to fetch version. HTTP code: %d\n", httpCode);
    http.end();
    return "";
  }
}

void downloadAndApplyFirmware() {
  Serial.println("📥 Downloading firmware from GitHub...");
  
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(GITHUB_FIRMWARE_URL);

  int httpCode = http.GET();
  Serial.printf("   HTTP GET code: %d\n", httpCode);

  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    Serial.printf("   Firmware size: %d bytes (%.2f KB)\n", contentLength, contentLength / 1024.0);

    if (contentLength > 0) {
      FastLED.clear();
      FastLED.show();
      Serial.println("   💡 LEDs turned off for OTA process...");

      WiFiClient* stream = http.getStreamPtr();
      if (startGitHubOTAUpdate(stream, contentLength)) {
        Serial.println("   ✅ GitHub OTA update successful, restarting...");
        delay(2000);
        ESP.restart();
      } else {
        Serial.println("   ❌ GitHub OTA update failed");
      }
    } else {
      Serial.println("   ❌ Invalid firmware size for GitHub OTA");
    }
  } else {
    Serial.printf("   ❌ Failed to fetch firmware from GitHub. HTTP code: %d\n", httpCode);
  }
  http.end();
}

bool startGitHubOTAUpdate(WiFiClient* client, int contentLength) {
  Serial.println("   🔄 Initializing GitHub update...");
  if (!Update.begin(contentLength)) {
    Serial.printf("   ❌ Update begin failed: %s\n", Update.errorString());
    return false;
  }

  Serial.println("   📝 Writing firmware...");
  size_t written = 0;
  int progress = 0;
  int lastProgress = 0;

  // Allocate larger buffer in PSRAM
  const size_t bufferSize = 16384; // 16KB buffer
  uint8_t *buffer = (uint8_t *)ps_malloc(bufferSize);
  if (!buffer) {
    Serial.println("   ⚠️  Failed to allocate PSRAM buffer, falling back to stack");
  }

  const unsigned long timeoutDuration = 10 * 1000;
  unsigned long lastDataTime = millis();

  while (written < contentLength) {
    esp_task_wdt_reset();
    
    if (client->available()) {
      size_t toRead = client->available();
      if (toRead > (buffer ? bufferSize : 128)) toRead = (buffer ? bufferSize : 128);
      
      size_t len = client->read(buffer ? buffer : (uint8_t*)alloca(128), toRead);
      if (len > 0) {
        Update.write(buffer ? buffer : (uint8_t*)alloca(128), len);
        written += len;
        lastDataTime = millis();

        progress = (written * 100) / contentLength;
        if (progress != lastProgress) {
          Serial.printf("      Progress: %d%%\r", progress);
          lastProgress = progress;
        }
      }
    }
    
    if (millis() - lastDataTime > timeoutDuration) {
      Serial.println("\n   ❌ Timeout: No data received. Aborting update...");
      if (buffer) free(buffer);
      Update.abort();
      return false;
    }

    yield();
  }
  if (buffer) free(buffer);
  Serial.println("\n   ✅ Writing complete");

  if (written != contentLength) {
    Serial.printf("   ❌ Error: Write incomplete. Expected %d but got %d bytes\n", contentLength, written);
    Update.abort();
    return false;
  }

  if (!Update.end()) {
    Serial.printf("   ❌ Error: Update end failed: %s\n", Update.errorString());
    return false;
  }

  Serial.println("   ✅ Update successfully completed");
  return true;
}