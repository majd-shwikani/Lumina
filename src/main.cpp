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
  
  Serial.println("📁 Initializing SPIFFS...");
  initSPIFFS();
  
  Serial.println("📖 Loading configuration...");
  if (!loadConfig()) {
    Serial.println("❌ No configuration found or failed to load. Please check SPIFFS.");
    // In a real scenario, we might want to start an AP here if we had the code,
    // but the user asked to remove the webserver/portal.
  }
  
  Serial.println("💡 Initializing LED strip...");
  leds = new CRGB[ledCount > 0 ? ledCount : 1];
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, ledCount > 0 ? ledCount : 1);
  FastLED.setBrightness(255);
  FastLED.show();
  
  Serial.println("⏱️  Initializing watchdog timer (30s)...");
  esp_task_wdt_init(30, true);
  
  Serial.println("\n🚀 INITIALIZING SYSTEMS:\n");
  
  Serial.println("📡 Connecting to WiFi...");
  connectToWiFi();
  
  Serial.println("🕐 Synchronizing time...");
  setupTime();
  
  Serial.println("🔄 Enabling OTA updates...");
  setupOTA();
  
  Serial.println("⚙️  Creating FreeRTOS tasks...");
  
  xTaskCreatePinnedToCore(ledTask, "LEDTask", 4000, NULL, 1, &ledTaskHandle, 1);
  Serial.println("      ✅ LED task created (Core 1)");

  xTaskCreatePinnedToCore(otaUpdateTask, "OTATask", 4000, NULL, 1, &otaTaskHandle, 0);
  Serial.println("      ✅ OTA task created (Core 0)");
  
  Serial.println("\n🎉 ALL SYSTEMS INITIALIZED 🎉\n");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  loopCounter++;
  
  if (millis() - lastSystemStatsReport > SYSTEM_STATS_INTERVAL) {
    lastSystemStatsReport = millis();
    printSystemStats();
  }

  vTaskDelay(1000 / portTICK_PERIOD_MS);
}
