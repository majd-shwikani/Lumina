#include "globals.h"

// ============================================================================
// SETUP FUNCTION
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n");
  Serial.println("╔═══════════════════════════════════════════════════════════════╗");
  Serial.println("║                    LUMINA LED CONTROLLER                       ║");
  Serial.println("║                     Firmware v" + String(currentFirmwareVersion) + "                          ║");
  Serial.println("╚═══════════════════════════════════════════════════════════════╝");
  Serial.println();
  
  Serial.println("🔄 BOOT INFORMATION:");
  Serial.printf("   CPU0 Reset Reason: %s\n", getResetReason(0));
  Serial.printf("   CPU1 Reset Reason: %s\n", getResetReason(1));
  Serial.println();
  
  Serial.println("📁 Initializing SPIFFS...");
  initSPIFFS();
  
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.println("🔘 Button configured on pin " + String(BUTTON_PIN));

  if (shouldStartConfigPortal()) {
    Serial.println("⚙️  No configuration found. Starting config portal...");
    startConfigPortal();
    return;
  }
  
  Serial.println("📖 Loading configuration...");
  if (!loadConfig()) {
    Serial.println("❌ Failed to load config, restarting...");
    delay(3000);
    ESP.restart();
    return;
  }
  
  Serial.println("\n╔═══════════════════════════════════════════════════════════════╗");
  Serial.println("║                  DEVICE CONFIGURATION                          ║");
  Serial.println("╠═══════════════════════════════════════════════════════════════╣");
  Serial.printf("║   Device ID: %-48s ║\n", deviceID.c_str());
  Serial.printf("║   Base Path: %-47s ║\n", basePath.c_str());
  Serial.printf("║   LED Count: %-47d ║\n", ledCount);
  Serial.printf("║   WiFi SSID: %-47s ║\n", wifiSSID.c_str());
  Serial.println("╚═══════════════════════════════════════════════════════════════╝\n");
  
  Serial.println("💡 Initializing LED strip...");
  leds = new CRGB[ledCount];
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, ledCount);
  FastLED.setBrightness(255);
  FastLED.show();
  Serial.printf("   ✅ %d LEDs initialized\n", ledCount);
  
  Serial.println("⏱️  Initializing watchdog timer (30s)...");
  esp_task_wdt_init(30, true);
  Serial.println("   ✅ Watchdog configured");
  
  Serial.println("🔌 Initializing I2C bus...");
  Wire.begin();
  Serial.println("   ✅ I2C ready");
  
  Serial.println("\n🚀 INITIALIZING SYSTEMS:\n");
  
  Serial.println("📡 [1/9] Connecting to WiFi...");
  connectToWiFi();
  Serial.println("      ✅ WiFi initialized");
  
  Serial.println("📡 [1.5/9] Initializing ESP-NOW Gateway...");
  setupEspNowGateway();
  
  Serial.println("🕐 [2/9] Synchronizing time...");
  setupTime();
  Serial.println("      ✅ Time synchronized");
  
  Serial.println("🔄 [3/9] Enabling OTA updates...");
  setupOTA();
  Serial.println("      ✅ OTA enabled");
  
  Serial.println("☀️  [4/9] Setting up light sensor...");
  setupVEML7700();
  if (sensorAvailable) {
    Serial.println("      ✅ Light sensor initialized");
  } else {
    Serial.println("      ⚠️  Light sensor not found");
  }
  
  Serial.println("🔥 [5/9] Connecting to Firebase...");
  setupFirebase();
  Serial.println("      ✅ Firebase initialized");
  
  Serial.println("🎤 [6/9] Initializing audio processing...");
  setupFrequencyDetection();
  Serial.println("      ✅ Frequency detection initialized");
  Serial.println("      🎵 Starting microphone calibration...");
  
  Serial.println("📨 [7/9] Setting up MQTT...");
  setupMQTT();
  if (mqttConnected) {
    Serial.println("      ✅ MQTT initialized");
  } else {
    Serial.println("      ⚠️  MQTT not connected");
  }
  
  Serial.println("📡 [8/9] Initializing radar sensor...");
  pinMode(RADAR_OUTPUT, INPUT);
  Serial.println("      ✅ LD2410 radar presence detection on GPIO " + String(RADAR_OUTPUT));
  
  Serial.println("⚙️  [9/9] Creating FreeRTOS tasks...");
  
  xTaskCreatePinnedToCore(firebaseTask, "FirebaseTask", 4000, NULL, 1, &firebaseTaskHandle, 0);
  Serial.println("      ✅ Firebase task created (Core 0, 4KB stack)");

  xTaskCreatePinnedToCore(ledTask, "LEDTask", 4000, NULL, 1, &ledTaskHandle, 1);
  Serial.println("      ✅ LED task created (Core 1, 4KB stack)");

  xTaskCreatePinnedToCore(sensorDataTask, "SensorDataTask", 12000, NULL, 1, &sensorTaskHandle, 0);
  Serial.println("      ✅ Sensor task created (Core 0, 12KB stack)");

  xTaskCreatePinnedToCore(automationtask, "AutomationTask", 4000, NULL, 0, &automationTaskHandle, 0);
  Serial.println("      ✅ Automation task created (Core 0, 4KB stack)");

  xTaskCreatePinnedToCore(timerTask, "TimerTask", 4000, NULL, 1, &timerTaskHandle, 0);
  Serial.println("      ✅ Timer task created (Core 0, 4KB stack)");

  xTaskCreatePinnedToCore(mqttTask, "MQTTTask", 8000, NULL, 1, &mqttTaskHandle, 0);
  Serial.println("      ✅ MQTT task created (Core 0, 4KB stack)");

  xTaskCreatePinnedToCore(otaUpdateTask, "OTAUpdateTask", 7000, NULL, 0, &otaTaskHandle, 0);
  Serial.println("      ✅ OTA Update task created (Core 0, 7KB stack)");
  
  Serial.println("\n╔═══════════════════════════════════════════════════════════════╗");
  Serial.println("║              🎉 ALL SYSTEMS INITIALIZED 🎉                     ║");
  Serial.println("║       Combined sensor+stats data sent every 2 seconds          ║");
  Serial.println("╚═══════════════════════════════════════════════════════════════╝\n");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  unsigned long loopStart = millis();
  loopCounter++;
  
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (!buttonActive) {
      buttonActive = true;
      buttonPressStart = millis();
      Serial.println("\n🔘 Button pressed - hold for 7 seconds to reset config");
    }
    
    if (millis() - buttonPressStart > 7000) {
      Serial.println("⚠️  7-second button press detected - resetting configuration...");
      
      if (SPIFFS.exists("/config.json")) {
        SPIFFS.remove("/config.json");
        Serial.println("✅ Configuration file deleted");
      }
      
      Serial.println("🔄 Restarting to enter config portal...");
      delay(1000);
      ESP.restart();
    }
  } else {
    buttonActive = false;
  }
  
  static unsigned long lastBroadcastTime = 0;
  if (millis() - lastBroadcastTime > 5000) {
    lastBroadcastTime = millis();
    broadcastGatewayState();
  }
  
  if (millis() - lastSystemStatsReport > SYSTEM_STATS_INTERVAL) {
    lastSystemStatsReport = millis();
    
    if (firebaseTaskHandle != NULL) {
      firebaseTaskStack = uxTaskGetStackHighWaterMark(firebaseTaskHandle);
    }
    if (ledTaskHandle != NULL) {
      ledTaskStack = uxTaskGetStackHighWaterMark(ledTaskHandle);
    }
    if (automationTaskHandle != NULL) {
      automationTaskStack = uxTaskGetStackHighWaterMark(automationTaskHandle);
    }
    if (sensorTaskHandle != NULL) {
      sensorTaskStack = uxTaskGetStackHighWaterMark(sensorTaskHandle);
    }
    if (timerTaskHandle != NULL) {
      timerTaskStack = uxTaskGetStackHighWaterMark(timerTaskHandle);
    }
    if (mqttTaskHandle != NULL) {
      mqttTaskStack = uxTaskGetStackHighWaterMark(mqttTaskHandle);
    }
    if (otaTaskHandle != NULL) {
      otaTaskStack = uxTaskGetStackHighWaterMark(otaTaskHandle);
    }
    
    printSystemStats();
  }

  lastLoopTime = millis();
  unsigned long loopDuration = lastLoopTime - loopStart;
  
  if (loopDuration > 1000) {
    Serial.printf("⚠️  WARNING: Loop took %lu ms (expected <100ms)\n", loopDuration);
  }
  
  vTaskDelay(100 / portTICK_PERIOD_MS);
}
