// ============================================================================
// mqtt_integration.cpp - FIXED VERSION (COMPLETE)
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
// LOAD/SAVE MQTT CONFIG FROM SPIFFS
// ============================================================================

bool loadMQTTConfig() {
  File configFile = SPIFFS.open("/mqtt_config.json", "r");
  if (!configFile) {
    Serial.println("No MQTT config file found");
    return false;
  }

  size_t size = configFile.size();
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  configFile.close();

  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, buf.get());
  if (error) {
    Serial.println("Failed to parse MQTT config");
    return false;
  }

  strlcpy(mqttConfig.broker_address, doc["broker_address"] | "", sizeof(mqttConfig.broker_address));
  mqttConfig.broker_port = doc["broker_port"] | 1883;
  strlcpy(mqttConfig.username, doc["username"] | "", sizeof(mqttConfig.username));
  strlcpy(mqttConfig.password, doc["password"] | "", sizeof(mqttConfig.password));
  mqttConfig.enabled = doc["enabled"] | false;
  strlcpy(mqttConfig.device_name, doc["device_name"] | "Lumina", sizeof(mqttConfig.device_name));

  Serial.printf("   ✅ MQTT config loaded from SPIFFS (broker: %s)\n", mqttConfig.broker_address);
  return true;
}

void saveMQTTConfig() {
  DynamicJsonDocument doc(1024);
  doc["broker_address"] = mqttConfig.broker_address;
  doc["broker_port"] = mqttConfig.broker_port;
  doc["username"] = mqttConfig.username;
  doc["password"] = mqttConfig.password;
  doc["enabled"] = mqttConfig.enabled;
  doc["device_name"] = mqttConfig.device_name;

  File configFile = SPIFFS.open("/mqtt_config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open MQTT config file for writing");
    return;
  }

  serializeJson(doc, configFile);
  configFile.close();
  Serial.println("   ✅ MQTT config saved to SPIFFS");
}

// ============================================================================
// UPDATE MQTT CONFIG FROM FIREBASE
// ============================================================================

void updateMQTTConfigFromFirebase() {
  String mqttBasePath = basePath + "/local/mqtt";

  String brokerPath = mqttBasePath + "/broker_address";
  if (Firebase.RTDB.getString(&fbdoSender, brokerPath.c_str())) {
    String brokerStr = fbdoSender.stringData();
    strlcpy(mqttConfig.broker_address, brokerStr.c_str(), sizeof(mqttConfig.broker_address));
  }

  String portPath = mqttBasePath + "/broker_port";
  if (Firebase.RTDB.getInt(&fbdoSender, portPath.c_str())) {
    mqttConfig.broker_port = fbdoSender.intData();
  } else {
    mqttConfig.broker_port = 1883;
  }

  String usernamePath = mqttBasePath + "/username";
  if (Firebase.RTDB.getString(&fbdoSender, usernamePath.c_str())) {
    strlcpy(mqttConfig.username, fbdoSender.stringData().c_str(), sizeof(mqttConfig.username));
  }

  String passwordPath = mqttBasePath + "/password";
  if (Firebase.RTDB.getString(&fbdoSender, passwordPath.c_str())) {
    strlcpy(mqttConfig.password, fbdoSender.stringData().c_str(), sizeof(mqttConfig.password));
  }

  String enabledPath = mqttBasePath + "/enabled";
  if (Firebase.RTDB.getBool(&fbdoSender, enabledPath.c_str())) {
    mqttConfig.enabled = fbdoSender.boolData();
  } else {
    mqttConfig.enabled = false;
  }

  String deviceNamePath = mqttBasePath + "/device_name";
  if (Firebase.RTDB.getString(&fbdoSender, deviceNamePath.c_str())) {
    strlcpy(mqttConfig.device_name, fbdoSender.stringData().c_str(), sizeof(mqttConfig.device_name));
  } else {
    strlcpy(mqttConfig.device_name, "Lumina", sizeof(mqttConfig.device_name));
  }

  saveMQTTConfig();
  Serial.printf("   ✅ MQTT config synced (broker: %s:%d, enabled: %s)\n",
                mqttConfig.broker_address, mqttConfig.broker_port,
                mqttConfig.enabled ? "yes" : "no");
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

void setupMQTT() {
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  
  loadMQTTConfig();
  mqttClient.setBufferSize(2048);

  if (firebaseConnected) {
    updateMQTTConfigFromFirebase();
  }

  deviceTopic = String("homeassistant/lumina/") + deviceID;
  Serial.printf("MQTT Device Topic: %s\n", deviceTopic.c_str());

  if (mqttConfig.enabled && firebaseConnected) {
    if (connectToMQTT()) {
      mqttConnected = true;
      delay(500);
      publishHomeAssistantDiscovery();
      mqttPublishState();
    } else {
      Serial.println("MQTT disabled or connection failed");
    }
  } else {
    Serial.println("MQTT not enabled in Firebase");
  }
}

void mqttConnectAndSubscribe() {
  if (!mqttConfig.enabled) {
    return;
  }

  if (!mqttClient.connected()) {
    unsigned long now = millis();
    static unsigned long lastAttempt = 0;

    if (now - lastAttempt > 5000) {
      lastAttempt = now;
      if (connectToMQTT()) {
        mqttConnected = true;
        Serial.println("📡 [MQTT] Reconnected → subscribing to topics");
        
        mqttClient.subscribe((deviceTopic + "/light/cmd").c_str());
        mqttClient.subscribe((deviceTopic + "/brightness/cmd").c_str());
        mqttClient.subscribe((deviceTopic + "/color/cmd").c_str());
        mqttClient.subscribe((deviceTopic + "/effect/cmd").c_str());
        mqttClient.subscribe((deviceTopic + "/speed/cmd").c_str());

        publishHomeAssistantDiscovery();
        mqttPublishState();
      } else {
        mqttConnected = false;
      }
    }
  } else {
    if (!mqttConnected) {
      mqttConnected = true;
      Serial.println("📡 [MQTT] Connection established → subscribing to topics");
      
      mqttClient.subscribe((deviceTopic + "/light/cmd").c_str());
      mqttClient.subscribe((deviceTopic + "/brightness/cmd").c_str());
      mqttClient.subscribe((deviceTopic + "/color/cmd").c_str());
      mqttClient.subscribe((deviceTopic + "/effect/cmd").c_str());
      mqttClient.subscribe((deviceTopic + "/speed/cmd").c_str());
      
      publishHomeAssistantDiscovery();
      mqttPublishState();
    }
    mqttClient.loop();
  }
}

// ============================================================================
// HOME ASSISTANT MQTT DISCOVERY
// ============================================================================

void publishHomeAssistantDiscovery() {
  if (!mqttClient.connected()) {
    Serial.println("MQTT not connected - cannot publish discovery");
    return;
  }

  Serial.println("\n=== Publishing Home Assistant Discovery ===");
  vTaskDelay(1000 / portTICK_PERIOD_MS);

  String deviceId = deviceID;
  String discoveryPrefix = "homeassistant";

  // COMMON DEVICE OBJECT
  DynamicJsonDocument device(256);
  JsonArray identifiers = device.createNestedArray("identifiers");
  identifiers.add(deviceId);
  device["name"] = mqttConfig.device_name;
  device["manufacturer"] = "Lumina";
  device["model"] = "LED Controller";
  device["sw_version"] = currentFirmwareVersion;

  // LIGHT ENTITY (Now only for Color/Brightness/Power)
  {
    String configTopic = discoveryPrefix + "/light/" + deviceId + "/config";
    DynamicJsonDocument doc(4096);

    doc["name"] = String(mqttConfig.device_name) + " Light";
    doc["unique_id"] = deviceId + "_light";
    doc["command_topic"] = deviceTopic + "/light/cmd";
    doc["state_topic"] = deviceTopic + "/state";
    doc["brightness_command_topic"] = deviceTopic + "/brightness/cmd";
    doc["brightness_state_topic"] = deviceTopic + "/state";
    doc["brightness_scale"] = 255;
    doc["brightness_value_template"] = "{{ value_json.brightness }}";
    doc["rgb_command_topic"] = deviceTopic + "/color/cmd";
    doc["rgb_state_topic"] = deviceTopic + "/state";
    doc["rgb_value_template"] = "{{ value_json.color.r }},{{ value_json.color.g }},{{ value_json.color.b }}";
    
    // NO EFFECTS HERE - Moved to separate entity
    
    JsonArray colorModes = doc.createNestedArray("supported_color_modes");
    colorModes.add("rgb");

    doc["device"] = device;
    doc["availability_topic"] = deviceTopic + "/status";

    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
    delay(100);
  }

  // POWER SWITCH (Separate button for power)
  {
    String configTopic = discoveryPrefix + "/switch/" + deviceId + "_power/config";
    DynamicJsonDocument doc(1024);

    doc["name"] = String(mqttConfig.device_name) + " Power";
    doc["unique_id"] = deviceId + "_power";
    doc["command_topic"] = deviceTopic + "/light/cmd";
    doc["state_topic"] = deviceTopic + "/state";
    doc["value_template"] = "{{ value_json.state }}";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:power";
    doc["device"] = device;
    doc["availability_topic"] = deviceTopic + "/status";

    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
    delay(100);
  }

  // EFFECT SELECT (Separate selector for effects)
  {
    String configTopic = discoveryPrefix + "/select/" + deviceId + "_effect/config";
    DynamicJsonDocument doc(4096);

    doc["name"] = String(mqttConfig.device_name) + " Effect";
    doc["unique_id"] = deviceId + "_effect";
    doc["command_topic"] = deviceTopic + "/effect/cmd";
    doc["state_topic"] = deviceTopic + "/state";
    doc["value_template"] = "{{ value_json.effect }}";
    
    JsonArray options = doc.createNestedArray("options");
    for (int i = 0; i < NUM_EFFECTS; i++) {
      options.add(EFFECT_NAMES[i]);
    }
    
    doc["icon"] = "mdi:palette-outline";
    doc["device"] = device;
    doc["availability_topic"] = deviceTopic + "/status";

    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
    delay(100);
  }

  // BRIGHTNESS NUMBER (Separate slider for brightness)
  {
    String configTopic = discoveryPrefix + "/number/" + deviceId + "_brightness/config";
    DynamicJsonDocument doc(1024);

    doc["name"] = String(mqttConfig.device_name) + " Brightness";
    doc["unique_id"] = deviceId + "_brightness";
    doc["command_topic"] = deviceTopic + "/brightness/cmd";
    doc["state_topic"] = deviceTopic + "/state";
    doc["value_template"] = "{{ value_json.brightness }}";
    doc["min"] = 0;
    doc["max"] = 255;
    doc["step"] = 1;
    doc["icon"] = "mdi:brightness-6";
    doc["mode"] = "slider";
    doc["device"] = device;
    doc["availability_topic"] = deviceTopic + "/status";

    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
    delay(100);
  }

  // SPEED NUMBER
  {
    String configTopic = discoveryPrefix + "/number/" + deviceId + "_speed/config";
    DynamicJsonDocument doc(1024);

    doc["name"] = String(mqttConfig.device_name) + " Speed";
    doc["unique_id"] = deviceId + "_speed";
    doc["command_topic"] = deviceTopic + "/speed/cmd";
    doc["state_topic"] = deviceTopic + "/state";
    doc["value_template"] = "{{ value_json.speed }}";
    doc["min"] = 10;
    doc["max"] = 200;
    doc["step"] = 5;
    doc["unit_of_measurement"] = "ms";
    doc["icon"] = "mdi:speedometer";
    doc["mode"] = "slider";
    doc["device"] = device;
    doc["availability_topic"] = deviceTopic + "/status";

    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
    delay(100);
  }

  // PRESENCE BINARY SENSOR
  {
    String configTopic = discoveryPrefix + "/binary_sensor/" + deviceId + "_presence/config";
    DynamicJsonDocument doc(1024);

    doc["name"] = String(mqttConfig.device_name) + " Presence";
    doc["unique_id"] = deviceId + "_presence";
    doc["state_topic"] = deviceTopic + "/sensors/presence";
    doc["device_class"] = "presence";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["device"] = device;
    doc["availability_topic"] = deviceTopic + "/status";

    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
    delay(100);
  }

  // VOLTAGE SENSOR
  {
    String configTopic = discoveryPrefix + "/sensor/" + deviceId + "_voltage/config";
    DynamicJsonDocument doc(1024);

    doc["name"] = String(mqttConfig.device_name) + " Voltage";
    doc["unique_id"] = deviceId + "_voltage";
    doc["state_topic"] = deviceTopic + "/sensors/power";
    doc["value_template"] = "{{ value_json.voltage }}";
    doc["unit_of_measurement"] = "V";
    doc["device_class"] = "voltage";
    doc["state_class"] = "measurement";
    doc["device"] = device;
    doc["availability_topic"] = deviceTopic + "/status";

    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
    delay(100);
  }

  // CURRENT SENSOR
  {
    String configTopic = discoveryPrefix + "/sensor/" + deviceId + "_current/config";
    DynamicJsonDocument doc(1024);

    doc["name"] = String(mqttConfig.device_name) + " Current";
    doc["unique_id"] = deviceId + "_current";
    doc["state_topic"] = deviceTopic + "/sensors/power";
    doc["value_template"] = "{{ value_json.current }}";
    doc["unit_of_measurement"] = "mA";
    doc["device_class"] = "current";
    doc["state_class"] = "measurement";
    doc["device"] = device;
    doc["availability_topic"] = deviceTopic + "/status";

    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
    delay(100);
  }

  // POWER SENSOR
  {
    String configTopic = discoveryPrefix + "/sensor/" + deviceId + "_power/config";
    DynamicJsonDocument doc(1024);

    doc["name"] = String(mqttConfig.device_name) + " Power";
    doc["unique_id"] = deviceId + "_power";
    doc["state_topic"] = deviceTopic + "/sensors/power";
    doc["value_template"] = "{{ value_json.power }}";
    doc["unit_of_measurement"] = "mW";
    doc["device_class"] = "power";
    doc["state_class"] = "measurement";
    doc["device"] = device;
    doc["availability_topic"] = deviceTopic + "/status";

    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
    delay(100);
  }

  // UPTIME SENSOR
  {
    String configTopic = discoveryPrefix + "/sensor/" + deviceId + "_uptime/config";
    DynamicJsonDocument doc(1024);

    doc["name"] = String(mqttConfig.device_name) + " Uptime";
    doc["unique_id"] = deviceId + "_uptime";
    doc["state_topic"] = deviceTopic + "/sensors/system";
    doc["value_template"] = "{{ value_json.uptime }}";
    doc["icon"] = "mdi:timer-outline";
    doc["device"] = device;
    doc["availability_topic"] = deviceTopic + "/status";

    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
    delay(100);
  }

  // WIFI STRENGTH SENSOR
  {
    String configTopic = discoveryPrefix + "/sensor/" + deviceId + "_wifi/config";
    DynamicJsonDocument doc(1024);

    doc["name"] = String(mqttConfig.device_name) + " WiFi Signal";
    doc["unique_id"] = deviceId + "_wifi";
    doc["state_topic"] = deviceTopic + "/sensors/system";
    doc["value_template"] = "{{ value_json.wifi_rssi }}";
    doc["unit_of_measurement"] = "dBm";
    doc["device_class"] = "signal_strength";
    doc["state_class"] = "measurement";
    doc["device"] = device;
    doc["availability_topic"] = deviceTopic + "/status";

    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
    delay(100);
  }

  // CPU TEMP SENSOR
  {
    String configTopic = discoveryPrefix + "/sensor/" + deviceId + "_cpu_temp/config";
    DynamicJsonDocument doc(1024);

    doc["name"] = String(mqttConfig.device_name) + " CPU Temp";
    doc["unique_id"] = deviceId + "_cpu_temp";
    doc["state_topic"] = deviceTopic + "/sensors/system";
    doc["value_template"] = "{{ value_json.cpu_temp }}";
    doc["unit_of_measurement"] = "°C";
    doc["device_class"] = "temperature";
    doc["state_class"] = "measurement";
    doc["device"] = device;
    doc["availability_topic"] = deviceTopic + "/status";

    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
    delay(100);
  }

  // RAM USAGE SENSOR
  {
    String configTopic = discoveryPrefix + "/sensor/" + deviceId + "_ram/config";
    DynamicJsonDocument doc(1024);

    doc["name"] = String(mqttConfig.device_name) + " RAM Usage";
    doc["unique_id"] = deviceId + "_ram";
    doc["state_topic"] = deviceTopic + "/sensors/system";
    doc["value_template"] = "{{ value_json.ram_usage }}";
    doc["unit_of_measurement"] = "%";
    doc["icon"] = "mdi:memory";
    doc["state_class"] = "measurement";
    doc["device"] = device;
    doc["availability_topic"] = deviceTopic + "/status";

    String payload;
    serializeJson(doc, payload);
    mqttClient.publish(configTopic.c_str(), payload.c_str(), true);
    delay(100);
  }

  Serial.println("=== Discovery Complete ===\n");
}

// ============================================================================
// MQTT PUBLISH STATE
// ============================================================================

void mqttPublishState() {
  if (!mqttClient.connected()) return;

  unsigned long now = millis();
  if (now - lastMQTTStatePublish < MQTT_STATE_PUBLISH_INTERVAL) {
    return;
  }
  lastMQTTStatePublish = now;

  DynamicJsonDocument stateDoc(1024);

  stateDoc["state"] = stripEnabled ? "ON" : "OFF";
  stateDoc["brightness"] = FastLED.getBrightness();
  stateDoc["color_mode"] = "rgb";
  
  if (currentEffect >= 0 && currentEffect < NUM_EFFECTS) {
    stateDoc["effect"] = EFFECT_NAMES[currentEffect];
  } else {
    stateDoc["effect"] = EFFECT_NAMES[0];
  }

  stateDoc["color"]["r"] = (effectColor >> 16) & 0xFF;
  stateDoc["color"]["g"] = (effectColor >> 8) & 0xFF;
  stateDoc["color"]["b"] = effectColor & 0xFF;
  
  stateDoc["speed"] = effectSpeed;

  String statePayload;
  serializeJson(stateDoc, statePayload);
  mqttClient.publish((deviceTopic + "/state").c_str(), statePayload.c_str());

  mqttClient.publish((deviceTopic + "/status").c_str(), "online");
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
  Serial.printf("📡 [MQTT] %s → %s\n", topicStr.c_str(), message);

  // Light control
  if (topicStr.endsWith("/light/cmd")) {
    bool newState = (strcmp(message, "ON") == 0);
    
    portENTER_CRITICAL(&stripMux);
    stripEnabled = newState;
    portEXIT_CRITICAL(&stripMux);
    
    manuallyTurnedOff = !newState;
    
    if (!newState) {
      FastLED.clear();
      FastLED.show();
    }
    
    enqueueFirebaseRequest(FB_SET_BOOL, basePath + "/local/enabled", newState ? "true" : "false");
    syncAllMirrors();
    
    lastMQTTStatePublish = 0;
    mqttPublishState();
  }
  // Brightness control
  else if (topicStr.endsWith("/brightness/cmd")) {
    int brightness = atoi(message);
    brightness = constrain(brightness, 0, 255);
    FastLED.setBrightness(brightness);
    enqueueFirebaseRequest(FB_SET_INT, basePath + "/local/brightness", String(brightness));
    lastMQTTStatePublish = 0;
    mqttPublishState();
  }
  // Color control
  else if (topicStr.endsWith("/color/cmd")) {
    uint8_t r = 0, g = 0, b = 0;
    bool parsed = false;
    
    DynamicJsonDocument doc(256);
    if (deserializeJson(doc, message) == DeserializationError::Ok) {
      if (doc.containsKey("r") && doc.containsKey("g") && doc.containsKey("b")) {
        r = doc["r"] | 0;
        g = doc["g"] | 0;
        b = doc["b"] | 0;
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
      effectColor = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
      char colorStr[7];
      sprintf(colorStr, "%02X%02X%02X", r, g, b);
      enqueueFirebaseRequest(FB_SET_STRING, basePath + "/local/color", colorStr);
      lastMQTTStatePublish = 0;
      mqttPublishState();
    }
  }
  // Effect selection
  else if (topicStr.endsWith("/effect/cmd")) {
    String payloadStr = String(message);
    int newEffect = -1;
    for (int i = 0; i < NUM_EFFECTS; i++) {
      if (payloadStr.equalsIgnoreCase(EFFECT_NAMES[i])) {
        newEffect = i;
        break;
      }
    }
    if (newEffect == -1) {
      int val = payloadStr.toInt();
      if (val >= 0 && val < NUM_EFFECTS) newEffect = val;
    }
    if (newEffect >= 0 && newEffect < NUM_EFFECTS) {
      currentEffect = newEffect;
      enqueueFirebaseRequest(FB_SET_INT, basePath + "/local/effect", String(currentEffect));
      lastMQTTStatePublish = 0;
      mqttPublishState();
    }
  }
  // Speed control
  else if (topicStr.endsWith("/speed/cmd")) {
    int speed = atoi(message);
    speed = constrain(speed, 10, 200);
    effectSpeed = speed;
    enqueueFirebaseRequest(FB_SET_INT, basePath + "/local/speed", String(speed));
  }

  broadcastGatewayState();
}

// ============================================================================
// IO TASK FOR FREERTOS (Merges MQTT + SmartHome)
// ============================================================================

void ioTask(void *parameter) {
  Serial.println("🌐 IO Task (MQTT+SmartHome) started on Core " + String(xPortGetCoreID()));
  
  for (;;) {
    esp_task_wdt_reset();
    
    if (WiFi.status() == WL_CONNECTED && firebaseConnected) {
      // 1. Handle MQTT
      mqttConnectAndSubscribe(); // Includes mqttClient.loop()
      mqttPublishState();
      mqttPublishSensorData();

      // 2. Handle Smart Home (SinricPro + Espalexa)
      handleSmartHome();
    }
    
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}