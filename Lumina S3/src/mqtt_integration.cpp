// ============================================================================
// mqtt_integration.cpp - MULTI-DEVICE SUPPORT
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <FastLED.h>
#include "mqtt_integration.h"
#include "smart_home.h"
#include "config.h"
#include "globals.h"
#include <SPIFFS.h>

// Global MQTT objects
WiFiClient espClient;
PubSubClient mqttClient(espClient);
MQTTConfig mqttConfig = {0};
volatile bool mqttConnected = false;
String deviceTopic = "";

// MQTT state tracking
unsigned long lastMQTTStatePublish = 0;
const unsigned long MQTT_STATE_PUBLISH_INTERVAL = 5000;
unsigned long lastMQTTSensorPublish = 0;
const unsigned long MQTT_SENSOR_PUBLISH_INTERVAL = 2000;

// Centralized Effect List
const char* EFFECT_NAMES[] = {
    // Standard (0-21)
    "Rainbow Cycle", "Meteor Shower", "Digital Rain", "Pulsing Spheres", "Binary Clock",
    "Vortex", "DNA Helix", "Audio Visualizer", "Lava Lamp", "Radar Sweep",
    "Quantum Particles", "Neural Network", "Galaxy Spin", "Crystal Growth",
    "Lightning Storm", "Ocean Depth", "Northern Lights", "Time Tunnel",
    "Cyber City", "Solar Flare", "Fire Simulation", "Solid Color",
    // Sound Reactive (22-32)
    "Frequency Spectrum", "Reactive Waveform", "Beat Pulse", "Frequency Bloom",
    "Audio Reactive Fire", "Musical Rainbow", "Reactive Strobe", "Guitar Visualizer",
    "Cascading Frequency", "Energy Orbits", "Audio Ripples",
    // Revolutionary (33-42)
    "Plasma Waves", "Confetti Palettes", "Sinelon Dual", "BPM", "Juggle",
    "Glitter Rainbow", "Pacific", "Twinkle Fox", "Color Waves", "Perlin Move",
    // Organic Lamps (43-52)
    "Plasma Lamp", "Bioluminescence", "Deep Sea Volcano", "Magical Aurora",
    "Solar Winds", "Ethereal Mist", "Bio-Pulse", "Radioactive Glow",
    "Supernova", "Enchanted Stream",
    // Fire Simulations (53-62)
    "Blue Gas Flame", "Wildfire", "Candle Flame", "Campfire", "Plasma Fire",
    "Inferno", "Smoldering Embers", "Lava Flow", "Ember Storm", "Hearth Fire"
};
const int NUM_EFFECTS = sizeof(EFFECT_NAMES) / sizeof(EFFECT_NAMES[0]);

// ============================================================================
// HELPER: CLEAN MAC STRING (REMOVE COLONS)
// ============================================================================

String getCleanMac(String macStr) {
  String clean = macStr;
  clean.replace(":", "");
  return clean;
}

// ============================================================================
// MQTT CONNECTION
// ============================================================================

bool connectToMQTT() {
  if (!mqttConfig.enabled || strlen(mqttConfig.broker_address) == 0) {
    Serial.println("   ⚠️  MQTT not enabled or broker address not set");
    return false;
  }

  Serial.printf("   Connecting to MQTT broker: %s:%d...\n", mqttConfig.broker_address, mqttConfig.broker_port);

  mqttClient.setServer(mqttConfig.broker_address, mqttConfig.broker_port);
  mqttClient.setCallback(mqttCallback);

  if (strlen(mqttConfig.username) > 0) {
    if (mqttClient.connect(deviceID.c_str(), mqttConfig.username, mqttConfig.password)) {
      Serial.println("   ✅ MQTT connected (authenticated)");
      return true;
    }
  } else {
    if (mqttClient.connect(deviceID.c_str())) {
      Serial.println("   ✅ MQTT connected (no auth)");
      return true;
    }
  }

  Serial.printf("   ❌ MQTT connection failed, state=%d\n", mqttClient.state());
  return false;
}

// ============================================================================
// HOME ASSISTANT MQTT DISCOVERY
// ============================================================================

void publishDiscoveryForDevice(String id, String name, String baseTopic, bool isGateway) {
  String discoveryPrefix = "homeassistant";

  // COMMON DEVICE OBJECT
  DynamicJsonDocument device(256);
  JsonArray identifiers = device.createNestedArray("identifiers");
  identifiers.add(id);
  device["name"] = name;
  device["manufacturer"] = "Lumina";
  device["model"] = isGateway ? "Gateway S3" : "Mini Receiver";
  device["sw_version"] = currentFirmwareVersion;

  // LIGHT ENTITY
  {
    String configTopic = discoveryPrefix + "/light/" + id + "/config";
    DynamicJsonDocument doc(4096);

    doc["name"] = name + " Light";
    doc["unique_id"] = id + "_light";
    doc["command_topic"] = baseTopic + "/light/cmd";
    doc["state_topic"] = baseTopic + "/state";
    doc["brightness_command_topic"] = baseTopic + "/brightness/cmd";
    doc["brightness_state_topic"] = baseTopic + "/state";
    doc["brightness_scale"] = 255;
    doc["brightness_value_template"] = "{{ value_json.brightness }}";
    doc["rgb_command_topic"] = baseTopic + "/color/cmd";
    doc["rgb_state_topic"] = baseTopic + "/state";
    doc["rgb_value_template"] = "{{ value_json.color.r }},{{ value_json.color.g }},{{ value_json.color.b }}";
    
    JsonArray colorModes = doc.createNestedArray("supported_color_modes");
    colorModes.add("rgb");

    doc["device"] = device;
    doc["availability_topic"] = baseTopic + "/status";

    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
  }

  // POWER SWITCH
  {
    String configTopic = discoveryPrefix + "/switch/" + id + "_power/config";
    DynamicJsonDocument doc(1024);

    doc["name"] = name + " Power";
    doc["unique_id"] = id + "_power";
    doc["command_topic"] = baseTopic + "/light/cmd";
    doc["state_topic"] = baseTopic + "/state";
    doc["value_template"] = "{{ value_json.state }}";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:power";
    doc["device"] = device;
    doc["availability_topic"] = baseTopic + "/status";

    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
  }

  // MIRROR MODE SWITCH (Only for Mini Receivers)
  if (!isGateway) {
    String configTopic = discoveryPrefix + "/switch/" + id + "_mirror/config";
    DynamicJsonDocument doc(1024);

    doc["name"] = name + " Mirror Mode";
    doc["unique_id"] = id + "_mirror";
    doc["command_topic"] = baseTopic + "/mirror/cmd";
    doc["state_topic"] = baseTopic + "/state";
    doc["value_template"] = "{{ value_json.mirror }}";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:mirror";
    doc["device"] = device;
    doc["availability_topic"] = baseTopic + "/status";

    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
  }

  // EFFECT SELECT
  {
    String configTopic = discoveryPrefix + "/select/" + id + "_effect/config";
    DynamicJsonDocument doc(4096);

    doc["name"] = name + " Effect";
    doc["unique_id"] = id + "_effect";
    doc["command_topic"] = baseTopic + "/effect/cmd";
    doc["state_topic"] = baseTopic + "/state";
    doc["value_template"] = "{{ value_json.effect }}";
    
    JsonArray options = doc.createNestedArray("options");
    for (int i = 0; i < NUM_EFFECTS; i++) {
      options.add(EFFECT_NAMES[i]);
    }
    
    doc["icon"] = "mdi:palette-outline";
    doc["device"] = device;
    doc["availability_topic"] = baseTopic + "/status";

    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
  }

  // BRIGHTNESS NUMBER
  {
    String configTopic = discoveryPrefix + "/number/" + id + "_brightness/config";
    DynamicJsonDocument doc(1024);

    doc["name"] = name + " Brightness";
    doc["unique_id"] = id + "_brightness";
    doc["command_topic"] = baseTopic + "/brightness/cmd";
    doc["state_topic"] = baseTopic + "/state";
    doc["value_template"] = "{{ value_json.brightness }}";
    doc["min"] = 0;
    doc["max"] = 255;
    doc["step"] = 1;
    doc["icon"] = "mdi:brightness-6";
    doc["mode"] = "slider";
    doc["device"] = device;
    doc["availability_topic"] = baseTopic + "/status";

    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
  }

  // SPEED NUMBER
  {
    String configTopic = discoveryPrefix + "/number/" + id + "_speed/config";
    DynamicJsonDocument doc(1024);

    doc["name"] = name + " Speed";
    doc["unique_id"] = id + "_speed";
    doc["command_topic"] = baseTopic + "/speed/cmd";
    doc["state_topic"] = baseTopic + "/state";
    doc["value_template"] = "{{ value_json.speed }}";
    doc["min"] = 10;
    doc["max"] = 200;
    doc["step"] = 5;
    doc["unit_of_measurement"] = "ms";
    doc["icon"] = "mdi:speedometer";
    doc["mode"] = "slider";
    doc["device"] = device;
    doc["availability_topic"] = baseTopic + "/status";

    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
  }

  if (isGateway) {
    // SENSORS ONLY FOR GATEWAY
    // PRESENCE BINARY SENSOR
    {
      String configTopic = discoveryPrefix + "/binary_sensor/" + id + "_presence/config";
      DynamicJsonDocument doc(1024);
      doc["name"] = name + " Presence";
      doc["unique_id"] = id + "_presence";
      doc["state_topic"] = baseTopic + "/sensors/presence";
      doc["device_class"] = "presence";
      doc["payload_on"] = "ON";
      doc["payload_off"] = "OFF";
      doc["device"] = device;
      doc["availability_topic"] = baseTopic + "/status";
      String payload;
      serializeJson(doc, payload);
      mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
    }
    // VOLTAGE SENSOR
    {
      String configTopic = discoveryPrefix + "/sensor/" + id + "_voltage/config";
      DynamicJsonDocument doc(1024);
      doc["name"] = name + " Voltage";
      doc["unique_id"] = id + "_voltage";
      doc["state_topic"] = baseTopic + "/sensors/power";
      doc["value_template"] = "{{ value_json.voltage }}";
      doc["unit_of_measurement"] = "V";
      doc["device_class"] = "voltage";
      doc["state_class"] = "measurement";
      doc["device"] = device;
      doc["availability_topic"] = baseTopic + "/status";
      String payload;
      serializeJson(doc, payload);
      mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
    }
    // CURRENT SENSOR
    {
      String configTopic = discoveryPrefix + "/sensor/" + id + "_current/config";
      DynamicJsonDocument doc(1024);
      doc["name"] = name + " Current";
      doc["unique_id"] = id + "_current";
      doc["state_topic"] = baseTopic + "/sensors/power";
      doc["value_template"] = "{{ value_json.current }}";
      doc["unit_of_measurement"] = "mA";
      doc["device_class"] = "current";
      doc["state_class"] = "measurement";
      doc["device"] = device;
      doc["availability_topic"] = baseTopic + "/status";
      String payload;
      serializeJson(doc, payload);
      mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
    }
    // POWER SENSOR
    {
      String configTopic = discoveryPrefix + "/sensor/" + id + "_power/config";
      DynamicJsonDocument doc(1024);
      doc["name"] = name + " Power";
      doc["unique_id"] = id + "_power";
      doc["state_topic"] = baseTopic + "/sensors/power";
      doc["value_template"] = "{{ value_json.power }}";
      doc["unit_of_measurement"] = "mW";
      doc["device_class"] = "power";
      doc["state_class"] = "measurement";
      doc["device"] = device;
      doc["availability_topic"] = baseTopic + "/status";
      String payload;
      serializeJson(doc, payload);
      mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
    }
    // UPTIME SENSOR
    {
      String configTopic = discoveryPrefix + "/sensor/" + id + "_uptime/config";
      DynamicJsonDocument doc(1024);
      doc["name"] = name + " Uptime";
      doc["unique_id"] = id + "_uptime";
      doc["state_topic"] = baseTopic + "/sensors/system";
      doc["value_template"] = "{{ value_json.uptime }}";
      doc["icon"] = "mdi:timer-outline";
      doc["device"] = device;
      doc["availability_topic"] = baseTopic + "/status";
      String payload;
      serializeJson(doc, payload);
      mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
    }
    // WIFI STRENGTH SENSOR
    {
      String configTopic = discoveryPrefix + "/sensor/" + id + "_wifi/config";
      DynamicJsonDocument doc(1024);
      doc["name"] = name + " WiFi Signal";
      doc["unique_id"] = id + "_wifi";
      doc["state_topic"] = baseTopic + "/sensors/system";
      doc["value_template"] = "{{ value_json.wifi_rssi }}";
      doc["unit_of_measurement"] = "dBm";
      doc["device_class"] = "signal_strength";
      doc["state_class"] = "measurement";
      doc["device"] = device;
      doc["availability_topic"] = baseTopic + "/status";
      String payload;
      serializeJson(doc, payload);
      mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
    }
    // CPU TEMP SENSOR
    {
      String configTopic = discoveryPrefix + "/sensor/" + id + "_cpu_temp/config";
      DynamicJsonDocument doc(1024);
      doc["name"] = name + " CPU Temp";
      doc["unique_id"] = id + "_cpu_temp";
      doc["state_topic"] = baseTopic + "/sensors/system";
      doc["value_template"] = "{{ value_json.cpu_temp }}";
      doc["unit_of_measurement"] = "°C";
      doc["device_class"] = "temperature";
      doc["state_class"] = "measurement";
      doc["device"] = device;
      doc["availability_topic"] = baseTopic + "/status";
      String payload;
      serializeJson(doc, payload);
      mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
    }
    // RAM USAGE SENSOR
    {
      String configTopic = discoveryPrefix + "/sensor/" + id + "_ram/config";
      DynamicJsonDocument doc(1024);
      doc["name"] = name + " RAM Usage";
      doc["unique_id"] = id + "_ram";
      doc["state_topic"] = baseTopic + "/sensors/system";
      doc["value_template"] = "{{ value_json.ram_usage }}";
      doc["unit_of_measurement"] = "%";
      doc["icon"] = "mdi:memory";
      doc["state_class"] = "measurement";
      doc["device"] = device;
      doc["availability_topic"] = baseTopic + "/status";
      String payload;
      serializeJson(doc, payload);
      mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
    }
    // LUX SENSOR
    {
      String configTopic = discoveryPrefix + "/sensor/" + id + "_lux/config";
      DynamicJsonDocument doc(1024);
      doc["name"] = name + " Illuminance";
      doc["unique_id"] = id + "_lux";
      doc["state_topic"] = baseTopic + "/sensors/system";
      doc["value_template"] = "{{ value_json.lux }}";
      doc["unit_of_measurement"] = "lx";
      doc["device_class"] = "illuminance";
      doc["state_class"] = "measurement";
      doc["device"] = device;
      doc["availability_topic"] = baseTopic + "/status";
      String payload;
      serializeJson(doc, payload);
      mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
    }
    // VERSION SENSOR
    {
      String configTopic = discoveryPrefix + "/sensor/" + id + "_version/config";
      DynamicJsonDocument doc(1024);
      doc["name"] = name + " Firmware Version";
      doc["unique_id"] = id + "_version";
      doc["state_topic"] = baseTopic + "/sensors/system";
      doc["value_template"] = "{{ value_json.version }}";
      doc["icon"] = "mdi:chip";
      doc["device"] = device;
      doc["availability_topic"] = baseTopic + "/status";
      String payload;
      serializeJson(doc, payload);
      mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
    }
  }
}

void publishHomeAssistantDiscovery() {
  if (!mqttClient.connected()) {
    Serial.println("MQTT not connected - cannot publish discovery");
    return;
  }

  Serial.println("\n=== Publishing Home Assistant Discovery ===");
  vTaskDelay(500 / portTICK_PERIOD_MS);

  // 1. Publish for Gateway
  publishDiscoveryForDevice(deviceID, mqttConfig.device_name, deviceTopic, true);
  vTaskDelay(200 / portTICK_PERIOD_MS);

  // 2. Publish for Receivers
  for (int i = 0; i < receiverCount; i++) {
    String cleanMac = getCleanMac(receivers[i].macStr);
    String recName = "Lumina Mini " + cleanMac.substring(cleanMac.length() - 4);
    String recTopic = "homeassistant/lumina/" + cleanMac;
    publishDiscoveryForDevice(cleanMac, recName, recTopic, false);
    receivers[i].mqttDiscoveryPublished = true;
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }

  Serial.println("=== Discovery Complete ===\n");
}

// ============================================================================
// MQTT PUBLISH STATE
// ============================================================================

void publishDeviceState(String topic, bool enabled, int brightness, int effect, uint32_t color, uint32_t speed, bool mirror = false, bool isGateway = false) {
  DynamicJsonDocument stateDoc(1024);

  stateDoc["state"] = enabled ? "ON" : "OFF";
  stateDoc["brightness"] = brightness;
  stateDoc["color_mode"] = "rgb";
  
  if (effect >= 0 && effect < NUM_EFFECTS) {
    stateDoc["effect"] = EFFECT_NAMES[effect];
  } else {
    stateDoc["effect"] = EFFECT_NAMES[0];
  }

  stateDoc["color"]["r"] = (color >> 16) & 0xFF;
  stateDoc["color"]["g"] = (color >> 8) & 0xFF;
  stateDoc["color"]["b"] = color & 0xFF;
  
  stateDoc["speed"] = speed;
  
  if (!isGateway) {
    stateDoc["mirror"] = mirror ? "ON" : "OFF";
  }

  String statePayload;
  serializeJson(stateDoc, statePayload);
  mqttClient.publish((topic + "/state").c_str(), statePayload.c_str());
  mqttClient.publish((topic + "/status").c_str(), "online");
}

void mqttPublishState() {
  if (!mqttClient.connected()) return;

  unsigned long now = millis();
  if (now - lastMQTTStatePublish < MQTT_STATE_PUBLISH_INTERVAL) {
    return;
  }
  lastMQTTStatePublish = now;

  // 1. Publish Gateway State
  publishDeviceState(deviceTopic, stripEnabled, FastLED.getBrightness(), currentEffect, effectColor, effectSpeed, false, true);

  // 2. Publish Receiver States
  for (int i = 0; i < receiverCount; i++) {
    String cleanMac = getCleanMac(receivers[i].macStr);
    String recTopic = "homeassistant/lumina/" + cleanMac;
    
    if (receivers[i].isMirror) {
      publishDeviceState(recTopic, stripEnabled, FastLED.getBrightness(), currentEffect, effectColor, effectSpeed, true, false);
    } else {
      publishDeviceState(recTopic, receivers[i].enabled, receivers[i].brightness, receivers[i].effect, receivers[i].color, receivers[i].speed, false, false);
    }
  }
}

void mqttPublishSensorData() {
  if (!mqttClient.connected()) return;

  unsigned long now = millis();
  if (now - lastMQTTSensorPublish < MQTT_SENSOR_PUBLISH_INTERVAL) {
    return;
  }
  lastMQTTSensorPublish = now;

  // 1. Presence Sensor
  bool present = (digitalRead(RADAR_OUTPUT) == HIGH);
  mqttClient.publish((deviceTopic + "/sensors/presence").c_str(), present ? "ON" : "OFF");

  // 2. Power Data
  DynamicJsonDocument powerDoc(256);
  powerDoc["voltage"] = String(currentVoltage, 2);
  powerDoc["current"] = String(currentCurrent, 1);
  powerDoc["power"] = String(currentPower, 0);
  
  String powerPayload;
  serializeJson(powerDoc, powerPayload);
  mqttClient.publish((deviceTopic + "/sensors/power").c_str(), powerPayload.c_str());

  // 3. System Data
  DynamicJsonDocument sysDoc(512);
  sysDoc["uptime"] = formatUptime(millis());
  sysDoc["wifi_rssi"] = WiFi.RSSI();
  sysDoc["cpu_temp"] = String(currentCpuTemp, 1);
  sysDoc["lux"] = String(currentLux, 1);
  sysDoc["version"] = currentFirmwareVersion;
  
  float ramUsage = (1.0 - (float)ESP.getFreeHeap() / (float)ESP.getHeapSize()) * 100.0;
  sysDoc["ram_usage"] = String(ramUsage, 1);
  
  String sysPayload;
  serializeJson(sysDoc, sysPayload);
  mqttClient.publish((deviceTopic + "/sensors/system").c_str(), sysPayload.c_str());
}

// ============================================================================
// MQTT CALLBACK - HANDLE INCOMING COMMANDS
// ============================================================================

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char message[512];
  if (length < sizeof(message) - 1) {
    strncpy(message, (char*)payload, length);
    message[length] = '\0';
  } else {
    return;
  }

  String topicStr(topic);

  // Identify target device
  bool isGatewayTarget = topicStr.startsWith(deviceTopic);
  int targetReceiverIndex = -1;

  if (!isGatewayTarget) {
    for (int i = 0; i < receiverCount; i++) {
      String cleanMac = getCleanMac(receivers[i].macStr);
      if (topicStr.indexOf("/lumina/" + cleanMac + "/") != -1) {
        targetReceiverIndex = i;
        break;
      }
    }
  }

  if (!isGatewayTarget && targetReceiverIndex == -1) return;

  // 1. Light control
  if (topicStr.endsWith("/light/cmd")) {
    bool newState = (strcmp(message, "ON") == 0);
    if (isGatewayTarget) {
      portENTER_CRITICAL(&stripMux);
      stripEnabled = newState;
      portEXIT_CRITICAL(&stripMux);
      manuallyTurnedOff = !newState;
      if (!newState) { FastLED.clear(); FastLED.show(); }
      enqueueFirebaseRequest(FB_SET_BOOL, basePath + "/local/enabled", newState ? "true" : "false");
      syncAllMirrors();
    } else {
      receivers[targetReceiverIndex].enabled = newState;
      receivers[targetReceiverIndex].isMirror = false; // Breaking mirror link
      routeCommandToReceiver(targetReceiverIndex);
    }
  }
  // 2. Brightness control
  else if (topicStr.endsWith("/brightness/cmd")) {
    int brightness = atoi(message);
    brightness = constrain(brightness, 0, 255);
    if (isGatewayTarget) {
      FastLED.setBrightness(brightness);
      enqueueFirebaseRequest(FB_SET_INT, basePath + "/local/brightness", String(brightness));
      syncAllMirrors();
    } else {
      receivers[targetReceiverIndex].brightness = brightness;
      receivers[targetReceiverIndex].isMirror = false;
      routeCommandToReceiver(targetReceiverIndex);
    }
  }
  // 3. Color control
  else if (topicStr.endsWith("/color/cmd")) {
    uint8_t r = 0, g = 0, b = 0;
    bool parsed = false;
    DynamicJsonDocument doc(256);
    if (deserializeJson(doc, message) == DeserializationError::Ok) {
      if (doc.containsKey("r") && doc.containsKey("g") && doc.containsKey("b")) {
        r = doc["r"] | 0; g = doc["g"] | 0; b = doc["b"] | 0;
        parsed = true;
      }
    }
    if (!parsed) {
      String msgStr(message);
      int commaPos1 = msgStr.indexOf(',');
      int commaPos2 = msgStr.lastIndexOf(',');
      if (commaPos1 > 0 && commaPos2 > commaPos1) {
        r = atoi(msgStr.substring(0, commaPos1).c_str());
        g = atoi(msgStr.substring(commaPos1 + 1, commaPos2).c_str());
        b = atoi(msgStr.substring(commaPos2 + 1).c_str());
        parsed = true;
      }
    }
    if (parsed) {
      uint32_t color = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
      if (isGatewayTarget) {
        effectColor = color;
        char colorStr[7]; sprintf(colorStr, "%02X%02X%02X", r, g, b);
        enqueueFirebaseRequest(FB_SET_STRING, basePath + "/local/color", colorStr);
        syncAllMirrors();
      } else {
        receivers[targetReceiverIndex].color = color;
        receivers[targetReceiverIndex].isMirror = false;
        routeCommandToReceiver(targetReceiverIndex);
      }
    }
  }
  // 4. Effect selection
  else if (topicStr.endsWith("/effect/cmd")) {
    String payloadStr = String(message);
    int newEffect = -1;
    for (int i = 0; i < NUM_EFFECTS; i++) {
      if (payloadStr.equalsIgnoreCase(EFFECT_NAMES[i])) { newEffect = i; break; }
    }
    if (newEffect == -1) {
      int val = payloadStr.toInt();
      if (val >= 0 && val < NUM_EFFECTS) newEffect = val;
    }
    if (newEffect >= 0 && newEffect < NUM_EFFECTS) {
      if (isGatewayTarget) {
        currentEffect = newEffect;
        enqueueFirebaseRequest(FB_SET_INT, basePath + "/local/effect", String(currentEffect));
        syncAllMirrors();
      } else {
        receivers[targetReceiverIndex].effect = newEffect;
        receivers[targetReceiverIndex].isMirror = false;
        routeCommandToReceiver(targetReceiverIndex);
      }
    }
  }
  // 5. Speed control
  else if (topicStr.endsWith("/speed/cmd")) {
    int speed = atoi(message);
    speed = constrain(speed, 10, 200);
    if (isGatewayTarget) {
      effectSpeed = speed;
      enqueueFirebaseRequest(FB_SET_INT, basePath + "/local/speed", String(speed));
      syncAllMirrors();
    } else {
      receivers[targetReceiverIndex].speed = speed;
      receivers[targetReceiverIndex].isMirror = false;
      routeCommandToReceiver(targetReceiverIndex);
    }
  }
  // 6. Mirror Mode toggle
  else if (!isGatewayTarget && topicStr.endsWith("/mirror/cmd")) {
    bool mirrorState = (strcmp(message, "ON") == 0);
    receivers[targetReceiverIndex].isMirror = mirrorState;
    
    // Update Firebase mirror flag for this receiver
    String mirrorPath = basePath + "/active_nodes/" + receivers[targetReceiverIndex].macStr + "/isMirror";
    enqueueFirebaseRequest(FB_SET_BOOL, mirrorPath, mirrorState ? "true" : "false");
    
    routeCommandToReceiver(targetReceiverIndex);
  }

  lastMQTTStatePublish = 0; // Force immediate state publish
  mqttPublishState();
  broadcastGatewayState();
}

// ============================================================================
// MQTT SUBSCRIPTIONS
// ============================================================================

void subscribeToTopics() {
  if (!mqttClient.connected()) return;

  Serial.println("📡 [MQTT] Subscribing to Command Topics");
  
  // 1. Gateway Subscriptions
  mqttClient.subscribe((deviceTopic + "/light/cmd").c_str());
  mqttClient.subscribe((deviceTopic + "/brightness/cmd").c_str());
  mqttClient.subscribe((deviceTopic + "/color/cmd").c_str());
  mqttClient.subscribe((deviceTopic + "/effect/cmd").c_str());
  mqttClient.subscribe((deviceTopic + "/speed/cmd").c_str());
  
  // 2. Receiver Subscriptions
  for (int i = 0; i < receiverCount; i++) {
    String cleanMac = getCleanMac(receivers[i].macStr);
    String recTopic = "homeassistant/lumina/" + cleanMac;
    mqttClient.subscribe((recTopic + "/light/cmd").c_str());
    mqttClient.subscribe((recTopic + "/brightness/cmd").c_str());
    mqttClient.subscribe((recTopic + "/color/cmd").c_str());
    mqttClient.subscribe((recTopic + "/effect/cmd").c_str());
    mqttClient.subscribe((recTopic + "/speed/cmd").c_str());
    mqttClient.subscribe((recTopic + "/mirror/cmd").c_str());
  }
}

// ============================================================================
// MQTT CONNECTION
// ============================================================================

void mqttConnectAndSubscribe() {
  if (!mqttConfig.enabled) return;

  if (!mqttClient.connected()) {
    unsigned long now = millis();
    static unsigned long lastAttempt = 0;
    if (now - lastAttempt > 5000) {
      lastAttempt = now;
      if (connectToMQTT()) {
        mqttConnected = true;
        subscribeToTopics();
        publishHomeAssistantDiscovery();
        mqttPublishState();
      } else {
        mqttConnected = false;
      }
    }
  } else {
    if (!mqttConnected) {
      mqttConnected = true;
      subscribeToTopics();
      publishHomeAssistantDiscovery();
      mqttPublishState();
    }
    mqttClient.loop();
  }
}

void setupMQTT() {
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  mqttClient.setBufferSize(2048);
  deviceTopic = String("homeassistant/lumina/") + deviceID;

  if (mqttConfig.enabled && WiFi.status() == WL_CONNECTED) {
    if (connectToMQTT()) {
      mqttConnected = true;
      subscribeToTopics(); // Fix: Actually subscribe so commands work
      delay(500);
      publishHomeAssistantDiscovery();
      mqttPublishState();
    }
  }
}

// ============================================================================
// IO TASK FOR FREERTOS
// ============================================================================

void ioTask(void *parameter) {
  Serial.println("🌐 IO Task (MQTT+SmartHome) started on Core " + String(xPortGetCoreID()));
  for (;;) {
    esp_task_wdt_reset();
    if (WiFi.status() == WL_CONNECTED && firebaseConnected) {
      mqttConnectAndSubscribe();
      mqttPublishState();
      mqttPublishSensorData();

      // Check if new receivers need discovery
      for (int i = 0; i < receiverCount; i++) {
        if (!receivers[i].mqttDiscoveryPublished && mqttClient.connected()) {
          String cleanMac = getCleanMac(receivers[i].macStr);
          String recName = "Lumina Mini " + cleanMac.substring(cleanMac.length() - 4);
          String recTopic = "homeassistant/lumina/" + cleanMac;
          publishDiscoveryForDevice(cleanMac, recName, recTopic, false);
          
          // Subscribe to NEW receiver commands
          mqttClient.subscribe((recTopic + "/light/cmd").c_str());
          mqttClient.subscribe((recTopic + "/brightness/cmd").c_str());
          mqttClient.subscribe((recTopic + "/color/cmd").c_str());
          mqttClient.subscribe((recTopic + "/effect/cmd").c_str());
          mqttClient.subscribe((recTopic + "/speed/cmd").c_str());
          mqttClient.subscribe((recTopic + "/mirror/cmd").c_str());

          receivers[i].mqttDiscoveryPublished = true;
          Serial.printf("📡 [MQTT] Discovery published for new receiver: %s\n", receivers[i].macStr.c_str());
        }
      }

      handleSmartHome();
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}