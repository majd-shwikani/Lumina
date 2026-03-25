#include "globals.h"

// ============================================================================
// VARIABLE DEFINITIONS
// ============================================================================
// Note: statsTaskStack and statsTaskHandle removed - merged into sensor task
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

// ============================================================================
// UPTIME FORMATTER - MOVED FROM STATS TASK
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
    } else {
      firebaseConnected = false;
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("⚠️  WiFi disconnected in Firebase task, attempting reconnect...");
        connectToWiFi();
      }
    }
    
    vTaskDelay(1000 / portTICK_PERIOD_MS);
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

void automationtask(void *parameter) {
  Serial.println("⚙️  Automation Task started on Core " + String(xPortGetCoreID()));
  
  const TickType_t xDelay = 100 / portTICK_PERIOD_MS;
  static unsigned long lastStateChangeTime = 0;
  const unsigned long CHANGE_LOCKOUT_MS = 3000;
  
  static bool lastPresenceDetected = false;
  static bool lastTargetState = false;
  static unsigned long lastLogTime = 0;
  
  vTaskDelay(3000 / portTICK_PERIOD_MS);

  for(;;) {
    esp_task_wdt_reset();
    
    if (sensorAvailable) {
      updateSensorData();
    }
    bool presenceDetected = (digitalRead(RADAR_OUTPUT) == HIGH);
    lastPresence = presenceDetected;

    if (manuallyTurnedOff) {
      bool currentEnabled;
      portENTER_CRITICAL(&stripMux);
      currentEnabled = stripEnabled;
      portEXIT_CRITICAL(&stripMux);
      
      if (currentEnabled) {
        portENTER_CRITICAL(&stripMux);
        stripEnabled = false;
        portEXIT_CRITICAL(&stripMux);
        syncAllMirrors();
      }
      vTaskDelay(xDelay);
      continue;
    }

    bool targetState = false;
    String reason = "";

    bool presenceCondition = true;
    bool darknessCondition = true;
    
    if (presenceDetectionEnabled) {
      presenceCondition = presenceDetected;
    }
    
    if (autoDarknessControl) {
      bool currentEnabled;
      portENTER_CRITICAL(&stripMux);
      currentEnabled = stripEnabled;
      portEXIT_CRITICAL(&stripMux);
      
      if (currentEnabled) {
        darknessCondition = (currentLux < luxThreshold);
      } else {
        darknessCondition = (currentLux < luxThreshold);
      }
    }
    
    if (presenceDetectionEnabled && autoDarknessControl) {
      targetState = presenceCondition && darknessCondition;
      if (targetState) {
        reason = "Presence + Darkness";
      } else if (!presenceCondition) {
        reason = "No presence";
      } else {
        reason = "Too bright";
      }
    }
    else if (presenceDetectionEnabled && !autoDarknessControl) {
      targetState = presenceCondition;
      reason = presenceCondition ? "Presence detected" : "No presence";
    }
    else if (!presenceDetectionEnabled && autoDarknessControl) {
      targetState = darknessCondition;
      reason = darknessCondition ? "Dark enough" : "Too bright";
    }
    else {
      targetState = true;
      reason = "Always ON";
    }

    bool currentEnabled;
    portENTER_CRITICAL(&stripMux);
    currentEnabled = stripEnabled;
    portEXIT_CRITICAL(&stripMux);

    if (presenceDetected != lastPresenceDetected || targetState != lastTargetState || (millis() - lastLogTime > 5000)) {
      Serial.printf("[Automation] State: presence: %d, presenceEnabled: %d, darknessEnabled: %d, lux: %.2f | Target: %d, Current: %d, Reason: %s\n",
          presenceDetected, presenceDetectionEnabled, autoDarknessControl, currentLux, targetState, currentEnabled, reason.c_str());
      lastPresenceDetected = presenceDetected;
      lastTargetState = targetState;
      lastLogTime = millis();
    }

    // CRITICAL FIX 7: Thread-safe state changes
    if (targetState != currentEnabled) {
      if (millis() - lastStateChangeTime > CHANGE_LOCKOUT_MS) {
        // Use critical section
        portENTER_CRITICAL(&stripMux);
        stripEnabled = targetState;
        portEXIT_CRITICAL(&stripMux);
        
        lastStateChangeTime = millis();
        
        if (targetState) {
          Serial.printf("\n💡 LEDs ON: %s (Presence: %s, Lux: %.2f)\n", 
                       reason.c_str(), presenceDetected ? "Yes" : "No", currentLux);
        } else {
          Serial.printf("\n🌙 LEDs OFF: %s (Presence: %s, Lux: %.2f)\n", 
                       reason.c_str(), presenceDetected ? "Yes" : "No", currentLux);
        }
        syncAllMirrors();
      }
    }

    vTaskDelay(xDelay);
  }
}

// ============================================================================
// SENSOR DATA TASK - NOW INCLUDES STATS COLLECTION
// ============================================================================

void sensorDataTask(void *parameter) {
  Serial.println("📊 Sensor+Stats Task started on Core " + String(xPortGetCoreID()));
  Serial.println("   📡 Sending combined data every 2 seconds to /sensor_stats");
  
  const TickType_t xDelay = 2000 / portTICK_PERIOD_MS; // Every 2 seconds
  const unsigned long FIREBASE_OPERATION_TIMEOUT = 10000;
  const unsigned long NETWORK_CHECK_TIMEOUT = 5000;
  
  static unsigned long lastNetworkCheck = 0;
  static unsigned long taskStartTime = millis(); // For uptime calculation
  
  unsigned long operationStart = 0;
  unsigned long operationDuration = 0;
  
  int consecutiveFailures = 0;
  const int MAX_CONSECUTIVE_FAILURES = 5;
  
  for(;;) {
    esp_task_wdt_reset();
    
    // ========================================================================
    // 1. CHECK NETWORK CONNECTION (every 5 seconds)
    // ========================================================================
    if (millis() - lastNetworkCheck > NETWORK_CHECK_TIMEOUT) {
      lastNetworkCheck = millis();
      
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("⚠️  [SensorTask] WiFi disconnected - skipping Firebase operations");
        firebaseConnected = false;
        esp_task_wdt_reset();
        vTaskDelay(xDelay);
        continue;
      }
      
      if (!Firebase.ready()) {
        Serial.println("⚠️  [SensorTask] Firebase not ready - skipping operations");
        firebaseConnected = false;
        esp_task_wdt_reset();
        vTaskDelay(xDelay);
        continue;
      }
    }
    
    // ========================================================================
    // 2. COLLECT SENSOR DATA (always do this - other tasks depend on it)
    // ========================================================================
    if (sensorAvailable) {
      esp_task_wdt_reset();
      updateSensorData(); // Reads currentLux
    }
    
    esp_task_wdt_reset();
    bool present = (digitalRead(RADAR_OUTPUT) == HIGH);
    lastPresence = present; // Update global for automation task
    
    // ========================================================================
    // 3. SEND COMBINED DATA TO FIREBASE (if connected)
    // ========================================================================
    if (firebaseConnected && WiFi.status() == WL_CONNECTED) {
      esp_task_wdt_reset();
      operationStart = millis();
      
      // ----------------------------------------------------------------------
      // 3A. COLLECT SYSTEM STATS
      // ----------------------------------------------------------------------
      uint32_t freeHeap = ESP.getFreeHeap();
      uint32_t totalHeap = ESP.getHeapSize();
      uint32_t minFreeHeap = ESP.getMinFreeHeap();
      uint32_t usedHeap = totalHeap - freeHeap;
      float heapUsagePercent = (usedHeap * 100.0) / totalHeap;
      
      // Get PSRAM stats if available
      #ifdef BOARD_HAS_PSRAM
        uint32_t psramSize = ESP.getPsramSize();
        uint32_t freePsram = ESP.getFreePsram();
      #else
        uint32_t psramSize = 0;
        uint32_t freePsram = 0;
      #endif
      
      String systemStatus = systemHealthy ? "ALL SYSTEMS NOMINAL" : "SYSTEM WARNINGS";
      
      // ----------------------------------------------------------------------
      // 3B. CREATE COMBINED JSON WITH SENSORS AND STATS
      // ----------------------------------------------------------------------
      FirebaseJson json;
      FirebaseJson sensors;
      FirebaseJson stats;
      
      // Add sensor data
      sensors.set("lux", currentLux);
      sensors.set("presence", present);
      sensors.set("voltage", currentVoltage);
      sensors.set("current", currentCurrent);
      sensors.set("power", currentPower);
      sensors.set("cpu_temp", currentCpuTemp);
      
      // Add system stats
      stats.set("uptime", formatUptime(millis() - taskStartTime));
      stats.set("status", systemStatus);
      stats.set("heap_usage_percent", heapUsagePercent);
      stats.set("free_heap_kb", freeHeap / 1024);
      stats.set("min_free_heap", minFreeHeap);
      stats.set("total_heap_kb", totalHeap / 1024);
      stats.set("wifi_rssi", WiFi.RSSI());
      stats.set("loop_counter", loopCounter);
      stats.set("firmware_version", currentFirmwareVersion);
      
      if (psramSize > 0) {
        stats.set("psram_size_kb", psramSize / 1024);
        stats.set("free_psram_kb", freePsram / 1024);
        stats.set("min_free_psram", ESP.getMinFreePsram());
      }
      
      // Combine into main JSON with your specified structure
      json.set("sensors", sensors);
      json.set("stats", stats);
      
      // ----------------------------------------------------------------------
      // 3C. SEND SINGLE FIREBASE UPDATE EVERY 2 SECONDS
      // ----------------------------------------------------------------------
      String combinedPath = basePath + "/local/sensor_stats";
      bool success = false;
      
      try {
        success = Firebase.RTDB.setJSON(&fbdoUpload, combinedPath.c_str(), &json);
      } catch (...) {
        Serial.println("⚠️  [SensorTask] CAUGHT Firebase exception - continuing...");
        success = false;
      }
      
      operationDuration = millis() - operationStart;
      
      if (success) {
        consecutiveFailures = 0;
        
        // Optional debug - uncomment to see when data is sent
        // Serial.printf("✅ [SensorTask] Combined data sent in %lu ms\n", operationDuration);
        
        if (operationDuration > 3000) {
          Serial.printf("⚠️  [SensorTask] Combined upload took %lu ms (slow but successful)\n", operationDuration);
        }
      } else {
        consecutiveFailures++;
        Serial.printf("⚠️  [SensorTask] Failed to send combined data (%d/%d): %s\n", 
                     consecutiveFailures, MAX_CONSECUTIVE_FAILURES, 
                     fbdoUpload.errorReason().c_str());
        
        if (consecutiveFailures >= MAX_CONSECUTIVE_FAILURES) {
          Serial.println("❌ [SensorTask] Too many consecutive failures - marking Firebase as disconnected");
          firebaseConnected = false;
          consecutiveFailures = 0;
        }
      }
      
      if (operationDuration > FIREBASE_OPERATION_TIMEOUT) {
        Serial.printf("❌ [SensorTask] Combined upload TIMEOUT (%lu ms) - may need to reset Firebase connection\n", 
                     operationDuration);
        firebaseConnected = false;
      }
      
    } else {
      if (!firebaseConnected) {
        // Firebase not connected - wait and retry
      } else if (WiFi.status() != WL_CONNECTED) {
        Serial.println("⚠️  [SensorTask] WiFi disconnected - skipping Firebase uploads");
      }
    }
    
    esp_task_wdt_reset();
    
    // ========================================================================
    // 4. MONITOR TASK STACK USAGE
    // ========================================================================
    if (sensorTaskHandle != NULL) {
      sensorTaskStack = uxTaskGetStackHighWaterMark(sensorTaskHandle);
      if (sensorTaskStack < 1000 && sensorTaskStack > 0) {
        Serial.printf("⚠️  [SensorTask] Stack running low: %d bytes remaining\n", 
                     sensorTaskStack * sizeof(StackType_t));
      }
    }
    
    esp_task_wdt_reset();
    
    // ========================================================================
    // 5. DELAY FOR NEXT CYCLE (2 seconds)
    // ========================================================================
    vTaskDelay(xDelay);
  }
}

void timerTask(void *parameter) {
  Serial.println("⏰ Timer Task started on Core " + String(xPortGetCoreID()));
  
  const TickType_t xDelay = 1000 / portTICK_PERIOD_MS;
  bool lastOnTriggered = false;
  bool lastOffTriggered = false;
  
  for(;;) {
    esp_task_wdt_reset();
    
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
    
    vTaskDelay(xDelay);
  }
}

void otaUpdateTask(void *parameter) {
  Serial.println("🔄 OTA Update Task started on Core " + String(xPortGetCoreID()));
  
  const TickType_t xDelay = UPDATE_CHECK_INTERVAL / portTICK_PERIOD_MS;
  
  vTaskDelay(30000 / portTICK_PERIOD_MS);
  
  for(;;) {
    esp_task_wdt_reset();
    
    if (WiFi.status() == WL_CONNECTED && firebaseConnected) {
      Serial.println("🔍 Checking for firmware updates...");
      checkForGitHubUpdate();
    } else {
      Serial.println("⚠️  Skipping update check - network not ready");
    }
    
    esp_task_wdt_reset();
    vTaskDelay(xDelay);
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

// ============================================================================
// NOTE: statsTask() function has been REMOVED - merged into sensorDataTask()
// ============================================================================