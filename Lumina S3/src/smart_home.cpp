#include "smart_home.h"
#include <SinricPro.h>
#include <SinricProLight.h>
#include "globals.h"
#include <FastLED.h>

// Your SinricPro Credentials - placeholders for user
#define APP_KEY    "8c8a8770-8da2-4b37-921b-647573117fc5"
#define APP_SECRET "37c469aa-2ff3-4483-af84-6ceab5e2f29f-30508049-c51e-48b3-bcdb-24e0daa399f5"
#define LIGHT_ID   "69938bb7decdf0b6f1803148"

SinricProLight &lumina = SinricPro[LIGHT_ID];

bool onPowerState(const String &deviceId, bool &state) {
  Serial.printf("🏠 [SinricPro] Power → %s\n", state ? "ON" : "OFF");
  portENTER_CRITICAL(&stripMux);
  stripEnabled = state;
  manuallyTurnedOff = !state;
  portEXIT_CRITICAL(&stripMux);
  syncAllMirrors();
  return true;
}

bool onBrightness(const String &deviceId, int &brightness) {
  uint8_t br = (brightness * 255) / 100;
  Serial.printf("🏠 [SinricPro] Brightness → %d%% (raw: %d)\n", brightness, br);
  FastLED.setBrightness(br);
  syncAllMirrors();
  return true;
}

bool onColor(const String &deviceId, byte r, byte g, byte b) {
  Serial.printf("🏠 [SinricPro] Color → #%02X%02X%02X (R:%d G:%d B:%d)\n", r, g, b, r, g, b);
  portENTER_CRITICAL(&stripMux);
  effectColor = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
  portEXIT_CRITICAL(&stripMux);
  syncAllMirrors();
  return true;
}

void setupSmartHome() {
  lumina.onPowerState(onPowerState);
  lumina.onBrightness(onBrightness);
  lumina.onColor(onColor);

  SinricPro.begin(APP_KEY, APP_SECRET);
  
  Serial.println("✅ SinricPro (Cloud) initialized");
}

void handleSmartHome() {
  SinricPro.handle();
}

void smartHomeTask(void *pvParameters) {
  Serial.println("🏠 Smart Home Task started");
  while (true) {
    handleSmartHome();
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}