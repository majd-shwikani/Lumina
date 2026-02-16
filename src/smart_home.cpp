#include "smart_home.h"
#include <SinricPro.h>
#include <SinricProLight.h>
#define ESPALEXA_ASYNC
#include <Espalexa.h>
#include "globals.h"
#include <FastLED.h>

// Your SinricPro Credentials - placeholders for user
#define APP_KEY    "YOUR_APP_KEY"
#define APP_SECRET "YOUR_APP_SECRET"
#define LIGHT_ID   "YOUR_LIGHT_ID"

SinricProLight &lumina = SinricPro[LIGHT_ID];
Espalexa espalexa;

// Callback for Espalexa
void onEspalexaCommand(uint8_t brightness) {
  Serial.printf("Espalexa: Brightness changed to %d\n", brightness);
  if (brightness == 0) {
    portENTER_CRITICAL(&stripMux);
    stripEnabled = false;
    manuallyTurnedOff = true;
    portEXIT_CRITICAL(&stripMux);
  } else {
    portENTER_CRITICAL(&stripMux);
    stripEnabled = true;
    manuallyTurnedOff = false;
    portEXIT_CRITICAL(&stripMux);
    FastLED.setBrightness(brightness);
  }
  syncAllMirrors();
}

bool onPowerState(const String &deviceId, bool &state) {
  Serial.printf("Smart Home: Device %s turned %s\n", deviceId.c_str(), state ? "on" : "off");
  portENTER_CRITICAL(&stripMux);
  stripEnabled = state;
  manuallyTurnedOff = !state;
  portEXIT_CRITICAL(&stripMux);
  syncAllMirrors();
  return true;
}

bool onBrightness(const String &deviceId, int &brightness) {
  Serial.printf("Smart Home: Device %s brightness changed to %d\n", deviceId.c_str(), brightness);
  uint8_t br = (brightness * 255) / 100;
  FastLED.setBrightness(br);
  syncAllMirrors();
  return true;
}

bool onColor(const String &deviceId, byte r, byte g, byte b) {
  Serial.printf("Smart Home: Device %s color changed to R:%d G:%d B:%d\n", deviceId.c_str(), r, g, b);
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
  
  espalexa.addDevice("Lumina", onEspalexaCommand);
  espalexa.begin();

  Serial.println("✅ SinricPro (Cloud) and Espalexa (Local) initialized");
}

void handleSmartHome() {
  SinricPro.handle();
  espalexa.loop();
}

void smartHomeTask(void *pvParameters) {
  Serial.println("🏠 Smart Home Task started");
  while (true) {
    handleSmartHome();
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}
