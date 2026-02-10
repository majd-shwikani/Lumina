// ============================================================================
// mqtt_integration.cpp - FIXED VERSION (COMPLETE)
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <FastLED.h>
#include "mqtt_integration.h"
#include "config.h"
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
    "Rainbow", "Meteor Shower", "Digital Rain", "Pulsing Spheres", "Binary Clock",
    "Vortex", "DNA Helix", "Audio Visualizer", "Lava Lamp", "Radar Sweep",
    "Quantum Particles", "Neural Network", "Galaxy Spin", "Crystal Growth",
    "Lightning Storm", "Ocean Depth", "Northern Lights", "Time Tunnel",
    "Cyber City", "Solar Flare", "Fire Simulation", "Solid Color",
    "Frequency Spectrum", "Reactive Waveform", "Beat Pulse", "Frequency Bloom",
    "Audio Reactive Fire", "Musical Rainbow", "Reactive Strobe", "Guitar Visualizer",
    "Cascading Frequency", "Energy Orbits", "Audio Ripples"
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

  Serial.println("MQTT config loaded from SPIFFS");
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
  Serial.println("MQTT config saved to SPIFFS");
}

// ============================================================================
// UPDATE MQTT CONFIG FROM FIREBASE
// ============================================================================

void updateMQTTConfigFromFirebase() {
  String mqttBasePath = basePath + "/mqtt";

  String brokerPath = mqttBasePath + "/broker_address";
  if (Firebase.RTDB.getString(&fbdoUpload, brokerPath.c_str())) {
    String brokerStr = fbdoUpload.stringData();
    strlcpy(mqttConfig.broker_address, brokerStr.c_str(), sizeof(mqttConfig.broker_address));
    Serial.printf("MQTT broker: %s\n", mqttConfig.broker_address);
  }

  String portPath = mqttBasePath + "/broker_port";
  if (Firebase.RTDB.getInt(&fbdoUpload, portPath.c_str())) {
    mqttConfig.broker_port = fbdoUpload.intData();
    Serial.printf("MQTT port: %d\n", mqttConfig.broker_port);
  } else {
    mqttConfig.broker_port = 1883;
  }

  String usernamePath = mqttBasePath + "/username";
  if (Firebase.RTDB.getString(&fbdoUpload, usernamePath.c_str())) {
    String usernameStr = fbdoUpload.stringData();
    strlcpy(mqttConfig.username, usernameStr.c_str(), sizeof(mqttConfig.username));
    Serial.println("MQTT username loaded");
  }

  String passwordPath = mqttBasePath + "/password";
  if (Firebase.RTDB.getString(&fbdoUpload, passwordPath.c_str())) {
    String passwordStr = fbdoUpload.stringData();
    strlcpy(mqttConfig.password, passwordStr.c_str(), sizeof(mqttConfig.password));
    Serial.println("MQTT password loaded");
  }

  String enabledPath = mqttBasePath + "/enabled";
  if (Firebase.RTDB.getBool(&fbdoUpload, enabledPath.c_str())) {
    mqttConfig.enabled = fbdoUpload.boolData();
    Serial.printf("MQTT enabled: %s\n", mqttConfig.enabled ? "true" : "false");
  } else {
    mqttConfig.enabled = false;
  }

  String deviceNamePath = mqttBasePath + "/device_name";
  if (Firebase.RTDB.getString(&fbdoUpload, deviceNamePath.c_str())) {
    String deviceNameStr = fbdoUpload.stringData();
    strlcpy(mqttConfig.device_name, deviceNameStr.c_str(), sizeof(mqttConfig.device_name));
    Serial.printf("MQTT device name: %s\n", mqttConfig.device_name);
  } else {
    strlcpy(mqttConfig.device_name, "Lumina", sizeof(mqttConfig.device_name));
  }

  saveMQTTConfig();
  Serial.println("MQTT configuration synchronized with Firebase");
}

// ============================================================================
// MQTT CONNECTION
// ============================================================================

bool connectToMQTT() {
  if (!mqttConfig.enabled || strlen(mqttConfig.broker_address) == 0) {
    Serial.println("MQTT not enabled or broker address not set");
    return false;
  }

  Serial.printf("Connecting to MQTT broker: %s:%d\n", mqttConfig.broker_address, mqttConfig.broker_port);

  mqttClient.setServer(mqttConfig.broker_address, mqttConfig.broker_port);
  mqttClient.setCallback(mqttCallback);

  if (strlen(mqttConfig.username) > 0) {
    if (mqttClient.connect(deviceID.c_str(), mqttConfig.username, mqttConfig.password)) {
      Serial.println("Connected to MQTT broker");
      return true;
    }
  } else {
    if (mqttClient.connect(deviceID.c_str())) {
      Serial.println("Connected to MQTT broker (no auth)");
      return true;
    }
  }

  Serial.printf("MQTT connection failed, rc=%d\n", mqttClient.state());
  return false;
}

void setupMQTT() {
  delay(1000);
  
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
        Serial.println("MQTT reconnected");
        
        mqttClient.subscribe((deviceTopic + "/light/cmd").c_str());
        mqttClient.subscribe((deviceTopic + "/brightness/cmd").c_str());
        mqttClient.subscribe((deviceTopic + "/color/cmd").c_str());
        mqttClient.subscribe((deviceTopic + "/effect/cmd").c_str());
        mqttClient.subscribe((deviceTopic + "/speed/cmd").c_str());
        mqttClient.subscribe((deviceTopic + "/timer/cmd").c_str());
        mqttClient.subscribe((deviceTopic + "/timer_on/cmd").c_str());
        mqttClient.subscribe((deviceTopic + "/timer_off/cmd").c_str());
        mqttClient.subscribe((deviceTopic + "/auto_darkness/cmd").c_str());
        mqttClient.subscribe((deviceTopic + "/lux_threshold/cmd").c_str());
        mqttClient.subscribe((deviceTopic + "/calibrate_mic/cmd").c_str());
        
        Serial.println("Subscribed to all command topics");

        publishHomeAssistantDiscovery();
        mqttPublishState();
      } else {
        mqttConnected = false;
      }
    }
  } else {
    if (!mqttConnected) {
      mqttConnected = true;
      Serial.println("MQTT connection established");
      
      mqttClient.subscribe((deviceTopic + "/light/cmd").c_str());
      mqttClient.subscribe((deviceTopic + "/brightness/cmd").c_str());
      mqttClient.subscribe((deviceTopic + "/color/cmd").c_str());
      mqttClient.subscribe((deviceTopic + "/effect/cmd").c_str());
      mqttClient.subscribe((deviceTopic + "/speed/cmd").c_str());
      mqttClient.subscribe((deviceTopic + "/timer/cmd").c_str());
      mqttClient.subscribe((deviceTopic + "/timer_on/cmd").c_str());
      mqttClient.subscribe((deviceTopic + "/timer_off/cmd").c_str());
      mqttClient.subscribe((deviceTopic + "/auto_darkness/cmd").c_str());
      mqttClient.subscribe((deviceTopic + "/lux_threshold/cmd").c_str());
      mqttClient.subscribe((deviceTopic + "/calibrate_mic/cmd").c_str());
      
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
  delay(1000);

  String deviceId = deviceID;
  String discoveryPrefix = "homeassistant";

  // LIGHT ENTITY
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
    doc["effect_command_topic"] = deviceTopic + "/effect/cmd";
    doc["effect_state_topic"] = deviceTopic + "/state";
    doc["effect_value_template"] = "{{ value_json.effect }}";
    
    JsonArray effectList = doc.createNestedArray("effect_list");
    for (int i = 0; i < NUM_EFFECTS; i++) {
      effectList.add(EFFECT_NAMES[i]);
    }

    JsonArray colorModes = doc.createNestedArray("supported_color_modes");
    colorModes.add("rgb");

    JsonObject device = doc.createNestedObject("device");
    JsonArray identifiers = device.createNestedArray("identifiers");
    identifiers.add(deviceId);
    device["name"] = mqttConfig.device_name;
    device["manufacturer"] = "Lumina";
    device["model"] = "LED Controller";
    
    doc["availability_topic"] = deviceTopic + "/status";
    doc["payload_available"] = "online";
    doc["payload_not_available"] = "offline";

    String payload;
    serializeJson(doc, payload);
    
    if (mqttClient.publish(configTopic.c_str(), payload.c_str(), true)) {
      Serial.println("✓ Light entity published");
    }
    delay(200);
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
    
    JsonObject device = doc.createNestedObject("device");
    JsonArray identifiers = device.createNestedArray("identifiers");
    identifiers.add(deviceId);
    doc["availability_topic"] = deviceTopic + "/status";

    String payload;
    serializeJson(doc, payload);
    if (mqttClient.publish(configTopic.c_str(), payload.c_str(), true)) {
      Serial.println("✓ Speed entity published");
    }
    delay(200);
  }

  // TIMER ON TIME
  {
    String configTopic = discoveryPrefix + "/text/" + deviceId + "_timer_on/config";
    DynamicJsonDocument doc(1024);

    doc["name"] = String(mqttConfig.device_name) + " Timer ON";
    doc["unique_id"] = deviceId + "_timer_on";
    doc["command_topic"] = deviceTopic + "/timer_on/cmd";
    doc["state_topic"] = deviceTopic + "/state";
    doc["value_template"] = "{{ value_json.timer_on }}";
    doc["icon"] = "mdi:clock-start";
    
    JsonObject device = doc.createNestedObject("device");
    JsonArray identifiers = device.createNestedArray("identifiers");
    identifiers.add(deviceId);
    doc["availability_topic"] = deviceTopic + "/status";

    String payload;
    serializeJson(doc, payload);
    if (mqttClient.publish(configTopic.c_str(), payload.c_str(), true)) {
      Serial.println("✓ Timer ON entity published");
    }
    delay(200);
  }

  // TIMER OFF TIME
  {
    String configTopic = discoveryPrefix + "/text/" + deviceId + "_timer_off/config";
    DynamicJsonDocument doc(1024);

    doc["name"] = String(mqttConfig.device_name) + " Timer OFF";
    doc["unique_id"] = deviceId + "_timer_off";
    doc["command_topic"] = deviceTopic + "/timer_off/cmd";
    doc["state_topic"] = deviceTopic + "/state";
    doc["value_template"] = "{{ value_json.timer_off }}";
    doc["icon"] = "mdi:clock-end";
    
    JsonObject device = doc.createNestedObject("device");
    JsonArray identifiers = device.createNestedArray("identifiers");
    identifiers.add(deviceId);
    doc["availability_topic"] = deviceTopic + "/status";

    String payload;
    serializeJson(doc, payload);
    if (mqttClient.publish(configTopic.c_str(), payload.c_str(), true)) {
      Serial.println("✓ Timer OFF entity published");
    }
    delay(200);
  }

  // TIMER ENABLED SWITCH
  {
    String configTopic = discoveryPrefix + "/switch/" + deviceId + "_timer/config";
    DynamicJsonDocument doc(1024);

    doc["name"] = String(mqttConfig.device_name) + " Timer";
    doc["unique_id"] = deviceId + "_timer";
    doc["command_topic"] = deviceTopic + "/timer/cmd";
    doc["state_topic"] = deviceTopic + "/state";
    doc["value_template"] = "{{ value_json.timer }}";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:clock";
    
    JsonObject device = doc.createNestedObject("device");
    JsonArray identifiers = device.createNestedArray("identifiers");
    identifiers.add(deviceId);
    doc["availability_topic"] = deviceTopic + "/status";

    String payload;
    serializeJson(doc, payload);
    if (mqttClient.publish(configTopic.c_str(), payload.c_str(), true)) {
      Serial.println("✓ Timer switch entity published");
    }
    delay(200);
  }

  // AUTO DARKNESS CONTROL SWITCH
  {
    String configTopic = discoveryPrefix + "/switch/" + deviceId + "_auto_darkness/config";
    DynamicJsonDocument doc(1024);

    doc["name"] = String(mqttConfig.device_name) + " Auto Darkness";
    doc["unique_id"] = deviceId + "_auto_darkness";
    doc["command_topic"] = deviceTopic + "/auto_darkness/cmd";
    doc["state_topic"] = deviceTopic + "/state";
    doc["value_template"] = "{{ value_json.auto_darkness }}";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:brightness-auto";
    
    JsonObject device = doc.createNestedObject("device");
    JsonArray identifiers = device.createNestedArray("identifiers");
    identifiers.add(deviceId);
    doc["availability_topic"] = deviceTopic + "/status";

    String payload;
    serializeJson(doc, payload);
    if (mqttClient.publish(configTopic.c_str(), payload.c_str(), true)) {
      Serial.println("✓ Auto Darkness entity published");
    }
    delay(200);
  }

  // LUX THRESHOLD NUMBER
  {
    String configTopic = discoveryPrefix + "/number/" + deviceId + "_lux_threshold/config";
    DynamicJsonDocument doc(1024);

    doc["name"] = String(mqttConfig.device_name) + " Lux Threshold";
    doc["unique_id"] = deviceId + "_lux_threshold";
    doc["command_topic"] = deviceTopic + "/lux_threshold/cmd";
    doc["state_topic"] = deviceTopic + "/state";
    doc["value_template"] = "{{ value_json.lux_threshold }}";
    doc["min"] = 0.5;
    doc["max"] = 100;
    doc["step"] = 0.1;
    doc["unit_of_measurement"] = "lux";
    doc["icon"] = "mdi:lightbulb";
    doc["mode"] = "box";
    
    JsonObject device = doc.createNestedObject("device");
    JsonArray identifiers = device.createNestedArray("identifiers");
    identifiers.add(deviceId);
    doc["availability_topic"] = deviceTopic + "/status";

    String payload;
    serializeJson(doc, payload);
    if (mqttClient.publish(configTopic.c_str(), payload.c_str(), true)) {
      Serial.println("✓ Lux Threshold entity published");
    }
    delay(200);
  }

  // MICROPHONE CALIBRATION BUTTON
  {
    String configTopic = discoveryPrefix + "/button/" + deviceId + "_calibrate_mic/config";
    DynamicJsonDocument doc(1024);

    doc["name"] = String(mqttConfig.device_name) + " Calibrate Mic";
    doc["unique_id"] = deviceId + "_calibrate_mic";
    doc["command_topic"] = deviceTopic + "/calibrate_mic/cmd";
    doc["payload_press"] = "PRESS";
    doc["icon"] = "mdi:microphone";
    
    JsonObject device = doc.createNestedObject("device");
    JsonArray identifiers = device.createNestedArray("identifiers");
    identifiers.add(deviceId);
    doc["availability_topic"] = deviceTopic + "/status";

    String payload;
    serializeJson(doc, payload);
    if (mqttClient.publish(configTopic.c_str(), payload.c_str(), true)) {
      Serial.println("✓ Calibrate Mic button published");
    }
    delay(200);
  }

  // LUX SENSOR
  {
    String configTopic = discoveryPrefix + "/sensor/" + deviceId + "_lux/config";
    DynamicJsonDocument doc(1024);

    doc["name"] = String(mqttConfig.device_name) + " Lux";
    doc["unique_id"] = deviceId + "_lux";
    doc["state_topic"] = deviceTopic + "/sensors/lux";
    doc["unit_of_measurement"] = "lux";
    doc["icon"] = "mdi:lightbulb";
    
    JsonObject device = doc.createNestedObject("device");
    JsonArray identifiers = device.createNestedArray("identifiers");
    identifiers.add(deviceId);
    doc["availability_topic"] = deviceTopic + "/status";

    String payload;
    serializeJson(doc, payload);
    if (mqttClient.publish(configTopic.c_str(), payload.c_str(), true)) {
      Serial.println("✓ Lux sensor published");
    }
    delay(200);
  }

  // AUDIO LEVEL SENSOR
  {
    String configTopic = discoveryPrefix + "/sensor/" + deviceId + "_audio/config";
    DynamicJsonDocument doc(1024);

    doc["name"] = String(mqttConfig.device_name) + " Audio Level";
    doc["unique_id"] = deviceId + "_audio";
    doc["state_topic"] = deviceTopic + "/sensors/audio_level";
    doc["unit_of_measurement"] = "%";
    doc["icon"] = "mdi:volume-high";
    
    JsonObject device = doc.createNestedObject("device");
    JsonArray identifiers = device.createNestedArray("identifiers");
    identifiers.add(deviceId);
    doc["availability_topic"] = deviceTopic + "/status";

    String payload;
    serializeJson(doc, payload);
    if (mqttClient.publish(configTopic.c_str(), payload.c_str(), true)) {
      Serial.println("✓ Audio Level sensor published");
    }
    delay(200);
  }

  // FIRMWARE VERSION SENSOR
  {
    String configTopic = discoveryPrefix + "/sensor/" + deviceId + "_firmware/config";
    DynamicJsonDocument doc(1024);

    doc["name"] = String(mqttConfig.device_name) + " Firmware";
    doc["unique_id"] = deviceId + "_firmware";
    doc["state_topic"] = deviceTopic + "/sensors/firmware";
    doc["icon"] = "mdi:information";
    
    JsonObject device = doc.createNestedObject("device");
    JsonArray identifiers = device.createNestedArray("identifiers");
    identifiers.add(deviceId);
    doc["availability_topic"] = deviceTopic + "/status";

    String payload;
    serializeJson(doc, payload);
    if (mqttClient.publish(configTopic.c_str(), payload.c_str(), true)) {
      Serial.println("✓ Firmware version sensor published");
    }
    delay(200);
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
  stateDoc["timer"] = timerEnabled ? "ON" : "OFF";
  stateDoc["timer_on"] = timerOnTime;
  stateDoc["timer_off"] = timerOffTime;
  stateDoc["auto_darkness"] = autoDarknessControl ? "ON" : "OFF";
  stateDoc["lux_threshold"] = luxThreshold;

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

  if (sensorAvailable) {
    String luxPayload = String(currentLux, 2);
    mqttClient.publish((deviceTopic + "/sensors/lux").c_str(), luxPayload.c_str());
  }

  String audioPayload = String((int)(globalAudioLevel * 100));
  mqttClient.publish((deviceTopic + "/sensors/audio_level").c_str(), audioPayload.c_str());

  extern const char* currentFirmwareVersion;
  mqttClient.publish((deviceTopic + "/sensors/firmware").c_str(), currentFirmwareVersion);
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
  Serial.printf("MQTT Message - Topic: %s, Payload: %s\n", topic, message);

  // Light control
  if (topicStr.endsWith("/light/cmd")) {
    if (strcmp(message, "ON") == 0) {
      stripEnabled = true;
      Firebase.RTDB.setBool(&fbdoUpload, (basePath + "/enabled").c_str(), true);
    } else if (strcmp(message, "OFF") == 0) {
      stripEnabled = false;
      Firebase.RTDB.setBool(&fbdoUpload, (basePath + "/enabled").c_str(), false);
      FastLED.clear();
      FastLED.show();
    }
    lastMQTTStatePublish = 0;
    mqttPublishState();
  }
  // Brightness control
  else if (topicStr.endsWith("/brightness/cmd")) {
    int brightness = atoi(message);
    brightness = constrain(brightness, 0, 255);
    FastLED.setBrightness(brightness);
    lastMQTTStatePublish = 0;
    mqttPublishState();
  }
  // Color control - Handle both JSON and comma-separated formats
  else if (topicStr.endsWith("/color/cmd")) {
    uint8_t r = 0, g = 0, b = 0;
    bool parsed = false;
    
    // Try parsing as JSON first
    DynamicJsonDocument doc(256);
    if (deserializeJson(doc, message) == DeserializationError::Ok) {
      if (doc.containsKey("r") && doc.containsKey("g") && doc.containsKey("b")) {
        r = doc["r"] | 0;
        g = doc["g"] | 0;
        b = doc["b"] | 0;
        parsed = true;
        Serial.println("Color parsed as JSON");
      }
    }
    
    // If JSON parsing failed, try comma-separated format (Home Assistant default)
    if (!parsed) {
      String msgStr(message);
      int commaPos1 = msgStr.indexOf(',');
      int commaPos2 = msgStr.lastIndexOf(',');
      
      if (commaPos1 > 0 && commaPos2 > commaPos1) {
        r = atoi(msgStr.substring(0, commaPos1).c_str());
        g = atoi(msgStr.substring(commaPos1 + 1, commaPos2).c_str());
        b = atoi(msgStr.substring(commaPos2 + 1).c_str());
        parsed = true;
        Serial.printf("Color parsed as CSV: R=%d, G=%d, B=%d\n", r, g, b);
      }
    }
    
    if (parsed) {
      effectColor = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
      
      char colorStr[7];
      sprintf(colorStr, "%02X%02X%02X", r, g, b);
      Firebase.RTDB.setString(&fbdoUpload, (basePath + "/color").c_str(), colorStr);
      
      Serial.printf("Color updated: #%s\n", colorStr);
      lastMQTTStatePublish = 0;
      mqttPublishState();
    } else {
      Serial.println("Failed to parse color command");
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
      if (payloadStr == "0") {
        newEffect = 0;
      } else {
        int val = payloadStr.toInt();
        if (val > 0) newEffect = val;
      }
    }
    
    if (newEffect >= 0 && newEffect < NUM_EFFECTS) {
      currentEffect = newEffect;
      Firebase.RTDB.setInt(&fbdoUpload, (basePath + "/effect").c_str(), currentEffect);
      lastMQTTStatePublish = 0;
      mqttPublishState();
    }
  }
  // Speed control
  else if (topicStr.endsWith("/speed/cmd")) {
    int speed = atoi(message);
    speed = constrain(speed, 10, 200);
    effectSpeed = speed;
    Firebase.RTDB.setInt(&fbdoUpload, (basePath + "/speed").c_str(), speed);
  }
  // Timer enabled
  else if (topicStr.endsWith("/timer/cmd")) {
    timerEnabled = strcmp(message, "ON") == 0;
    Firebase.RTDB.setBool(&fbdoUpload, (basePath + "/timer_enabled").c_str(), timerEnabled);
    lastMQTTStatePublish = 0;
    mqttPublishState();
  }
  // Timer ON time
  else if (topicStr.endsWith("/timer_on/cmd")) {
    if (strlen(message) == 5) {
      strncpy(timerOnTime, message, sizeof(timerOnTime));
      Firebase.RTDB.setString(&fbdoUpload, (basePath + "/timer_on").c_str(), timerOnTime);
      lastMQTTStatePublish = 0;
      mqttPublishState();
    }
  }
  // Timer OFF time
  else if (topicStr.endsWith("/timer_off/cmd")) {
    if (strlen(message) == 5) {
      strncpy(timerOffTime, message, sizeof(timerOffTime));
      Firebase.RTDB.setString(&fbdoUpload, (basePath + "/timer_off").c_str(), timerOffTime);
      lastMQTTStatePublish = 0;
      mqttPublishState();
    }
  }
  // Auto Darkness Control
  else if (topicStr.endsWith("/auto_darkness/cmd")) {
    autoDarknessControl = strcmp(message, "ON") == 0;
    Firebase.RTDB.setBool(&fbdoUpload, (basePath + "/auto_darkness_control").c_str(), autoDarknessControl);
    lastMQTTStatePublish = 0;
    mqttPublishState();
  }
  // Lux Threshold
  else if (topicStr.endsWith("/lux_threshold/cmd")) {
    float threshold = atof(message);
    threshold = constrain(threshold, 0.5, 100.0);
    luxThreshold = threshold;
    Firebase.RTDB.setFloat(&fbdoUpload, (basePath + "/lux_threshold").c_str(), threshold);
    lastMQTTStatePublish = 0;
    mqttPublishState();
  }
  // Microphone Calibration
  else if (topicStr.endsWith("/calibrate_mic/cmd")) {
    Serial.println("Microphone calibration triggered via MQTT");
    Firebase.RTDB.setBool(&fbdoUpload, (basePath + "/mic_calibration").c_str(), true);
    triggerMicCalibration = true;
  }
}

// ============================================================================
// MQTT TASK FOR FREERTOS
// ============================================================================

void mqttTask(void *parameter) {
  for (;;) {
    if (firebaseConnected && WiFi.status() == WL_CONNECTED) {
      mqttConnectAndSubscribe();
      mqttPublishState();
      mqttPublishSensorData();
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}