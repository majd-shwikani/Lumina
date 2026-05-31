#include "globals.h"

// ============================================================================
// VARIABLE DEFINITIONS
// ============================================================================
// Note: statsTaskStack and statsTaskHandle removed - merged into sensor task

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

  // ==========================================================================
  // Audio DSP Task Lifecycle Management
  // AudioDSPTask runs ONLY when an audio-reactive effect (22-26) is active.
  // When switching away, the task is deleted to free ~8KB of RAM.
  // ==========================================================================
  {
    static bool audioTaskRunning = false;
    bool isAudioEffect = (currentEffect >= 22 && currentEffect <= 26);

    if (isAudioEffect && !audioTaskRunning) {
      xTaskCreatePinnedToCore(audioProcessingTask, "AudioDSP", 8192, NULL, 3, &audioTaskHandle, 0);
      audioTaskRunning = true;
      Serial.println("🎵 Audio DSP Task started on Core 0");
    } else if (!isAudioEffect && audioTaskRunning) {
      if (audioTaskHandle != NULL) {
        vTaskDelete(audioTaskHandle);
        audioTaskHandle = NULL;
      }
      audioTaskRunning = false;
      Serial.println("🎵 Audio DSP Task stopped — RAM freed");
    }
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

    // ---- Advanced Audio-Reactive Effects (22-26) -----------------------------
    case 22: effectSpectrumRipple();    break;
    case 23: effectKineticPlasma();     break;
    case 24: effectTransientPulse();    break;
    case 25: effectSpectrumBars();      break;
    case 26: effectSpectralVerve();     break;

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
  // Now only sets a flag for cloudTask to handle
  pendingTimerUpdate = true;
  targetTimerState = state;
  Serial.printf("⏱️  [Timer] Signaling LEDs → %s\n", state ? "ON" : "OFF");
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

// ============================================================================
// CLOUD SYNC TASK (Merges Firebase & Sensors)
// ============================================================================

void cloudTask(void *parameter) {
  Serial.println("☁️  Cloud Task started on Core " + String(xPortGetCoreID()));
  
  const TickType_t xDelay = 2000 / portTICK_PERIOD_MS;
  uint32_t lastStatsUpload = 0;
  static uint32_t taskStartTime = millis();

  for(;;) {
    esp_task_wdt_reset();
    
    if (WiFi.status() == WL_CONNECTED && Firebase.ready()) {
      firebaseConnected = true;
      ArduinoOTA.handle();

      // 1. Process Receiver Registry Changes (Every loop if needed)
      if (registryChanged) {
        bool allSynced = true;
        for (int i = 0; i < receiverCount; i++) {
          if (receivers[i].needsFirebaseSync) {
            String rPath = basePath + "/receivers/" + receivers[i].macStr;
            FirebaseJson rJson;
            rJson.set("isMirror", receivers[i].isMirror);
            rJson.set("effect", receivers[i].effect);
            rJson.set("speed", receivers[i].speed);
            rJson.set("color", "FF0000");
            rJson.set("enabled", receivers[i].enabled);
            
            if (Firebase.RTDB.setJSON(&fbdoSender, rPath.c_str(), &rJson)) {
              receivers[i].needsFirebaseSync = false;
            } else {
              allSynced = false;
            }
          }
        }
        if (allSynced) {
          updateActiveNodesInFirebase();
          registryChanged = false;
        }
      }

      // 2. Process Firebase Request Queue
      FirebaseRequest req;
      while (firebaseQueue != NULL && xQueueReceive(firebaseQueue, &req, 0) == pdPASS) {
        bool success = false;
        switch (req.method) {
          case FB_SET_BOOL:
            success = Firebase.RTDB.setBool(&fbdoSender, req.path, strcmp(req.payload, "true") == 0);
            break;
          case FB_SET_INT:
            success = Firebase.RTDB.setInt(&fbdoSender, req.path, atoi(req.payload));
            break;
          case FB_SET_FLOAT:
            success = Firebase.RTDB.setFloat(&fbdoSender, req.path, atof(req.payload));
            break;
          case FB_SET_STRING:
            success = Firebase.RTDB.setString(&fbdoSender, req.path, req.payload);
            break;
          case FB_SET_JSON:
            FirebaseJson json;
            json.setJsonData(req.payload);
            success = Firebase.RTDB.setJSON(&fbdoSender, req.path, &json);
            break;
        }
        if (!success) {
          Serial.printf("❌ [CloudTask] Queue update failed for %s: %s\n", req.path, fbdoSender.errorReason().c_str());
        }
      }

      // 3. Process Pending Flag-based Updates
      if (pendingTimerUpdate) {
        String enabledPath = basePath + "/local/enabled";
        if (Firebase.RTDB.setBool(&fbdoSender, enabledPath.c_str(), targetTimerState)) {
          portENTER_CRITICAL(&stripMux);
          stripEnabled = targetTimerState;
          portEXIT_CRITICAL(&stripMux);
          if (targetTimerState) {
            portENTER_CRITICAL(&stripMux);
            manuallyTurnedOff = false;
            portEXIT_CRITICAL(&stripMux);
          }
          Serial.printf("⏱️  [CloudTask] Timer state pushed to Firebase: %s\n", targetTimerState ? "ON" : "OFF");
          syncAllMirrors();
          pendingTimerUpdate = false;
        }
      }

      if (pendingMQTTConfigUpdate) {
        Serial.println("ℹ️ MQTT configuration change from Firebase is now ignored. Use Web Portal.");
        pendingMQTTConfigUpdate = false;
      }

      // 4. Collect and Upload Sensor/Stats (Every 2 seconds)
      if (millis() - lastStatsUpload >= 2000) {
        lastStatsUpload = millis();
        
        bool present = (digitalRead(RADAR_OUTPUT) == HIGH);
        lastPresence = present;

        FirebaseJson json, sensors, stats;
        
        sensors.set("lux", currentLux);
        sensors.set("presence", present);
        sensors.set("voltage", currentVoltage);
        sensors.set("current", currentCurrent);
        sensors.set("power", currentPower);
        sensors.set("cpu_temp", currentCpuTemp);

        uint32_t totalHeap = ESP.getHeapSize();
        uint32_t freeHeap = ESP.getFreeHeap();
        uint32_t usedHeap = totalHeap - freeHeap;
        String systemStatus = systemHealthy ? "ALL SYSTEMS NOMINAL" : "SYSTEM WARNINGS";
        
        stats.set("uptime", formatUptime(millis() - taskStartTime));
        stats.set("status", systemStatus);
        stats.set("heap_usage_percent", (usedHeap * 100.0) / totalHeap);
        stats.set("free_heap_kb", freeHeap / 1024);
        stats.set("min_free_heap", ESP.getMinFreeHeap());
        stats.set("total_heap_kb", totalHeap / 1024);
        stats.set("wifi_rssi", WiFi.RSSI());
        stats.set("loop_counter", loopCounter);
        stats.set("firmware_version", currentFirmwareVersion);

        #ifdef BOARD_HAS_PSRAM
        uint32_t psramSize = ESP.getPsramSize();
        if (psramSize > 0) {
          stats.set("psram_size_kb", psramSize / 1024);
          stats.set("free_psram_kb", ESP.getFreePsram() / 1024);
          stats.set("min_free_psram", ESP.getMinFreePsram());
        }
        #endif

        json.set("sensors", sensors);
        json.set("stats", stats);
        Firebase.RTDB.setJSON(&fbdoSender, (basePath + "/local/sensor_stats").c_str(), &json);
      }
    } else {
      firebaseConnected = false;
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// ============================================================================
// SYSTEM MAINTENANCE TASK (Merges Automation, Timer, OTA, Status LED)
// ============================================================================

void systemTask(void *parameter) {
  Serial.println("⚙️  System Task started on Core " + String(xPortGetCoreID()));
  
  uint32_t lastOtaCheck = 0;
  uint32_t lastTimerCheck = 0;
  uint32_t lastStatusUpdate = 0;
  uint32_t lastAutomationLog = 0;
  uint32_t lastStateChangeTime = 0;
  
  bool lastOnTriggered = false;
  bool lastOffTriggered = false;
  bool readySignalDone = false;
  uint32_t readyStartTime = 0;
  
  for(;;) {
    esp_task_wdt_reset();
    uint32_t now = millis();

    // 1. AUTOMATION LOGIC (Only runs after full system init)
    if (systemInitialized) {
      if (sensorAvailable) updateSensorData();
      bool presenceDetected = (digitalRead(RADAR_OUTPUT) == HIGH);
      lastPresence = presenceDetected;

      bool currentEnabled;
      portENTER_CRITICAL(&stripMux);
      currentEnabled = stripEnabled;
      portEXIT_CRITICAL(&stripMux);

      if (manuallyTurnedOff) {
        if (currentEnabled) {
          portENTER_CRITICAL(&stripMux);
          stripEnabled = false;
          portEXIT_CRITICAL(&stripMux);
          syncAllMirrors();
        }
      } else {
        bool targetState = true;
        if (presenceDetectionEnabled && autoDarknessControl) targetState = (presenceDetected && (currentLux < luxThreshold));
        else if (presenceDetectionEnabled) targetState = presenceDetected;
        else if (autoDarknessControl) targetState = (currentLux < luxThreshold);

        if (targetState != currentEnabled && (now - lastStateChangeTime > 3000)) {
          portENTER_CRITICAL(&stripMux);
          stripEnabled = targetState;
          portEXIT_CRITICAL(&stripMux);
          lastStateChangeTime = now;
          syncAllMirrors();
          Serial.printf("⚙️  [Automation] LEDs → %s (presence=%s, lux=%.2f)\n",
                        targetState ? "ON" : "OFF",
                        presenceDetected ? "yes" : "no",
                        currentLux);
        }
      }
    }

    // 2. TIMER LOGIC (Only runs after full system init)
    if (systemInitialized && (now - lastTimerCheck >= 1000)) {
      lastTimerCheck = now;
      if (firebaseConnected && timerEnabled) {
        if (checkTimeMatch(timerOnTime)) {
          if (!lastOnTriggered) { updateTimerState(true); lastOnTriggered = true; }
        } else lastOnTriggered = false;
        
        if (checkTimeMatch(timerOffTime)) {
          if (!lastOffTriggered) { updateTimerState(false); lastOffTriggered = true; }
        } else lastOffTriggered = false;
      }
    }

    // 3. STATUS LED (Runs ALWAYS - provides the boot animation)
    {
      static uint8_t rainbowHue = 0;
      bool doUpdate = false;

      if (!configPortalActive) {
        bool wifiOk = (WiFi.status() == WL_CONNECTED);

        if (!systemInitialized) {
          // ── BOOTING: smooth fast rainbow — runs every 10ms task tick ─────
          onboardLed[0] = CHSV(rainbowHue, 255, 200);
          rainbowHue += 3;   // 3 per 10ms = ~1.2 full cycles/sec, buttery smooth
          doUpdate = true;
          lastStatusUpdate = now;
        } else if (!wifiOk || !firebaseConnected) {
          // ── NO WIFI / FIREBASE: solid red ─────────────────────────────────
          if (now - lastStatusUpdate >= 500) {
            lastStatusUpdate = now;
            onboardLed[0] = CRGB::Red;
            doUpdate = true;
          }
        } else {
          // ── FULLY BOOTED: green for 10 s, then off ────────────────────────
          if (now - lastStatusUpdate >= 500) {
            lastStatusUpdate = now;
            if (!readySignalDone) {
              if (readyStartTime == 0) readyStartTime = now;
              if (now - readyStartTime < 10000) onboardLed[0] = CRGB::Green;
              else { onboardLed[0] = CRGB::Black; readySignalDone = true; }
            } else {
              onboardLed[0] = CRGB::Black;
            }
            doUpdate = true;
          }
        }

        if (doUpdate) FastLED.show();
      }
    }

    // 4. OTA CHECK (Only runs after full system init)
    if (systemInitialized && (now - lastOtaCheck >= UPDATE_CHECK_INTERVAL || lastOtaCheck == 0)) {
      if (lastOtaCheck == 0) lastOtaCheck = now; // Delay first check
      else {
        lastOtaCheck = now;
        if (WiFi.status() == WL_CONNECTED && firebaseConnected) checkForGitHubUpdate();
      }
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);  // 10ms tick: enables smooth LED updates
  }
}

void ledTask(void *parameter) {
  Serial.println("💡 LED Task started on Core " + String(xPortGetCoreID()));
  for(;;) {
    esp_task_wdt_reset();
    // Only update LEDs if not actively receiving a pixel stream
    if (!usbPixelStreamActive) {
      updateLEDs();
    }
    vTaskDelay(effectSpeed / portTICK_PERIOD_MS);
  }
}

// ============================================================================
// USB DATA TASK (Core 0)
// ============================================================================

void usbDataTask(void *parameter) {
  Serial.println("🔌 USB Data Task started on Core " + String(xPortGetCoreID()));
  
  // High speed serial configuration
  Serial.setRxBufferSize(2048); // Increase buffer for high-speed data
  while(Serial.available() > 0) Serial.read();

  const uint8_t magic_lumi[] = {0x4C, 0x55, 0x4D, 0x49}; // "LUMI"
  const uint8_t cmd_handshake = 0xCC;
  const uint8_t cmd_data = 0xBB;
  const uint8_t cmd_audio = 0xAA;
  
  for(;;) {
    esp_task_wdt_reset();

    // Look for our 4-byte "LUMI" magic header
    if (Serial.available() >= 5) { // 4 bytes magic + 1 byte command
      bool match = true;
      for(int i = 0; i < 4; i++) {
        if (Serial.peek() == magic_lumi[i]) {
            Serial.read();
        } else {
            match = false;
            break;
        }
      }

      if (match) {
        uint8_t cmd = Serial.read();
        
        if (cmd == cmd_handshake) {
          uint8_t response[2];
          response[0] = (ledCount >> 8) & 0xFF;
          response[1] = ledCount & 0xFF;
          Serial.write(response, 2);
          Serial.flush();
        }
        else if (cmd == cmd_data) {
          usbPixelStreamActive = true;
          lastUsbPixelTime = millis();
          
          int bytesToRead = ledCount * 3;
          uint8_t* rawLeds = (uint8_t*)leds;
          
          Serial.setTimeout(50); 
          size_t bytesRead = Serial.readBytes(rawLeds, bytesToRead);

          if (bytesRead == bytesToRead) {
            FastLED.show();
          } else {
            while(Serial.available() > 0) Serial.read();
          }
        }
        else if (cmd == cmd_audio) {
          uint8_t audioData[16];
          Serial.setTimeout(10);
          size_t bytesRead = Serial.readBytes(audioData, 16);
          
          if (bytesRead == 16) {
            audioMirrorMode = true;
            lastUsbAudioTime = millis();
            
            for (int i = 0; i < NUM_FREQ_BANDS; i++) {
              bandMagnitudes[i] = audioData[i] / 255.0;
            }
          }
        }
      } else {
          // If no match, discard one byte and try again
          if (Serial.available() > 0) Serial.read();
      }
    }
    
    // Timeout check for Pixel Stream (2 seconds)
    if (usbPixelStreamActive && (millis() - lastUsbPixelTime > 2000)) {
        usbPixelStreamActive = false;
        Serial.println("🖥️  USB Pixel stream timeout, resuming effects...");
    }

    // Timeout check for Audio Stream (2 seconds)
    if (audioMirrorMode && (millis() - lastUsbAudioTime > 2000)) {
        audioMirrorMode = false;
        Serial.println("🎵 USB Audio stream timeout, reverting to microphone...");
    }

    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

void toggleUsbMirror(bool enable, bool isAudio) {
  if (enable) {
    usbMirrorActive = true;
    if (usbDataTaskHandle == NULL) {
        Serial.println("🔌 Enabling USB Data Task...");
        xTaskCreatePinnedToCore(usbDataTask, "USBTask", 4000, NULL, 2, &usbDataTaskHandle, 0);
    }
  } 
  else {
    usbMirrorActive = false;
    audioMirrorMode = false;
    usbPixelStreamActive = false;
    
    // Clear LEDs when exiting mirror mode
    FastLED.clear();
    FastLED.show();
    
    if (usbDataTaskHandle != NULL) {
      Serial.println("🔌 Disabling USB Data Task...");
      vTaskDelete(usbDataTaskHandle);
      usbDataTaskHandle = NULL;
    }
  }
}