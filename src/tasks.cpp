#include "globals.h"

// ============================================================================
// STUBS FOR LOGGING AND GATEWAY FUNCTIONS
// ============================================================================
void logToPSRAM(const char* format, ...) {
  // Stub - empty for now to fix compilation
}

void broadcastGatewayState() {
  // Stub - empty for now
}

void syncAllMirrors() {
  // Stub - empty for now
}

void routeCommandToReceiver(int index) {
  // Stub - empty for now
}

void updateActiveNodesInFirebase() {
  // Stub - empty for now
}

void setupEspNowGateway() {
  // Stub - empty for now
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
  
  // NEW STRUCTURE: /local/enabled
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
    
    if (WiFi.status() == WL_CONNECTED) {
      ArduinoOTA.handle();
      
      if (!Firebase.ready()) {
        firebaseConnected = false;
      } else {
        firebaseConnected = true;
      }
    } else {
      firebaseConnected = false;
      if (WiFi.status() != WL_CONNECTED) {
          Serial.println("⚠️  WiFi disconnected in Firebase task, attempting reconnect...");
          connectToWiFi();
      }
    }
    
    vTaskDelay(100 / portTICK_PERIOD_MS);
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

    if (targetState != currentEnabled) {
      if (millis() - lastStateChangeTime > CHANGE_LOCKOUT_MS) {
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
      }
    }

    vTaskDelay(xDelay);
  }
}

void sensorDataTask(void *parameter) {
  Serial.println("📊 Sensor+Stats Task started on Core " + String(xPortGetCoreID()));
  
  const TickType_t xDelay = 2000 / portTICK_PERIOD_MS;
  static unsigned long taskStartTime = millis();
  
  for(;;) {
    esp_task_wdt_reset();
    
    if (WiFi.status() == WL_CONNECTED && Firebase.ready()) {
      firebaseConnected = true;
      
      if (sensorAvailable) {
        updateSensorData();
      }
      
      bool present = (digitalRead(RADAR_OUTPUT) == HIGH);
      lastPresence = present;
      
      uint32_t freeHeap = ESP.getFreeHeap();
      uint32_t totalHeap = ESP.getHeapSize();
      
      FirebaseJson json;
      FirebaseJson sensors;
      FirebaseJson stats;
      
      sensors.set("lux", currentLux);
      sensors.set("presence", present);
      
      stats.set("uptime", formatUptime(millis() - taskStartTime));
      stats.set("status", systemHealthy ? "ALL SYSTEMS NOMINAL" : "SYSTEM WARNINGS");
      stats.set("free_heap_kb", freeHeap / 1024);
      stats.set("wifi_rssi", WiFi.RSSI());
      stats.set("loop_counter", loopCounter);
      stats.set("firmware_version", currentFirmwareVersion);
      
      json.set("sensors", sensors);
      json.set("stats", stats);
      
      // NEW STRUCTURE: /local/sensor_stats
      String combinedPath = basePath + "/local/sensor_stats";
      Firebase.RTDB.setJSON(&fbdoUpload, combinedPath.c_str(), &json);
    }
    
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
      checkForGitHubUpdate();
    }
    
    vTaskDelay(xDelay);
  }
}
