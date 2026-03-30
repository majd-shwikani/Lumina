#include "globals.h"
#include "smart_home.h"

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
  checkBootCount();
  
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.println("🔘 Button configured on pin " + String(BUTTON_PIN));

  if (shouldStartConfigPortal()) {
    configPortalActive = true;
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
  i2sMutex = xSemaphoreCreateMutex();
  leds = new CRGB[ledCount];
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, ledCount);
  FastLED.addLeds<WS2812B, ONBOARD_LED_PIN, GRB>(onboardLed, 1);
  FastLED.setBrightness(255);
  FastLED.show();
  Serial.printf("   ✅ %d LEDs + onboard initialized\n", ledCount);
  
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
  
  Serial.println("☀️  [4/9] Setting up light and power sensors...");
  setupVEML7700();
  if (sensorAvailable) {
    Serial.println("      ✅ Light sensor initialized");
  } else {
    Serial.println("      ⚠️  Light sensor not found");
  }

  setupINA219();
  if (ina219Available) {
    Serial.println("      ✅ INA219 power monitor initialized");
  } else {
    Serial.println("      ⚠️  INA219 not found");
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
  
  Serial.println("🎤 [8.5/9] Setting up smart home...");
  setupSmartHome();

  Serial.println("⚙️  [9/9] Creating Consolidated FreeRTOS tasks...");
  
  // Cloud Sync Task: Firebase + Sensors (16KB stack)
  xTaskCreatePinnedToCore(cloudTask, "CloudTask", 8000, NULL, 1, &cloudTaskHandle, 0);
  Serial.println("      ✅ Cloud Task created (Core 0, 12KB stack)");

  // LED Animation Task: Keep separate for smoothness (4KB stack)
  xTaskCreatePinnedToCore(ledTask, "LEDTask", 4000, NULL, 1, &ledTaskHandle, 1);
  Serial.println("      ✅ LED Task created (Core 1, 4KB stack)");

  // IO Task: MQTT + SmartHome (Alexa/Sinric) (12KB stack)
  xTaskCreatePinnedToCore(ioTask, "IOTask", 8000, NULL, 1, &ioTaskHandle, 0);
  Serial.println("      ✅ IO Task created (Core 0, 12KB stack)");
  // Start System Task early to provide visual feedback (Status LED) during boot
  xTaskCreatePinnedToCore(systemTask, "SystemTask", 4000, NULL, 0, &systemTaskHandle, 0);
  Serial.println("⚙️  System Task started early for boot monitoring");

  
  systemInitialized = true;
  
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
      Serial.println("\n🔘 Button pressed");
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
    
    if (cloudTaskHandle != NULL) {
      cloudTaskStack = uxTaskGetStackHighWaterMark(cloudTaskHandle);
    }
    if (ledTaskHandle != NULL) {
      ledTaskStack = uxTaskGetStackHighWaterMark(ledTaskHandle);
    }
    if (ioTaskHandle != NULL) {
      ioTaskStack = uxTaskGetStackHighWaterMark(ioTaskHandle);
    }
    if (systemTaskHandle != NULL) {
      systemTaskStack = uxTaskGetStackHighWaterMark(systemTaskHandle);
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
