#include "globals.h"

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
    strip.clear();
    strip.show();
    lastStripEnabled = false;
    return;
  }
  
  if (!currentEnabled) {
    return;
  }
  
  if (currentEnabled && !lastStripEnabled) {
    lastStripEnabled = true;
  }
  
  switch(currentEffect) {
    case 0: effectRainbow(); break;
    case 1: effectMeteorShower(); break;
    case 2: effectDigitalRain(); break;
    case 3: effectPulsingSpheres(); break;
    case 4: effectBinaryClock(); break;
    case 5: effectVortex(); break;
    case 6: effectDNAHelix(); break;
    case 7: effectAudioVisualizer(); break;
    case 8: effectLavaLamp(); break;
    case 9: effectRadarSweep(); break;
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
    case 22: effectFrequencySpectrum(); break;
    case 23: effectReactiveWaveform(); break;
    case 24: effectBeatPulse(); break;
    case 25: effectFrequencyBloom(); break;
    case 26: effectAudioReactiveFire(); break;
    case 27: effectMusicalRainbow(); break;
    case 28: effectReactiveStrobe(); break;
    case 29: effectGuitarVisualizer(); break;
    case 30: effectCascadingFrequency(); break;
    case 31: effectEnergyOrbits(); break;
    case 32: effectAudioRipples(); break;
    
    default: effectRainbow(); break;
  }
  strip.show();
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
  
  if (manuallyTurnedOff && state) {
    Serial.println("⚠️  Timer cannot turn on LEDs - manually locked off");
    return;
  }
  
  String enabledPath = basePath + "/enabled";
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
    
    if (!state) {
      strip.clear();
      strip.show();
    }
  } else {
    Serial.printf("Failed to update enabled state: %s\n", fbdoUpload.errorReason().c_str());
  }
  esp_task_wdt_reset();
}

// ============================================================================
// FREERTOS TASKS
// ============================================================================

void firebaseTask(void *parameter) {
  Serial.println("🔥 Firebase Task started on Core " + String(xPortGetCoreID()));
  
  for(;;) {
    esp_task_wdt_reset();
    
    if (WiFi.status() == WL_CONNECTED) {
      ArduinoOTA.handle();
      
      if (!Firebase.ready()) {
        Serial.println("⚠️  Firebase not ready, reconnecting...");
        firebaseConnected = false;
        delay(1000);
      } else {
        firebaseConnected = true;
      }
    } else {
      firebaseConnected = false;
      Serial.println("⚠️  WiFi disconnected in Firebase task, attempting reconnect...");
      connectToWiFi();
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
  
  vTaskDelay(3000 / portTICK_PERIOD_MS);

  for(;;) {
    esp_task_wdt_reset();
    
    if (sensorAvailable) {
      updateSensorData();
    }
    radar.read();
    bool presenceDetected = radar.presenceDetected();
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
        strip.clear();
        strip.show();
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
          strip.clear();
          strip.show();
          Serial.printf("\n🌙 LEDs OFF: %s (Presence: %s, Lux: %.2f)\n", 
                       reason.c_str(), presenceDetected ? "Yes" : "No", currentLux);
        }
      }
    }

    vTaskDelay(xDelay);
  }
}

void sensorDataTask(void *parameter) {
  Serial.println("📊 Sensor Data Task started on Core " + String(xPortGetCoreID()));
  
  const TickType_t xDelay = 2000 / portTICK_PERIOD_MS;
  const unsigned long FIREBASE_OPERATION_TIMEOUT = 10000;
  const unsigned long NETWORK_CHECK_TIMEOUT = 5000;
  
  static bool lastPresenceSent = false;
  static unsigned long lastPresenceSend = 0;
  static unsigned long lastNetworkCheck = 0;
  
  unsigned long operationStart = 0;
  unsigned long operationDuration = 0;
  
  int consecutiveFailures = 0;
  const int MAX_CONSECUTIVE_FAILURES = 5;
  
  for(;;) {
    esp_task_wdt_reset();
    
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
    
    if (sensorAvailable) {
      esp_task_wdt_reset();
      
      operationStart = millis();
      updateSensorData();
      operationDuration = millis() - operationStart;
      
      if (operationDuration > 2000) {
        Serial.printf("⚠️  [SensorTask] updateSensorData took %lu ms\n", operationDuration);
      }
    }
    
    esp_task_wdt_reset();
    
    operationStart = millis();
    radar.read();
    bool present = radar.presenceDetected();
    operationDuration = millis() - operationStart;
    
    if (operationDuration > 1000) {
      Serial.printf("⚠️  [SensorTask] radar.read() took %lu ms\n", operationDuration);
    }
    
    if (firebaseConnected && WiFi.status() == WL_CONNECTED) {
      
      if (sensorAvailable) {
        esp_task_wdt_reset();
        
        String luxPath = basePath + "/lux";
        operationStart = millis();
        
        bool luxSuccess = Firebase.RTDB.setFloat(&fbdoUpload, luxPath.c_str(), currentLux);
        operationDuration = millis() - operationStart;
        
        if (luxSuccess) {
          consecutiveFailures = 0;
          
          if (operationDuration > 3000) {
            Serial.printf("⚠️  [SensorTask] Lux upload took %lu ms (slow but successful)\n", operationDuration);
          }
        } else {
          consecutiveFailures++;
          Serial.printf("⚠️  [SensorTask] Failed to send lux data (%d/%d): %s\n", 
                       consecutiveFailures, MAX_CONSECUTIVE_FAILURES, 
                       fbdoUpload.errorReason().c_str());
          
          if (consecutiveFailures >= MAX_CONSECUTIVE_FAILURES) {
            Serial.println("❌ [SensorTask] Too many consecutive failures - marking Firebase as disconnected");
            firebaseConnected = false;
            consecutiveFailures = 0;
          }
        }
        
        if (operationDuration > FIREBASE_OPERATION_TIMEOUT) {
          Serial.printf("❌ [SensorTask] Lux upload TIMEOUT (%lu ms) - may need to reset Firebase connection\n", 
                       operationDuration);
          firebaseConnected = false;
        }
      }
      
      esp_task_wdt_reset();
      
      bool shouldUploadPresence = (present != lastPresenceSent) || 
                                   ((millis() - lastPresenceSend) > 5000);
      
      if (shouldUploadPresence && firebaseConnected) {
        String presencePath = basePath + "/presence";
        operationStart = millis();
        
        bool presenceSuccess = Firebase.RTDB.setBool(&fbdoUpload, presencePath.c_str(), present);
        operationDuration = millis() - operationStart;
        
        if (presenceSuccess) {
          lastPresenceSent = present;
          lastPresenceSend = millis();
          consecutiveFailures = 0;
          
          if (operationDuration > 3000) {
            Serial.printf("⚠️  [SensorTask] Presence upload took %lu ms (slow but successful)\n", 
                         operationDuration);
          }
        } else {
          consecutiveFailures++;
          Serial.printf("⚠️  [SensorTask] Failed to send presence (%d/%d): %s\n",
                       consecutiveFailures, MAX_CONSECUTIVE_FAILURES,
                       fbdoUpload.errorReason().c_str());
          
          if (consecutiveFailures >= MAX_CONSECUTIVE_FAILURES) {
            Serial.println("❌ [SensorTask] Too many consecutive failures - marking Firebase as disconnected");
            firebaseConnected = false;
            consecutiveFailures = 0;
          }
        }
        
        if (operationDuration > FIREBASE_OPERATION_TIMEOUT) {
          Serial.printf("❌ [SensorTask] Presence upload TIMEOUT (%lu ms) - may need to reset Firebase connection\n",
                       operationDuration);
          firebaseConnected = false;
        }
      }
      
    } else {
      if (!firebaseConnected) {
      } else if (WiFi.status() != WL_CONNECTED) {
        Serial.println("⚠️  [SensorTask] WiFi disconnected - skipping Firebase uploads");
      }
    }
    
    esp_task_wdt_reset();
    
    if (sensorTaskStack < 1000 && sensorTaskStack > 0) {
      Serial.printf("⚠️  [SensorTask] Stack running low: %d bytes remaining\n", 
                   sensorTaskStack * sizeof(StackType_t));
    }
    
    esp_task_wdt_reset();
    
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