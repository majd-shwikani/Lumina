#include "globals.h"
#include <WiFiClientSecure.h>

// ============================================================================
// GITHUB OTA FUNCTIONS
// ============================================================================

String fetchLatestVersion() {
  if (WiFi.status() != WL_CONNECTED) return "";
  
  WiFiClientSecure client;
  client.setInsecure(); // GitHub uses HTTPS
  
  HTTPClient http;
  http.begin(client, GITHUB_VERSION_URL);
  
  int httpCode = http.GET();
  String payload = "";
  
  if (httpCode == HTTP_CODE_OK) {
    payload = http.getString();
    payload.trim();
  }
  
  http.end();
  return payload;
}

void downloadAndApplyFirmware() {
  Serial.println("📥 Starting firmware download...");
  
  WiFiClientSecure client;
  client.setInsecure();
  
  HTTPClient http;
  http.begin(client, GITHUB_FIRMWARE_URL);
  
  // Follow redirects (GitHub releases use redirects to AWS/etc.)
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("❌ Failed to download firmware, HTTP code: %d\n", httpCode);
    http.end();
    return;
  }
  
  int contentLength = http.getSize();
  if (contentLength <= 0) {
    Serial.println("❌ Invalid content length");
    http.end();
    return;
  }
  
  bool canBegin = Update.begin(contentLength);
  if (!canBegin) {
    Serial.println("❌ Not enough space for OTA");
    http.end();
    return;
  }
  
  Serial.printf("📦 Updating: %d bytes\n", contentLength);
  
  WiFiClient* stream = http.getStreamPtr();
  size_t written = Update.writeStream(*stream);
  
  if (written != (size_t)contentLength) {
    Serial.printf("❌ Write failed: %d/%d\n", written, contentLength);
  } else if (Update.end()) {
    if (Update.isFinished()) {
      Serial.println("✅ Update successful! Restarting...");
      delay(1000);
      ESP.restart();
    } else {
      Serial.println("❌ Update not finished");
    }
  } else {
    Serial.printf("❌ Update error: %s\n", Update.errorString());
  }
  
  http.end();
}

void checkForGitHubUpdate() {
  Serial.println("🔍 Checking for updates on GitHub...");
  String latestVersion = fetchLatestVersion();
  
  if (latestVersion == "") {
    Serial.println("⚠️  Could not fetch version info");
    return;
  }
  
  Serial.printf("   Current: %s | Latest: %s\n", currentFirmwareVersion, latestVersion.c_str());
  
  if (latestVersion != currentFirmwareVersion) {
    Serial.println("🆕 New version available! Starting update...");
    downloadAndApplyFirmware();
  } else {
    Serial.println("✅ Firmware is up to date");
  }
}

// ============================================================================
// LED UPDATE FUNCTION
// ============================================================================

void updateLEDs() {
  static bool lastStripEnabled = true;
  
  bool currentEnabled;
  portENTER_CRITICAL(&stripMux);
  currentEnabled = stripEnabled;
  portEXIT_CRITICAL(&stripMux);
  
  if (!currentEnabled && lastStripEnabled) {
    FastLED.clear();
    FastLED.show();
    lastStripEnabled = false;
    return;
  }
  
  if (!currentEnabled) return;
  if (currentEnabled && !lastStripEnabled) lastStripEnabled = true;
  
  switch(currentEffect) {
    case 0: effectRainbow(); break;
    case 1: effectMeteorShower(); break;
    case 2: effectDigitalRain(); break;
    case 3: effectPulsingSpheres(); break;
    case 4: effectBinaryClock(); break;
    case 5: effectVortex(); break;
    case 6: effectDNAHelix(); break;
    case 8: effectLavaLamp(); break;
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
    case 31: effectEnergyOrbits(); break;
    default: effectRainbow(); break;
  }
  FastLED.show();
}

// ============================================================================
// FREERTOS TASKS
// ============================================================================

void ledTask(void *parameter) {
  Serial.println("💡 LED Task started on Core " + String(xPortGetCoreID()));
  for(;;) {
    esp_task_wdt_reset();
    updateLEDs();
    vTaskDelay(effectSpeed / portTICK_PERIOD_MS);
  }
}

void otaUpdateTask(void *parameter) {
  Serial.println("🔄 OTA Task started on Core " + String(xPortGetCoreID()));
  
  const TickType_t xDelay = UPDATE_CHECK_INTERVAL / portTICK_PERIOD_MS;
  vTaskDelay(10000 / portTICK_PERIOD_MS); // Wait 10s after boot
  
  for(;;) {
    esp_task_wdt_reset();
    if (WiFi.status() == WL_CONNECTED) {
      ArduinoOTA.handle();
      checkForGitHubUpdate();
    }
    vTaskDelay(xDelay);
  }
}
