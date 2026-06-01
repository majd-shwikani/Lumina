#include "smart_home.h"
#include <SinricPro.h>
#include <SinricProLight.h>
#include "globals.h"
#include <FastLED.h>

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
  globalBrightness = br; flag_forceBrightnessUpdate = true;
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
  if (sinricAppKey.length() == 0 || sinricAppSecret.length() == 0 || sinricLightID.length() == 0) {
    Serial.println("⚠️ SinricPro credentials not set, skipping initialization");
    return;
  }

  SinricProLight &lumina = SinricPro[sinricLightID];
  lumina.onPowerState(onPowerState);
  lumina.onBrightness(onBrightness);
  lumina.onColor(onColor);

  SinricPro.begin(sinricAppKey.c_str(), sinricAppSecret.c_str());
  
  Serial.println("✅ SinricPro (Cloud) initialized");
}

void handleSmartHome() {
  if (sinricAppKey.length() > 0) {
    SinricPro.handle();
  }
}
