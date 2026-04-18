#include "globals.h"

// ============================================================================
// ESP-NOW CALLBACKS
// ============================================================================

void OnDataSent(const uint8_t *mac, esp_now_send_status_t status) {
  if (status != ESP_NOW_SEND_SUCCESS) {
    Serial.println("   ⚠️  ESP-NOW Send Failed");
  }
}

void OnDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  if (len < sizeof(LuminaMessage)) {
    Serial.printf("   ⚠️  Received small packet: %d bytes\n", len);
    return;
  }
  
  LuminaMessage *msg = (LuminaMessage *)data;
  
  // 1. Discovery Reply
  if (strcmp(msg->msgType, "LUMINA_OFFER") == 0) {
    lastGatewayContact = millis(); // Reset timer on handshake
    if (!gatewayFound) {
      memcpy(gatewayMAC, mac, 6);
      gatewayFound = true;
      
      esp_now_peer_info_t peerInfo = {};
      memcpy(peerInfo.peer_addr, gatewayMAC, 6);
      peerInfo.channel = currentWifiChannel;
      peerInfo.encrypt = false;
      
      if (!esp_now_is_peer_exist(gatewayMAC)) {
        esp_now_add_peer(&peerInfo);
      }
      
      // Cache the successful channel
      File f = SPIFFS.open("/last_ch", "w");
      if (f) {
        f.print(currentWifiChannel);
        f.close();
      }
      
      Serial.printf("✨ Gateway Offer Received! Locked to channel %d from %02X:%02X:%02X:%02X:%02X:%02X\n",
                    currentWifiChannel, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
  } 
  // 2. Control Command
  else if (strcmp(msg->msgType, "LUMINA_CMD") == 0) {
    // Identity Filter
    uint8_t myMac[6];
    esp_read_mac(myMac, ESP_MAC_WIFI_STA);
    
    uint8_t broadcastMac[6] = {0, 0, 0, 0, 0, 0};
    bool isBroadcast = (memcmp(msg->targetMac, broadcastMac, 6) == 0);
    bool isForMe = (memcmp(msg->targetMac, myMac, 6) == 0);
    
    if (isBroadcast || isForMe) {
      if (!gatewayFound || memcmp(mac, gatewayMAC, 6) == 0) {
        if (!gatewayFound) {
          Serial.println("   🔓 Auto-locking to command source...");
          gatewayFound = true;
          memcpy(gatewayMAC, mac, 6);
          
          // Also cache the channel if we auto-lock
          File f = SPIFFS.open("/last_ch", "w");
          if (f) {
            f.print(currentWifiChannel);
            f.close();
          }
        }
        
        lastGatewayContact = millis();
        // ... (rest of command handling)
        
        // Update mode based on message type
        if (isBroadcast) {
          if (currentMode != MODE_MIRROR) {
            currentMode = MODE_MIRROR;
            Serial.println("🔄 Switched to MIRROR Mode (Global Broadcast)");
          }
        } else {
          if (currentMode != MODE_STANDALONE) {
            currentMode = MODE_STANDALONE;
            Serial.println("🎯 Switched to STANDALONE Mode (Individual Command)");
          }
        }
        
        portENTER_CRITICAL(&stripMux);
        currentEffect = msg->effect;
        effectSpeed = msg->speed;
        effectColor = msg->color;
        globalBrightness = msg->brightness;
        stripEnabled = msg->enabled;
        portEXIT_CRITICAL(&stripMux);
        
        Serial.printf("🎮 [%s] Received Command: Effect=%d, Speed=%d, Color=%06X, Brightness=%d, Enabled=%s\n", 
                      (currentMode == MODE_MIRROR ? "Mirror" : "Standalone"),
                      msg->effect, msg->speed, msg->color, msg->brightness, msg->enabled ? "ON" : "OFF");
      }
    } else {
      // Ignore messages intended for other devices
    }
  } else {
    Serial.printf("   ❓ Unknown Message Type: %s\n", msg->msgType);
  }
}

// ============================================================================
// DISCOVERY TASK (THE "HUNT")
// ============================================================================

void discoveryTask(void *parameter) {
  Serial.println("🔍 Discovery Task started on Core " + String(xPortGetCoreID()));
  
  LuminaMessage discoveryMsg;
  strcpy(discoveryMsg.msgType, "LUMINA_DISCOVERY");
  esp_read_mac(discoveryMsg.targetMac, ESP_MAC_WIFI_STA); 
  
  uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  // Try cached channel first if available
  if (SPIFFS.exists("/last_ch")) {
    File f = SPIFFS.open("/last_ch", "r");
    if (f) {
      int cachedCh = f.readString().toInt();
      f.close();
      if (cachedCh >= 1 && cachedCh <= 13) {
        Serial.printf("📡 [Receiver] Trying cached channel %d first...\n", cachedCh);
        currentWifiChannel = cachedCh;
        esp_wifi_set_channel(cachedCh, WIFI_SECOND_CHAN_NONE);
        esp_now_send(broadcastAddress, (uint8_t *)&discoveryMsg, sizeof(discoveryMsg));
        vTaskDelay(200 / portTICK_PERIOD_MS); // Give it a bit more time for the first hit
      }
    }
  }

  for(;;) {
    esp_task_wdt_reset();

    if (!gatewayFound) {
      Serial.println("📡 Hunting for Gateway (scanning channels 1-13)...");
      for (int ch = 1; ch <= 13 && !gatewayFound; ch++) {
        currentWifiChannel = ch;
        esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
        
        Serial.printf("📡 [Receiver] Sending Discovery on Channel %d...\n", ch);
        esp_now_send(broadcastAddress, (uint8_t *)&discoveryMsg, sizeof(discoveryMsg));
        
        vTaskDelay(80 / portTICK_PERIOD_MS); // Aggressive scan
      }
    } else {
      // Self-Healing
      if (millis() - lastGatewayContact > GATEWAY_TIMEOUT) {
        Serial.printf("💔 Lost contact with Gateway (MAC: %02X:%02X...) - Restarting Hunt\n", gatewayMAC[0], gatewayMAC[1]);
        gatewayFound = false;
        if (esp_now_is_peer_exist(gatewayMAC)) {
          esp_now_del_peer(gatewayMAC);
        }
      }
      
      vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
  }
}

// ============================================================================
// LED UPDATE FUNCTION
// ============================================================================

void updateLEDs() {
  static bool lastStripEnabled = true;
  bool currentEnabled;
  uint8_t currentBrightness;

  portENTER_CRITICAL(&stripMux);
  currentEnabled = stripEnabled;
  currentBrightness = globalBrightness;
  portEXIT_CRITICAL(&stripMux);
  
  if (!currentEnabled && lastStripEnabled) {
    FastLED.clear();
    FastLED.show();
    lastStripEnabled = false;
    return;
  }
  
  if (!currentEnabled) return;
  if (currentEnabled && !lastStripEnabled) lastStripEnabled = true;

  FastLED.setBrightness(currentBrightness);
  
   switch (currentEffect) {
    // ---- Original Animation Effects (0-21) ----------------------------------
    case  0: effectRainbow();            break;
    case  1: effectMeteorShower();       break;
    case  2: effectDigitalRain();        break;
    case  3: effectPulsingSpheres();     break;
    case  4: effectBinaryClock();        break;
    case  5: effectVortex();             break;
    case  6: effectDNAHelix();           break;
    case  7: effectAudioVisualizer();    break;
    case  8: effectLavaLamp();           break;
    case  9: effectRadarSweep();         break;
    case 10: effectQuantumParticles();   break;
    case 11: effectNeuralNetwork();      break;
    case 12: effectGalaxySpin();         break;
    case 13: effectCrystalGrowth();      break;
    case 14: effectLightningStorm();     break;
    case 15: effectOceanDepth();         break;
    case 16: effectNorthernLights();     break;
    case 17: effectTimeTunnel();         break;
    case 18: effectCyberCity();          break;
    case 19: effectSolarFlare();         break;
    case 20: effectFireSimulation();     break;
    case 21: effectSolidColor();         break;

    // ---- Revolutionary Effects (33-42) --------------------------------------
    case 33: effectPlasmaWaves();        break;
    case 34: effectConfettiPalettes();   break;
    case 35: effectSinelonDual();        break;
    case 36: effectBPM();                break;
    case 37: effectJuggle();             break;
    case 38: effectGlitterRainbow();     break;
    case 39: effectPacific();            break;
    case 40: effectTwinkleFox();         break;
    case 41: effectColorWaves();         break;
    case 42: effectPerlinMove();         break;

    // ---- Organic Lamp Effects (43-52) ---------------------------------------
    case 43: effectPlasmaLamp();         break;
    case 44: effectBiolume();            break;
    case 45: effectDeepSeaVolcano();     break;
    case 46: effectMagicalAurora();      break;
    case 47: effectSolarWinds();         break;
    case 48: effectEtherealMist();       break;
    case 49: effectBioPulse();           break;
    case 50: effectRadioactiveGlow();    break;
    case 51: effectSupernova();          break;
    case 52: effectEnchantedStream();    break;

    // ---- Fire Simulation Effects (53-62) ------------------------------------
    case 53: effectBlueGasFlame();       break;
    case 54: effectWildfire();           break;
    case 55: effectCandleFlame();        break;
    case 56: effectCampfire();           break;
    case 57: effectPlasmaFire();         break;
    case 58: effectInferno();            break;
    case 59: effectSmolderingEmbers();   break;
    case 60: effectLavaFlow();           break;
    case 61: effectEmberStorm();         break;
    case 62: effectHearthFire();         break;

    default: effectRainbow();            break;
  }
  FastLED.show();
}

void ledTask(void *parameter) {
  Serial.println("💡 LED Task started on Core " + String(xPortGetCoreID()));
  for(;;) {
    esp_task_wdt_reset();
    updateLEDs();
    vTaskDelay(effectSpeed / portTICK_PERIOD_MS);
  }
}

// ============================================================================
// BOOT OTA UPDATE TASK (RUNS ONCE AT BOOT)
// ============================================================================

void bootOTAUpdateTask(void *parameter) {
  Serial.println("🔄 Boot OTA Update Task started on Core " + String(xPortGetCoreID()));
  
  // 1. Connect to WiFi
  connectToWiFi();
  
  // 2. Check for update if connected
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("🔍 Checking for boot firmware update...");
    checkForGitHubUpdate();
  } else {
    Serial.println("⚠️  WiFi not connected for boot OTA check");
  }
  
  // 3. Disconnect WiFi to return to "Receiver" state (ESP-NOW works best when STA isn't actively hunting)
  // Actually, keeping STA connected is fine if it matched, but for clean ESP-NOW, we can disconnect if desired.
  // The user asked to connect, check, then continue.
  
  // 4. Start the rest of the systems
  startSystems();
  
  // 5. Self-terminate to free memory
  Serial.println("🗑️  Boot OTA task completed. Terminating to free memory...");
  otaTaskHandle = NULL;
  vTaskDelete(NULL);
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
