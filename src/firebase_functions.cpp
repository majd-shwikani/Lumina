#include "globals.h"

// ============================================================================
// FIREBASE SETUP & CONFIGURATION
// ============================================================================

void setupFirebase() {
  Serial.println("   Configuring Firebase...");
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_SECRET;
  config.timeout.serverResponse = 10 * 1000;
  
  fbdoStream.setResponseSize(2048);
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);
  
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
  
  String effectPath = basePath + "/effect";
  if (Firebase.RTDB.getInt(&fbdoUpload, effectPath.c_str())) {
    currentEffect = fbdoUpload.intData();
    Serial.printf("      Effect: %d\n", currentEffect);
  } else {
    Serial.printf("      ⚠️  Failed to read effect: %s\n", fbdoUpload.errorReason().c_str());
    currentEffect = 0;
    Firebase.RTDB.setInt(&fbdoUpload, effectPath.c_str(), currentEffect);
  }
  
  String speedPath = basePath + "/speed";
  if (Firebase.RTDB.getInt(&fbdoUpload, speedPath.c_str())) {
    effectSpeed = fbdoUpload.intData();
    Serial.printf("      Speed: %d\n", effectSpeed);
  } else {
    Serial.printf("      ⚠️  Failed to read speed: %s\n", fbdoUpload.errorReason().c_str());
    effectSpeed = 50;
    Firebase.RTDB.setInt(&fbdoUpload, speedPath.c_str(), effectSpeed);
  }
  
  String colorPath = basePath + "/color";
  if (Firebase.RTDB.getString(&fbdoUpload, colorPath.c_str())) {
    String colorStr = fbdoUpload.stringData();
    if (colorStr.length() == 6) {
      effectColor = strtoul(colorStr.c_str(), NULL, 16);
      Serial.printf("      Color: %s\n", colorStr.c_str());
    }
  } else {
    Serial.printf("      ⚠️  Failed to read color: %s\n", fbdoUpload.errorReason().c_str());
    effectColor = 0xFF0000;
    Firebase.RTDB.setString(&fbdoUpload, colorPath.c_str(), "FF0000");
  }
  
  // CRITICAL FIX 2: Set manuallyTurnedOff correctly on boot
  String enabledPath = basePath + "/enabled";
  if (Firebase.RTDB.getBool(&fbdoUpload, enabledPath.c_str())) {
    stripEnabled = fbdoUpload.boolData();
    Serial.printf("      Enabled: %s\n", stripEnabled ? "true" : "false");
    
    // CRITICAL: If disabled at boot, set manual lock
    if (!stripEnabled) {
      manuallyTurnedOff = true;
      Serial.println("      ⚠️  Device booted with LEDs disabled - manual lock active");
      strip.clear();
      strip.show();
    } else {
      manuallyTurnedOff = false;
    }
  } else {
    Serial.printf("      ⚠️  Failed to read enabled state: %s\n", fbdoUpload.errorReason().c_str());
    stripEnabled = true;
    manuallyTurnedOff = false;
    Firebase.RTDB.setBool(&fbdoUpload, enabledPath.c_str(), stripEnabled);
  }
  
  String autoDarknessPath = basePath + "/auto_darkness_control";
  if (Firebase.RTDB.getBool(&fbdoUpload, autoDarknessPath.c_str())) {
    autoDarknessControl = fbdoUpload.boolData();
    Serial.printf("      Auto Darkness: %s\n", autoDarknessControl ? "true" : "false");
  } else {
    autoDarknessControl = true;
    Firebase.RTDB.setBool(&fbdoUpload, autoDarknessPath.c_str(), autoDarknessControl);
  }
  
  String presenceEnabledPath = basePath + "/presence_detection_enabled";
  if (Firebase.RTDB.getBool(&fbdoUpload, presenceEnabledPath.c_str())) {
    presenceDetectionEnabled = fbdoUpload.boolData();
    Serial.printf("      Presence Detection: %s\n", presenceDetectionEnabled ? "true" : "false");
  } else {
    presenceDetectionEnabled = true;
    Firebase.RTDB.setBool(&fbdoUpload, presenceEnabledPath.c_str(), presenceDetectionEnabled);
  }
  
  String luxThresholdPath = basePath + "/lux_threshold";
  if (Firebase.RTDB.getFloat(&fbdoUpload, luxThresholdPath.c_str())) {
    luxThreshold = fbdoUpload.floatData();
    Serial.printf("      Lux Threshold: %.2f\n", luxThreshold);
  } else {
    luxThreshold = 1.0;
    Firebase.RTDB.setFloat(&fbdoUpload, luxThresholdPath.c_str(), luxThreshold);
  }
  
  String timerOnPath = basePath + "/timer_on";
  if (Firebase.RTDB.getString(&fbdoUpload, timerOnPath.c_str())) {
    String timeStr = fbdoUpload.stringData();
    if (timeStr.length() == 5) {
      strncpy(timerOnTime, timeStr.c_str(), sizeof(timerOnTime));
      Serial.printf("      Timer ON: %s\n", timerOnTime);
    }
  } else {
    strcpy(timerOnTime, "09:00");
    Firebase.RTDB.setString(&fbdoUpload, timerOnPath.c_str(), timerOnTime);
  }
  
  String timerOffPath = basePath + "/timer_off";
  if (Firebase.RTDB.getString(&fbdoUpload, timerOffPath.c_str())) {
    String timeStr = fbdoUpload.stringData();
    if (timeStr.length() == 5) {
      strncpy(timerOffTime, timeStr.c_str(), sizeof(timerOffTime));
      Serial.printf("      Timer OFF: %s\n", timerOffTime);
    }
  } else {
    strcpy(timerOffTime, "17:00");
    Firebase.RTDB.setString(&fbdoUpload, timerOffPath.c_str(), timerOffTime);
  }
  
  String timerEnabledPath = basePath + "/timer_enabled";
  if (Firebase.RTDB.getBool(&fbdoUpload, timerEnabledPath.c_str())) {
    timerEnabled = fbdoUpload.boolData();
    Serial.printf("      Timer Enabled: %s\n", timerEnabled ? "true" : "false");
  } else {
    timerEnabled = true;
    Firebase.RTDB.setBool(&fbdoUpload, timerEnabledPath.c_str(), timerEnabled);
  }
  
  String resetPath = basePath + "/reset";
  Firebase.RTDB.setBool(&fbdoUpload, resetPath.c_str(), false);
  
  String micCalibPath = basePath + "/mic_calibration";
  Firebase.RTDB.setBool(&fbdoUpload, micCalibPath.c_str(), false);
  
  String mqttEnabledPath = basePath + "/mqtt/enabled";
  if (!Firebase.RTDB.getBool(&fbdoUpload, mqttEnabledPath.c_str())) {
    Serial.println("      Creating default MQTT configuration...");
    
    String mqttBasePath = basePath + "/mqtt";
    delay(500);
    
    Firebase.RTDB.setString(&fbdoUpload, (mqttBasePath + "/broker_address").c_str(), "192.168.1.100");
    delay(200);
    Firebase.RTDB.setInt(&fbdoUpload, (mqttBasePath + "/broker_port").c_str(), 1883);
    delay(200);
    Firebase.RTDB.setString(&fbdoUpload, (mqttBasePath + "/username").c_str(), "mqtt_user");
    delay(200);
    Firebase.RTDB.setString(&fbdoUpload, (mqttBasePath + "/password").c_str(), "mqtt_password");
    delay(200);
    Firebase.RTDB.setBool(&fbdoUpload, (mqttBasePath + "/enabled").c_str(), false);
    delay(200);
    Firebase.RTDB.setString(&fbdoUpload, (mqttBasePath + "/device_name").c_str(), "Lumina");
    
    Serial.println("      ✅ MQTT config created");
    updateMQTTConfigFromFirebase();
  } else {
    updateMQTTConfigFromFirebase();
  }
  
  String versionPath = basePath + "/version";
  if (Firebase.RTDB.setString(&fbdoUpload, versionPath.c_str(), currentFirmwareVersion)) {
    Serial.printf("      ✅ Version published: %s\n", currentFirmwareVersion);
  }
  
  Serial.println("   ✅ Initial data loaded");
}

void streamCallback(FirebaseStream data) {
  //Serial.printf("📡 Firebase Stream - Path: %s, Type: %s, Value: %s\n",
                //data.dataPath().c_str(),
               // data.dataType().c_str(),
                //data.stringData().c_str());

  String dataPath = data.dataPath().c_str();

  if (dataPath == "/effect") {
    currentEffect = data.intData();
    Serial.printf("   Effect changed to: %d\n", currentEffect);
  }
  else if (dataPath == "/speed") {
    effectSpeed = data.intData();
    Serial.printf("   Speed changed to: %d\n", effectSpeed);
  }
  else if (dataPath == "/color") {
    String colorStr = data.stringData();
    if (colorStr.length() == 6) {
      effectColor = strtoul(colorStr.c_str(), NULL, 16);
      Serial.printf("   Color changed to: %s\n", colorStr.c_str());
    }
  }
  else if (dataPath == "/lux_threshold") {
    luxThreshold = data.floatData();
    Serial.printf("   Lux threshold changed to: %.2f\n", luxThreshold);
  }
  // CRITICAL FIX 1: Thread-safe access to stripEnabled
  else if (dataPath == "/enabled") {
    bool newState = data.boolData();
    
    // Use critical section for thread-safe access
    portENTER_CRITICAL(&stripMux);
    stripEnabled = newState;
    portEXIT_CRITICAL(&stripMux);
    
    turnedOffByDarkness = false;
    
    if (newState == false) {
      portENTER_CRITICAL(&stripMux);
      manuallyTurnedOff = true;
      portEXIT_CRITICAL(&stripMux);
      Serial.println("   ⚠️  MANUAL OFF: LEDs locked off until manually re-enabled");
    } else {
      portENTER_CRITICAL(&stripMux);
      manuallyTurnedOff = false;
      portEXIT_CRITICAL(&stripMux);
      Serial.println("   ✅ Manual ON: Automation can now control LEDs");
    }
    
    Serial.printf("   Strip %s\n", stripEnabled ? "enabled" : "disabled");
    
    if (!stripEnabled) {
      strip.clear();
      strip.show();
      Serial.println("   LEDs turned off");
    }
  }
  else if (dataPath == "/timer_enabled") {
    timerEnabled = data.boolData();
    Serial.printf("   Timer %s\n", timerEnabled ? "enabled" : "disabled");
  }
  else if (dataPath == "/auto_darkness_control") {
    autoDarknessControl = data.boolData();
    Serial.printf("   Auto darkness control %s\n", autoDarknessControl ? "enabled" : "disabled");
  }
  else if (dataPath == "/presence_detection_enabled") {
    presenceDetectionEnabled = data.boolData();
    Serial.printf("   Presence detection %s\n", presenceDetectionEnabled ? "enabled" : "disabled");
  }
  else if (dataPath == "/timer_on") {
    String timeStr = data.stringData();
    if (timeStr.length() == 5) {
      strncpy(timerOnTime, timeStr.c_str(), sizeof(timerOnTime));
      Serial.printf("   Timer ON time changed to: %s\n", timerOnTime);
    }
  }
  else if (dataPath == "/timer_off") {
    String timeStr = data.stringData();
    if (timeStr.length() == 5) {
      strncpy(timerOffTime, timeStr.c_str(), sizeof(timerOffTime));
      Serial.printf("   Timer OFF time changed to: %s\n", timerOffTime);
    }
  }
  else if (dataPath == "/mqtt/broker_address" || 
           dataPath == "/mqtt/broker_port" ||
           dataPath == "/mqtt/username" ||
           dataPath == "/mqtt/password" ||
           dataPath == "/mqtt/enabled" ||
           dataPath == "/mqtt/device_name") {
    updateMQTTConfigFromFirebase();
    Serial.println("   MQTT configuration updated from Firebase");
  }
  else if (dataPath == "/reset" && data.boolData() == true) {
    Serial.println("🔄 Reset command received - deleting config and restarting...");
    
    if (SPIFFS.exists("/config.json")) {
      SPIFFS.remove("/config.json");
    }
    
    ESP.restart();
    return;
  }
  else if (dataPath == "/mic_calibration" && data.boolData() == true) {
    Serial.println("\n🎤 MICROPHONE CALIBRATION TRIGGERED VIA FIREBASE");
    triggerMicCalibration = true;
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
  String testPath = basePath + "/effect";
  
  if (Firebase.RTDB.getInt(&fbdoUpload, testPath.c_str())) {
    Serial.println("Firebase data already exists, skipping default data creation");
    defaultDataCreated = true;
    return;
  }
  
  Serial.println("Creating default Firebase structure for device: " + deviceID);
  
  bool allSuccess = true;
  
  String effectPath = basePath + "/effect";
  if (Firebase.RTDB.setInt(&fbdoUpload, effectPath.c_str(), 0)) {
    Serial.println("Effect set to default: 0");
  } else {
    Serial.printf("Failed to set effect: %s\n", fbdoUpload.errorReason().c_str());
    allSuccess = false;
  }
  
  String speedPath = basePath + "/speed";
  if (Firebase.RTDB.setInt(&fbdoUpload, speedPath.c_str(), 50)) {
    Serial.println("Speed set to default: 50");
  } else {
    Serial.printf("Failed to set speed: %s\n", fbdoUpload.errorReason().c_str());
    allSuccess = false;
  }
  
  String colorPath = basePath + "/color";
  if (Firebase.RTDB.setString(&fbdoUpload, colorPath.c_str(), "FF0000")) {
    Serial.println("Color set to default: FF0000");
  } else {
    Serial.printf("Failed to set color: %s\n", fbdoUpload.errorReason().c_str());
    allSuccess = false;
  }
  
  String enabledPath = basePath + "/enabled";
  if (Firebase.RTDB.setBool(&fbdoUpload, enabledPath.c_str(), true)) {
    Serial.println("Enabled set to default: true");
  } else {
    Serial.printf("Failed to set enabled: %s\n", fbdoUpload.errorReason().c_str());
    allSuccess = false;
  }
  
  String autoDarknessPath = basePath + "/auto_darkness_control";
  if (Firebase.RTDB.setBool(&fbdoUpload, autoDarknessPath.c_str(), true)) {
    Serial.println("Auto darkness control set to default: true");
  } else {
    Serial.printf("Failed to set auto darkness control: %s\n", fbdoUpload.errorReason().c_str());
    allSuccess = false;
  }
  
  String presenceEnabledPath = basePath + "/presence_detection_enabled";
  if (Firebase.RTDB.setBool(&fbdoUpload, presenceEnabledPath.c_str(), true)) {
    Serial.println("Presence detection enabled set to default: true");
  } else {
    Serial.printf("Failed to set presence detection enabled: %s\n", fbdoUpload.errorReason().c_str());
    allSuccess = false;
  }
  
  String luxThresholdPath = basePath + "/lux_threshold";
  if (Firebase.RTDB.setFloat(&fbdoUpload, luxThresholdPath.c_str(), 1.0)) {
    Serial.println("Lux threshold set to default: 1.0");
  } else {
    Serial.printf("Failed to set lux threshold: %s\n", fbdoUpload.errorReason().c_str());
    allSuccess = false;
  }
  
  String timerOnPath = basePath + "/timer_on";
  if (Firebase.RTDB.setString(&fbdoUpload, timerOnPath.c_str(), "09:00")) {
    Serial.println("Timer ON set to default: 09:00");
  } else {
    Serial.printf("Failed to set timer ON: %s\n", fbdoUpload.errorReason().c_str());
    allSuccess = false;
  }
  
  String timerOffPath = basePath + "/timer_off";
  if (Firebase.RTDB.setString(&fbdoUpload, timerOffPath.c_str(), "17:00")) {
    Serial.println("Timer OFF set to default: 17:00");
  } else {
    Serial.printf("Failed to set timer OFF: %s\n", fbdoUpload.errorReason().c_str());
    allSuccess = false;
  }
  
  String timerEnabledPath = basePath + "/timer_enabled";
  if (Firebase.RTDB.setBool(&fbdoUpload, timerEnabledPath.c_str(), true)) {
    Serial.println("Timer enabled set to default: true");
  } else {
    Serial.printf("Failed to set timer enabled: %s\n", fbdoUpload.errorReason().c_str());
    allSuccess = false;
  }
  
  String resetPath = basePath + "/reset";
  if (Firebase.RTDB.setBool(&fbdoUpload, resetPath.c_str(), false)) {
    Serial.println("Reset flag set to default: false");
  } else {
    Serial.printf("Failed to set reset flag: %s\n", fbdoUpload.errorReason().c_str());
    allSuccess = false;
  }
  
  String micCalibPath = basePath + "/mic_calibration";
  if (Firebase.RTDB.setBool(&fbdoUpload, micCalibPath.c_str(), false)) {
    Serial.println("Mic calibration flag set to default: false");
  } else {
    Serial.printf("Failed to set mic calibration flag: %s\n", fbdoUpload.errorReason().c_str());
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
      strip.clear();
      strip.show();
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

  const unsigned long timeoutDuration = 10 * 1000;
  unsigned long lastDataTime = millis();

  while (written < contentLength) {
    esp_task_wdt_reset();
    
    if (client->available()) {
      uint8_t buffer[128];
      size_t len = client->read(buffer, sizeof(buffer));
      if (len > 0) {
        Update.write(buffer, len);
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
      Update.abort();
      return false;
    }

    yield();
  }
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