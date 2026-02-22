#include "globals.h"
#include "ota_manager.h" // For OTA references if needed, though mostly moved

// ============================================================================
// VARIABLE DEFINITIONS
// ============================================================================
Receiver receivers[10];
int receiverCount = 0;

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
  
  if (!currentEnabled) {
    return;
  }
  
  if (isListening) {
    // Pulsing blue animation for listening
    uint8_t pulse = beatsin8(30, 50, 255);
    for(int i = 0; i < ledCount; i++) {
      leds[i] = CRGB::Blue;
      leds[i].nscale8(pulse);
    }
    FastLED.show();
    return;
  }
  
  if (currentEnabled && !lastStripEnabled) {
    lastStripEnabled = true;
  }
  
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

    // ---- Sound-Reactive Effects (22-32) -------------------------------------
    case 22: effectFrequencySpectrum();  break;
    case 23: effectReactiveWaveform();   break;
    case 24: effectBeatPulse();          break;
    case 25: effectFrequencyBloom();     break;
    case 26: effectAudioReactiveFire();  break;
    case 27: effectMusicalRainbow();     break;
    case 28: effectReactiveStrobe();     break;
    case 29: effectGuitarVisualizer();   break;
    case 30: effectCascadingFrequency(); break;
    case 31: effectEnergyOrbits();       break;
    case 32: effectAudioRipples();       break;

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

// ============================================================================
// TIMER HELPER FUNCTIONS
// ============================================================================

bool checkTimeMatch(const char* scheduledTime) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return false;
  }
  
  char currentTime[6];
  strftime(currentTime, sizeof(currentTime), "%H:%M", &timeinfo);
  
  return strcmp(currentTime, scheduledTime) == 0;
}

void updateTimerState(bool state) {
  esp_task_wdt_reset();
  
  String enabledPath = basePath + "/local/enabled";
  if (Firebase.RTDB.setBool(&fbdoUpload, enabledPath.c_str(), state)) {
    portENTER_CRITICAL(&stripMux);
    stripEnabled = state;
    portEXIT_CRITICAL(&stripMux);
    
    if (state) {
      portENTER_CRITICAL(&stripMux);
      manuallyTurnedOff = false;
      portEXIT_CRITICAL(&stripMux);
      Serial.println("Timer turned ON LEDs - manual lock cleared");
    }
    
    Serial.printf("Timer updated enabled state to: %s\n", state ? "true" : "false");
    syncAllMirrors();
  } else {
    Serial.printf("Failed to update enabled state: %s\n", fbdoUpload.errorReason().c_str());
  }
  esp_task_wdt_reset();
}

void handleTimers() {
  static bool lastOnTriggered = false;
  static bool lastOffTriggered = false;
  
  if (firebaseConnected && timerEnabled) {
    if (checkTimeMatch(timerOnTime)) {
      if (!lastOnTriggered) {
        Serial.println("⏰ Timer ON triggered");
        updateTimerState(true);
        lastOnTriggered = true;
      }
    } else {
      lastOnTriggered = false;
    }
    
    if (checkTimeMatch(timerOffTime)) {
      if (!lastOffTriggered) {
        Serial.println("⏰ Timer OFF triggered");
        updateTimerState(false);
        lastOffTriggered = true;
      }
    } else {
      lastOffTriggered = false;
    }
  }
}

// ============================================================================
// UPTIME FORMATTER
// ============================================================================

String formatUptime(unsigned long milliseconds) {
  unsigned long seconds = milliseconds / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  unsigned long days = hours / 24;
  
  if (days > 0) {
    return String(days) + "d " + String(hours % 24) + "h";
  } else if (hours > 0) {
    return String(hours) + "h " + String(minutes % 60) + "m";
  } else if (minutes > 0) {
    return String(minutes) + "m " + String(seconds % 60) + "s";
  } else {
    return String(seconds) + "s";
  }
}

// ============================================================================
// FREERTOS TASKS
// ============================================================================

void firebaseTask(void *parameter) {
  Serial.println("🔥 Firebase Task started on Core " + String(xPortGetCoreID()));
  
  // Timer for stats upload
  unsigned long lastStatsUpload = 0;
  const unsigned long STATS_UPLOAD_INTERVAL = 2000;
  
  for(;;) {
    esp_task_wdt_reset();
    
    if (WiFi.status() == WL_CONNECTED && Firebase.ready()) {
      firebaseConnected = true;
      ArduinoOTA.handle();
      
      // 1. Process Receiver Registry Changes
      if (registryChanged) {
        bool allSynced = true;
        for (int i = 0; i < receiverCount; i++) {
          if (receivers[i].needsFirebaseSync) {
            String rPath = basePath + "/receivers/" + receivers[i].macStr;
            FirebaseJson rJson;
            rJson.set("isMirror", receivers[i].isMirror);
            rJson.set("effect", receivers[i].effect);
            rJson.set("speed", receivers[i].speed);
            rJson.set("color", "FF0000"); // Standard default
            rJson.set("enabled", receivers[i].enabled);
            
            Serial.printf("☁️ [FirebaseTask] Syncing receiver %s to cloud...\n", receivers[i].macStr.c_str());
            if (Firebase.RTDB.setJSON(&fbdoUpload, rPath.c_str(), &rJson)) {
              receivers[i].needsFirebaseSync = false;
            } else {
              allSynced = false;
              Serial.printf("⚠️ [FirebaseTask] Failed to sync receiver: %s\n", fbdoUpload.errorReason().c_str());
            }
          }
        }
        
        // 2. Update Active Nodes List if needed
        if (allSynced) {
          updateActiveNodesInFirebase();
          registryChanged = false;
        }
      }
      
      // 3. Upload Sensor/System Stats (Migrated from SensorDataTask)
      if (millis() - lastStatsUpload > STATS_UPLOAD_INTERVAL) {
        lastStatsUpload = millis();
        
        uint32_t freeHeap = ESP.getFreeHeap();
        uint32_t totalHeap = ESP.getHeapSize();
        
        FirebaseJson json;
        FirebaseJson sensors;
        FirebaseJson stats;
        
        // Add sensor data (Updated by handleSensorsAndAutomation in main loop)
        sensors.set("lux", currentLux);
        sensors.set("presence", lastPresence);
        
        // Add system stats
        stats.set("uptime", formatUptime(millis()));
        stats.set("status", systemHealthy ? "ALL SYSTEMS NOMINAL" : "SYSTEM WARNINGS");
        stats.set("free_heap_kb", freeHeap / 1024);
        stats.set("wifi_rssi", WiFi.RSSI());
        stats.set("loop_counter", loopCounter);
        stats.set("firmware_version", currentFirmwareVersion);
        
        json.set("sensors", sensors);
        json.set("stats", stats);
        
        String combinedPath = basePath + "/local/sensor_stats";
        // Fire and forget (best effort), don't block too long
        Firebase.RTDB.setJSON(&fbdoUpload, combinedPath.c_str(), &json);
      }
      
    } else {
      firebaseConnected = false;
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("⚠️  WiFi disconnected in Firebase task, attempting reconnect...");
        connectToWiFi();
      }
    }
    
    vTaskDelay(100 / portTICK_PERIOD_MS); // Run at 10Hz
  }
}

void ledTask(void *parameter) {
  Serial.println("💡 LED Task started on Core " + String(xPortGetCoreID()));
  
  for(;;) {
    esp_task_wdt_reset();
    updateLEDs();
    vTaskDelay(effectSpeed / portTICK_PERIOD_MS);
  }
}

void statusLedTask(void *parameter) {
  Serial.println("🚨 Status LED Task started");
  bool readySignalDone = false;
  unsigned long readyStartTime = 0;

  for(;;) {
    esp_task_wdt_reset();
    
    if (configPortalActive) {
      // AP Mode: Rainbow Cycle
      static uint8_t hue = 0;
      onboardLed[0] = CHSV(hue++, 255, 255);
      FastLED.show();
      vTaskDelay(20 / portTICK_PERIOD_MS);
      continue;
    }

    bool wifiOk = (WiFi.status() == WL_CONNECTED);
    bool firebaseOk = firebaseConnected;

    if (!wifiOk || !firebaseOk) {
      // Disconnected: Solid Red
      onboardLed[0] = CRGB::Red;
      FastLED.show();
      vTaskDelay(500 / portTICK_PERIOD_MS);
    } 
    else if (!systemInitialized) {
      // Booting/Initializing: Flash Green (1s)
      static bool toggle = false;
      onboardLed[0] = toggle ? CRGB::Green : CRGB::Black;
      toggle = !toggle;
      FastLED.show();
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    } 
    else {
      // Ready (WiFi + Firebase connected and init done)
      if (!readySignalDone) {
        if (readyStartTime == 0) readyStartTime = millis();
        
        if (millis() - readyStartTime < 10000) {
          // Solid Green for 10s
          onboardLed[0] = CRGB::Green;
          FastLED.show();
        } else {
          // Turn OFF after 10s
          onboardLed[0] = CRGB::Black;
          FastLED.show();
          readySignalDone = true;
        }
      } else {
        // Keep OFF while healthy
        onboardLed[0] = CRGB::Black;
        FastLED.show();
      }
      vTaskDelay(500 / portTICK_PERIOD_MS);
    }
  }
}
