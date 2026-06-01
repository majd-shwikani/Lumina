#include "globals.h"

// ============================================================================
// ADVANCED AUDIO GLOBAL BUFFERS
// ============================================================================
__attribute__((aligned(16))) float fft_input_output[FFT_SAMPLES * 2];
__attribute__((aligned(16))) float window_coefficients[FFT_SAMPLES];
float prevMagnitudes[FFT_SAMPLES / 2] = {0};
int binToBand[FFT_SAMPLES / 2];

float bandEnergy[NUM_BANDS] = {0};
float bandSmoothed[NUM_BANDS] = {0};
float bandPeak[NUM_BANDS] = {0};

AudioData sharedAudio;

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

    // ---- Advanced Sound-Reactive Effects (22-26) -----------------------------
    case 22: effectSpectrumRipple();     break;
    case 23: effectKineticPlasma();      break;
    case 24: effectTransientPulse();     break;
    case 25: effectSpectrumBars();       break;
    case 26: effectSpectralVerve();      break;

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
// ADVANCED DSP AUDIO ENGINE
// ============================================================================

void initBandMapping() {
    float minFreq = BIN_WIDTH * 2.0f;
    float maxFreq = BIN_WIDTH * (FFT_SAMPLES / 2 - 1);
    float logMin = log10f(minFreq);
    float logMax = log10f(maxFreq);
    float logRange = logMax - logMin;

    for (int bin = 0; bin < FFT_SAMPLES / 2; bin++) {
        if (bin < 2) {
            binToBand[bin] = 0;
        } else {
            float freq = bin * BIN_WIDTH;
            float norm = (log10f(freq) - logMin) / logRange;
            int band = (int)(norm * (NUM_BANDS - 1) + 0.5f);
            binToBand[bin] = constrain(band, 0, NUM_BANDS - 1);
        }
    }
}

void audioProcessingTask(void *pvParameters) {
    if (dsps_fft2r_init_fc32(NULL, 1024) != ESP_OK) {
        Serial.println(F("FATAL: SIMD FFT init failed!"));
        vTaskDelete(NULL);
    }

    dsps_wind_hann_f32(window_coefficients, FFT_SAMPLES);
    Serial.println(F("Audio DSP task running on Core 0."));

    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLING_FREQ,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK_PIN,
        .ws_io_num = I2S_WS_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD_PIN
    };

    i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_PORT, &pin_config);

    int32_t dmaBuffer[FFT_SAMPLES];
    size_t bytesRead;

    // Global AGC & squelch state
    float agcMax = 500000.0f;
    float noiseFloor = 40000.0f;

    // Multi-band onset baselines
    float bassBaseline = 0, midBaseline = 0, highBaseline = 0;

    uint32_t frameCounter = 0;

    while (true) {
        i2s_read(I2S_PORT, &dmaBuffer, sizeof(dmaBuffer), &bytesRead, portMAX_DELAY);
        int samplesRead = bytesRead / sizeof(int32_t);
        if (samplesRead == 0) continue;

        // --- Convert to float & remove DC ---
        float mean = 0;
        for (int i = 0; i < samplesRead; i++) {
            fft_input_output[i * 2 + 0] = (float)(dmaBuffer[i] >> 8);
            mean += fft_input_output[i * 2 + 0];
        }
        mean /= samplesRead;

        for (int i = 0; i < samplesRead; i++) {
            fft_input_output[i * 2 + 0] = (fft_input_output[i * 2 + 0] - mean) * window_coefficients[i];
            fft_input_output[i * 2 + 1] = 0.0f;
        }

        // --- SIMD FFT pipeline ---
        dsps_fft2r_fc32(fft_input_output, FFT_SAMPLES);
        dsps_bit_rev_fc32(fft_input_output, FFT_SAMPLES);

        // Reset per-band accumulators
        for (int b = 0; b < NUM_BANDS; b++) bandEnergy[b] = 0;

        // --- Magnitudes, band accumulation, spectral features ---
        float fluxDelta = 0;
        float fluxTotal = 0.001f;
        float centroidWeighted = 0;
        float centroidTotal = 0.001f;

        for (int i = 2; i < FFT_SAMPLES / 2; i++) {
            float real = fft_input_output[i * 2 + 0];
            float imag = fft_input_output[i * 2 + 1];
            float mag = sqrtf(real * real + imag * imag);

            // Accumulate into log-spaced band
            bandEnergy[binToBand[i]] += mag;

            // Spectral flux (positive magnitude change)
            float diff = mag - prevMagnitudes[i];
            if (diff > 0) fluxDelta += diff;
            prevMagnitudes[i] = mag;
            fluxTotal += mag;

            // Spectral centroid (frequency-weighted sum)
            centroidWeighted += mag * (i * BIN_WIDTH);
            centroidTotal += mag;
        }

        float fluxNorm = fluxDelta / fluxTotal;
        float centroidHz = centroidWeighted / centroidTotal;
        float centroidNorm = constrain((centroidHz - 200.0f) / (20000.0f - 200.0f), 0.0f, 1.0f);

        // --- Per-band envelope followers & peak tracking ---
        float currentPeak = 0;
        for (int b = 0; b < NUM_BANDS; b++) {
            float attack  = 0.35f + (b / (float)(NUM_BANDS - 1)) * 0.25f;
            float release = 0.08f + (1.0f - b / (float)(NUM_BANDS - 1)) * 0.12f;

            if (bandEnergy[b] > bandSmoothed[b]) {
                bandSmoothed[b] += (bandEnergy[b] - bandSmoothed[b]) * attack;
            } else {
                bandSmoothed[b] += (bandEnergy[b] - bandSmoothed[b]) * release;
            }
            if (bandSmoothed[b] < 0) bandSmoothed[b] = 0;

            if (bandSmoothed[b] > bandPeak[b]) {
                bandPeak[b] = bandSmoothed[b];
            } else {
                bandPeak[b] *= 0.9993f;
            }
            if (bandPeak[b] < 10000.0f) bandPeak[b] = 10000.0f;

            if (bandSmoothed[b] > currentPeak) currentPeak = bandSmoothed[b];
        }

        // --- Noise floor & global AGC ---
        if (currentPeak < noiseFloor * 1.6f) {
            noiseFloor = (noiseFloor * 0.995f) + (currentPeak * 0.005f);
        }
        bool environmentIsSilent = (currentPeak < (noiseFloor + 15000.0f));

        if (!environmentIsSilent) {
            if (currentPeak > agcMax) agcMax = currentPeak;
            else agcMax = (agcMax * 0.9997f) + (currentPeak * 0.0003f);
            if (agcMax < 100000.0f) agcMax = 100000.0f;
        }

        // --- Multi-band onset detection ---
        float bassSum = 0, midSum = 0, highSum = 0;
        for (int b = 0; b < NUM_BANDS; b++) {
            if (b <= 3)      bassSum += bandSmoothed[b];
            else if (b <= 9) midSum  += bandSmoothed[b];
            else             highSum += bandSmoothed[b];
        }

        bool bassOnset = (bassSum > bassBaseline * 1.35f && bassSum > noiseFloor * 2.0f);
        bool midOnset  = (midSum  > midBaseline  * 1.25f && midSum  > noiseFloor * 1.5f);
        bool highOnset = (highSum > highBaseline * 1.25f && highSum > noiseFloor);

        bassBaseline = (bassBaseline * 0.7f) + (bassSum * 0.3f);
        midBaseline  = (midBaseline  * 0.7f) + (midSum  * 0.3f);
        highBaseline = (highBaseline * 0.7f) + (highSum * 0.3f);

        // --- Write to shared audio struct ---
        if (xSemaphoreTake(i2sMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            sharedAudio.volume = environmentIsSilent ? 0.0f : constrain(currentPeak / agcMax, 0.0f, 1.0f);
            sharedAudio.onset.bass = bassOnset;
            sharedAudio.onset.mid  = midOnset;
            sharedAudio.onset.high = highOnset;
            sharedAudio.centroid = centroidNorm;
            sharedAudio.flux = constrain(fluxNorm * 4.0f, 0.0f, 1.0f);

            for (int b = 0; b < NUM_BANDS; b++) {
                if (environmentIsSilent) {
                    sharedAudio.bands[b] = 0.0f;
                } else {
                    sharedAudio.bands[b] = constrain(bandSmoothed[b] / (bandPeak[b] * 1.2f), 0.0f, 1.0f);
                }
            }
            xSemaphoreGive(i2sMutex);
        }

        // --- Periodic serial status (~every 2 s at 75 fps) ---
        frameCounter++;
        if (frameCounter % 150 == 0) {
            Serial.print(F("[AUDIO] vol="));
            Serial.print(sharedAudio.volume, 2);
            Serial.print(F("  centroid="));
            Serial.print(centroidNorm, 2);
            Serial.print(F("  flux="));
            Serial.print(fluxNorm, 3);
            Serial.print(F("  onsets="));
            Serial.print(bassOnset  ? F("B") : F("-"));
            Serial.print(midOnset   ? F("M") : F("-"));
            Serial.println(highOnset ? F("H") : F("-"));

            Serial.print(F("  Bands: "));
            for (int b = 0; b < NUM_BANDS; b++) {
                Serial.print((int)(sharedAudio.bands[b] * 100));
                if (b < NUM_BANDS - 1) Serial.print(F(","));
            }
            Serial.println();

            Serial.print(F("  Centroid freq: "));
            Serial.print(centroidHz, 0);
            Serial.println(F(" Hz"));
        }

        vTaskDelay(pdMS_TO_TICKS(2));
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