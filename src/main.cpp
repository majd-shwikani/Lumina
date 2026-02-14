#include "globals.h"
#include "web_config_portal.h"

// ============================================================================
// SETUP FUNCTION
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n");
  Serial.println("╔═══════════════════════════════════════════════════════════════╗");
  Serial.println("║                LUMINA mini RECEIVER                           ║");
  Serial.println("║                     Firmware v" + String(currentFirmwareVersion) + "                   ║");
  Serial.println("╚═══════════════════════════════════════════════════════════════╝");
  Serial.println();
  
  Serial.println("📁 Initializing SPIFFS...");
  initSPIFFS();
  
  if (shouldStartConfigPortal()) {
    Serial.println("⚙️  No configuration found. Starting config portal...");
    startConfigPortal();
    return;
  }
  
  Serial.println("📖 Loading configuration...");
  if (!loadConfig()) {
    Serial.println("❌ Failed to load config, starting portal...");
    startConfigPortal();
    return;
  }
  
  Serial.println("\n╔═══════════════════════════════════════════════════════════════╗");
  Serial.println("║                  DEVICE CONFIGURATION                          ║");
  Serial.println("╠═══════════════════════════════════════════════════════════════╣");
  Serial.printf("║   Device ID: %-48s ║\n", deviceID.c_str());
  Serial.printf("║   LED Count: %-47d ║\n", ledCount);
  Serial.printf("║   WiFi SSID: %-47s ║\n", wifiSSID.c_str());
  Serial.println("╚═══════════════════════════════════════════════════════════════╝\n");

  Serial.println("💡 Initializing LED strip...");
  leds = new CRGB[ledCount > 0 ? ledCount : 1];
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, ledCount > 0 ? ledCount : 1);
  FastLED.setBrightness(255);
  FastLED.show();
  Serial.printf("   ✅ %d LEDs initialized\n", ledCount);
  
  Serial.println("⏱️  Initializing watchdog timer (30s)...");
  esp_task_wdt_init(30, true);
  
  Serial.println("\n🚀 INITIALIZING SYSTEMS:\n");
  
  // Try to connect to WiFi for background OTA tasks if configured
  connectToWiFi();

  Serial.println("📡 Initializing ESP-NOW...");
  setupEspNow();
  
  Serial.println("⚙️  Creating FreeRTOS tasks...");
  
  xTaskCreatePinnedToCore(ledTask, "LEDTask", 4000, NULL, 1, &ledTaskHandle, 1);
  Serial.println("      ✅ LED task created (Core 1)");

  xTaskCreatePinnedToCore(discoveryTask, "DiscoveryTask", 4000, NULL, 1, &discoveryTaskHandle, 0);
  Serial.println("      ✅ Discovery task created (Core 0)");

  xTaskCreatePinnedToCore(otaUpdateTask, "OTAUpdateTask", 8000, NULL, 0, &otaTaskHandle, 0);
  Serial.println("      ✅ OTA Update task created (Core 0)");
  
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

  vTaskDelay(100 / portTICK_PERIOD_MS);
}
