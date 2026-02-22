#include "globals.h"
#include "smart_home.h"
#include "voice_recognition.h"
#include "ota_manager.h"
#include "mqtt_integration.h"
#include "sensors.h"

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

  Serial.println("🧠 Initializing PSRAM Circular Log (1MB)...");
  circularLog = (char*)ps_malloc(CIRCULAR_LOG_SIZE);
  if (circularLog) {
    memset(circularLog, 0, CIRCULAR_LOG_SIZE);
    logWriteIdx = 0;
    logToPSRAM("--- Lumina Boot: %s ---", currentFirmwareVersion);
    Serial.println("      ✅ PSRAM Log initialized");
  } else {
    Serial.println("      ❌ PSRAM Log allocation failed!");
  }
  
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
  
  leds = (CRGB*)ps_malloc(ledCount * sizeof(CRGB));
  if (leds) {
    memset(leds, 0, ledCount * sizeof(CRGB));
    Serial.printf("      ✅ %d LEDs allocated in PSRAM\n", ledCount);
    logToPSRAM("LED Framebuffer initialized in PSRAM: %d LEDs", ledCount);
  } else {
    Serial.println("      ❌ PSRAM LED allocation failed! Falling back to SRAM...");
    leds = new CRGB[ledCount];
  }

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, ledCount);
  FastLED.addLeds<WS2812B, ONBOARD_LED_PIN, GRB>(onboardLed, 1);
  FastLED.setBrightness(255);
  FastLED.show();
  Serial.printf("   ✅ %d LEDs + onboard initialized\n", ledCount);
  
  xTaskCreatePinnedToCore(statusLedTask, "StatusLEDTask", 2048, NULL, 1, NULL, 0);
  
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
  
  Serial.println("🎤 [8.5/9] Setting up smart home and voice detection...");
  setupSmartHome();
  setupVoiceRecognition();

  Serial.println("⚙️  [9/9] Creating FreeRTOS tasks...");
  
  // PROTECTED TASKS
  xTaskCreatePinnedToCore(firebaseTask, "FirebaseTask", 12000, NULL, 1, &firebaseTaskHandle, 0);
  Serial.println("      ✅ Firebase task created (Core 0, 12KB stack)");

  xTaskCreatePinnedToCore(ledTask, "LEDTask", 4000, NULL, 1, &ledTaskHandle, 1);
  Serial.println("      ✅ LED task created (Core 1, 4KB stack)");

  xTaskCreatePinnedToCore(voiceRecognitionTask, "VoiceTask", 10000, NULL, 1, &voiceTaskHandle, 0);
  Serial.println("      ✅ Voice Detection task created (Core 0, 10KB stack)");

  Serial.println("      ℹ️  Lightweight tasks consolidated into Main Loop Dispatcher");
  
  systemInitialized = true;
  
  Serial.println("\n╔═══════════════════════════════════════════════════════════════╗");
  Serial.println("║              🎉 ALL SYSTEMS INITIALIZED 🎉                     ║");
  Serial.println("║       Consolidated dispatcher loop active every 10ms           ║");
  Serial.println("╚═══════════════════════════════════════════════════════════════╝\n");
}

// ============================================================================
// MAIN LOOP - CLEAN DISPATCHER PATTERN
// ============================================================================

void loop() {
  unsigned long loopStart = millis();
  loopCounter++;
  
  // 1. Button Handling
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (!buttonActive) {
      buttonActive = true;
      buttonPressStart = millis();
      Serial.println("\n🔘 Button pressed");
    }
  } else {
    buttonActive = false;
  }
  
  // 2. Dispatcher Timers
  static unsigned long lastSensorRun = 0;
  static unsigned long lastTimerRun = 0;
  
  // 3. FAST DISPATCH (Every Loop)
  handleSmartHome();
  handleMQTT();
  
  // 4. MEDIUM DISPATCH (Every 100ms)
  if (millis() - lastSensorRun > 100) {
    lastSensorRun = millis();
    handleSensorsAndAutomation();
  }
  
  // 5. SLOW DISPATCH (Every 1000ms)
  if (millis() - lastTimerRun > 1000) {
    lastTimerRun = millis();
    handleTimers();
  }
  
  // 6. VERY SLOW DISPATCH (OTA Check)
  handleOTAUpdate();
  
  // 7. Gateway Broadcast (every 5s)
  static unsigned long lastBroadcastTime = 0;
  if (millis() - lastBroadcastTime > 5000) {
    lastBroadcastTime = millis();
    broadcastGatewayState();
  }
  
  // 8. System Stats Reporting
  if (millis() - lastSystemStatsReport > SYSTEM_STATS_INTERVAL) {
    lastSystemStatsReport = millis();
    
    if (firebaseTaskHandle != NULL) firebaseTaskStack = uxTaskGetStackHighWaterMark(firebaseTaskHandle);
    if (ledTaskHandle != NULL) ledTaskStack = uxTaskGetStackHighWaterMark(ledTaskHandle);
    if (voiceTaskHandle != NULL) voiceTaskStack = uxTaskGetStackHighWaterMark(voiceTaskHandle);
    
    printSystemStats();
  }

  // 9. Watchdog & Loop Timing
  lastLoopTime = millis();
  unsigned long loopDuration = lastLoopTime - loopStart;
  
  if (loopDuration > 1000) {
    Serial.printf("⚠️  WARNING: Loop took %lu ms (expected <100ms)\n", loopDuration);
  }
  
  vTaskDelay(10 / portTICK_PERIOD_MS); 
}
