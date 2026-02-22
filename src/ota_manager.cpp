#include "ota_manager.h"
#include "globals.h"

// Non-blocking handle for the main loop
void handleOTAUpdate() {
  static unsigned long lastCheck = 0;
  // Check every 10 minutes (600,000 ms)
  if (millis() - lastCheck > 600000) {
    lastCheck = millis();
    if (WiFi.status() == WL_CONNECTED) {
       checkForGitHubUpdate();
    }
  }
}

void checkForGitHubUpdate() {
  Serial.println("🔍 [OTA] Checking for GitHub firmware update...");
  
  if (currentFirmwareVersion == NULL || strlen(currentFirmwareVersion) == 0) {
    Serial.println("   ❌ Current firmware version is invalid");
    return;
  }

  String latestVersion = fetchLatestVersion();
  if (latestVersion == "" || latestVersion.length() == 0) {
    return;
  }

  Serial.printf("   Current: %s | Latest: %s\n", currentFirmwareVersion, latestVersion.c_str());

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
  http.setTimeout(3000); 
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
  Serial.println("📥 [OTA] Downloading firmware...");
  
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(10000); 
  http.begin(GITHUB_FIRMWARE_URL);

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    Serial.printf("   Firmware size: %d bytes (%.2f KB)\n", contentLength, contentLength / 1024.0);

    if (contentLength > 0) {
      FastLED.clear();
      FastLED.show();
      
      WiFiClient* stream = http.getStreamPtr();
      if (startGitHubOTAUpdate(stream, contentLength)) {
        Serial.println("   ✅ OTA update successful, restarting...");
        delay(2000);
        ESP.restart();
      } else {
        Serial.println("   ❌ OTA update failed");
      }
    } else {
      Serial.println("   ❌ Invalid firmware size");
    }
  } else {
    Serial.printf("   ❌ Failed to download firmware. HTTP code: %d\n", httpCode);
  }
  http.end();
}

bool startGitHubOTAUpdate(WiFiClient* client, int contentLength) {
  if (!Update.begin(contentLength)) {
    Serial.printf("   ❌ Update begin failed: %s\n", Update.errorString());
    return false;
  }

  size_t written = 0;
  int progress = 0;
  int lastProgress = 0;

  const size_t bufferSize = 16384; 
  uint8_t *buffer = (uint8_t *)ps_malloc(bufferSize);
  if (!buffer) {
    Serial.println("   ⚠️  Failed to PSRAM buffer, using stack fallback");
  }

  const unsigned long timeoutDuration = 20000;
  unsigned long lastDataTime = millis();

  while (written < contentLength) {
    esp_task_wdt_reset(); 
    
    if (client->available()) {
      size_t toRead = client->available();
      if (buffer) {
        if (toRead > bufferSize) toRead = bufferSize;
      } else {
        if (toRead > 256) toRead = 256;
      }
      
      size_t len = client->read(buffer ? buffer : (uint8_t*)alloca(256), toRead);
      if (len > 0) {
        Update.write(buffer ? buffer : (uint8_t*)alloca(256), len);
        written += len;
        lastDataTime = millis();

        progress = (written * 100) / contentLength;
        if (progress != lastProgress) {
          Serial.printf("      Progress: %d%%\n", progress);
          lastProgress = progress;
        }
      }
    }
    
    if (millis() - lastDataTime > timeoutDuration) {
      Serial.println("\n   ❌ Timeout: No data received.");
      if (buffer) free(buffer);
      Update.abort();
      return false;
    }
  }
  
  if (buffer) free(buffer);
  
  if (written != contentLength) {
    Serial.printf("\n   ❌ Write incomplete. %d/%d\n", written, contentLength);
    Update.abort();
    return false;
  }

  if (!Update.end()) {
    Serial.printf("\n   ❌ Update end failed: %s\n", Update.errorString());
    return false;
  }

  return true;
}
