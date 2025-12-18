// ============================================================================
// mqtt_integration.cpp
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <Adafruit_NeoPixel.h>
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

  // Read broker address
  String brokerPath = mqttBasePath + "/broker_address";
  if (Firebase.RTDB.getString(&fbdoUpload, brokerPath.c_str())) {
    String brokerStr = fbdoUpload.stringData();
    strlcpy(mqttConfig.broker_address, brokerStr.c_str(), sizeof(mqttConfig.broker_address));
    Serial.printf("MQTT broker: %s\n", mqttConfig.broker_address);
  } else {
    Serial.println("⚠ MQTT broker_address not found in Firebase");
  }

  // Read broker port
  String portPath = mqttBasePath + "/broker_port";
  if (Firebase.RTDB.getInt(&fbdoUpload, portPath.c_str())) {
    mqttConfig.broker_port = fbdoUpload.intData();
    Serial.printf("MQTT port: %d\n", mqttConfig.broker_port);
  } else {
    Serial.println("⚠ MQTT broker_port not found, using default 1883");
    mqttConfig.broker_port = 1883;
  }

  // Read username
  String usernamePath = mqttBasePath + "/username";
  if (Firebase.RTDB.getString(&fbdoUpload, usernamePath.c_str())) {
    String usernameStr = fbdoUpload.stringData();
    strlcpy(mqttConfig.username, usernameStr.c_str(), sizeof(mqttConfig.username));
    Serial.println("MQTT username loaded");
  } else {
    Serial.println("⚠ MQTT username not found in Firebase");
  }

  // Read password
  String passwordPath = mqttBasePath + "/password";
  if (Firebase.RTDB.getString(&fbdoUpload, passwordPath.c_str())) {
    String passwordStr = fbdoUpload.stringData();
    strlcpy(mqttConfig.password, passwordStr.c_str(), sizeof(mqttConfig.password));
    Serial.println("MQTT password loaded");
  } else {
    Serial.println("⚠ MQTT password not found in Firebase");
  }

  // Read enabled state
  String enabledPath = mqttBasePath + "/enabled";
  if (Firebase.RTDB.getBool(&fbdoUpload, enabledPath.c_str())) {
    mqttConfig.enabled = fbdoUpload.boolData();
    Serial.printf("MQTT enabled: %s\n", mqttConfig.enabled ? "true" : "false");
  } else {
    Serial.println("⚠ MQTT enabled not found, setting to false");
    mqttConfig.enabled = false;
  }

  // Read device name
  String deviceNamePath = mqttBasePath + "/device_name";
  if (Firebase.RTDB.getString(&fbdoUpload, deviceNamePath.c_str())) {
    String deviceNameStr = fbdoUpload.stringData();
    strlcpy(mqttConfig.device_name, deviceNameStr.c_str(), sizeof(mqttConfig.device_name));
    Serial.printf("MQTT device name: %s\n", mqttConfig.device_name);
  } else {
    Serial.println("⚠ MQTT device_name not found, using default Lumina");
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
  // Wait for Firebase to be ready
  delay(1000);
  
  loadMQTTConfig();

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

  // Light entity
  {
    String configTopic = discoveryPrefix + "/light/" + deviceId + "/config";
    DynamicJsonDocument doc(2048);

    doc["name"] = String(mqttConfig.device_name) + " Light";
    doc["unique_id"] = deviceId + "_light";
    doc["command_topic"] = deviceTopic + "/light/cmd";
    doc["state_topic"] = deviceTopic + "/state";
    doc["brightness_command_topic"] = deviceTopic + "/brightness/cmd";
    doc["brightness_state_topic"] = deviceTopic + "/state";
    doc["brightness_scale"] = 100;
    doc["rgb_command_topic"] = deviceTopic + "/color/cmd";
    doc["rgb_state_topic"] = deviceTopic + "/state";
    doc["effect_command_topic"] = deviceTopic + "/effect/cmd";
    doc["effect_state_topic"] = deviceTopic + "/state";
    
    // Effect list
    JsonArray effectList = doc.createNestedArray("effect_list");
    effectList.add("Rainbow");
    effectList.add("Meteor Shower");
    effectList.add("Digital Rain");
    effectList.add("Pulsing Spheres");
    effectList.add("Binary Clock");
    effectList.add("Vortex");
    effectList.add("DNA Helix");
    effectList.add("Audio Visualizer");
    effectList.add("Lava Lamp");
    effectList.add("Radar Sweep");
    effectList.add("Quantum Particles");
    effectList.add("Neural Network");
    effectList.add("Galaxy Spin");
    effectList.add("Crystal Growth");
    effectList.add("Lightning Storm");
    effectList.add("Ocean Depth");
    effectList.add("Northern Lights");
    effectList.add("Time Tunnel");
    effectList.add("Cyber City");
    effectList.add("Solar Flare");
    effectList.add("Fire Simulation");
    effectList.add("Solid Color");
    effectList.add("Frequency Spectrum");
    effectList.add("Reactive Waveform");
    effectList.add("Beat Pulse");
    effectList.add("Frequency Bloom");
    effectList.add("Audio Reactive Fire");
    effectList.add("Musical Rainbow");
    effectList.add("Reactive Strobe");
    effectList.add("Guitar Visualizer");
    effectList.add("Cascading Frequency");
    effectList.add("Energy Orbits");
    effectList.add("Audio Ripples");

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
    
    Serial.printf("Publishing to: %s\n", configTopic.c_str());
    Serial.printf("Payload size: %d bytes\n", payload.length());
    
    if (mqttClient.publish(configTopic.c_str(), payload.c_str(), true)) {
      Serial.println("✓ Light discovery published");
    } else {
      Serial.println("✗ Failed to publish light discovery");
    }
    delay(200);
  }

  // Lux sensor
  {
    String configTopic = discoveryPrefix + "/sensor/" + deviceId + "_lux/config";
    DynamicJsonDocument doc(1024);

    doc["name"] = String(mqttConfig.device_name) + " Lux";
    doc["unique_id"] = deviceId + "_lux";
    doc["state_topic"] = deviceTopic + "/sensors/lux";
    doc["unit_of_measurement"] = "lux";
    doc["icon"] = "mdi:lightbulb";
    doc["value_template"] = "{{ value }}";
    
    JsonObject device = doc.createNestedObject("device");
    JsonArray identifiers = device.createNestedArray("identifiers");
    identifiers.add(deviceId);
    
    doc["availability_topic"] = deviceTopic + "/status";

    String payload;
    serializeJson(doc, payload);
    if (mqttClient.publish(configTopic.c_str(), payload.c_str(), true)) {
      Serial.println("✓ Lux sensor discovery published");
    }
    delay(200);
  }

  // Audio level sensor
  {
    String configTopic = discoveryPrefix + "/sensor/" + deviceId + "_audio/config";
    DynamicJsonDocument doc(1024);

    doc["name"] = String(mqttConfig.device_name) + " Audio";
    doc["unique_id"] = deviceId + "_audio";
    doc["state_topic"] = deviceTopic + "/sensors/audio_level";
    doc["unit_of_measurement"] = "%";
    doc["icon"] = "mdi:volume-high";
    doc["value_template"] = "{{ value }}";
    
    JsonObject device = doc.createNestedObject("device");
    JsonArray identifiers = device.createNestedArray("identifiers");
    identifiers.add(deviceId);
    
    doc["availability_topic"] = deviceTopic + "/status";

    String payload;
    serializeJson(doc, payload);
    if (mqttClient.publish(configTopic.c_str(), payload.c_str(), true)) {
      Serial.println("✓ Audio sensor discovery published");
    }
    delay(200);
  }

  // Timer switch
  {
    String configTopic = discoveryPrefix + "/switch/" + deviceId + "_timer/config";
    DynamicJsonDocument doc(1024);

    doc["name"] = String(mqttConfig.device_name) + " Timer";
    doc["unique_id"] = deviceId + "_timer";
    doc["command_topic"] = deviceTopic + "/timer/cmd";
    doc["state_topic"] = deviceTopic + "/state";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["state_on"] = "ON";
    doc["state_off"] = "OFF";
    doc["icon"] = "mdi:clock";
    doc["value_template"] = "{{ value_json.timer }}";
    
    JsonObject device = doc.createNestedObject("device");
    JsonArray identifiers = device.createNestedArray("identifiers");
    identifiers.add(deviceId);
    
    doc["availability_topic"] = deviceTopic + "/status";

    String payload;
    serializeJson(doc, payload);
    if (mqttClient.publish(configTopic.c_str(), payload.c_str(), true)) {
      Serial.println("✓ Timer switch discovery published");
    }
    delay(200);
  }

  // Speed number
  {
    String configTopic = discoveryPrefix + "/number/" + deviceId + "_speed/config";
    DynamicJsonDocument doc(1024);

    doc["name"] = String(mqttConfig.device_name) + " Speed";
    doc["unique_id"] = deviceId + "_speed";
    doc["command_topic"] = deviceTopic + "/speed/cmd";
    doc["state_topic"] = deviceTopic + "/state";
    doc["min"] = 10;
    doc["max"] = 200;
    doc["step"] = 5;
    doc["unit_of_measurement"] = "ms";
    doc["icon"] = "mdi:speedometer";
    doc["mode"] = "slider";
    doc["value_template"] = "{{ value_json.speed }}";
    
    JsonObject device = doc.createNestedObject("device");
    JsonArray identifiers = device.createNestedArray("identifiers");
    identifiers.add(deviceId);
    
    doc["availability_topic"] = deviceTopic + "/status";

    String payload;
    serializeJson(doc, payload);
    if (mqttClient.publish(configTopic.c_str(), payload.c_str(), true)) {
      Serial.println("✓ Speed number discovery published");
    }
    delay(200);
  }

  Serial.println("\n=== Discovery Complete ===\n");
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
  stateDoc["brightness"] = strip.getBrightness();
  stateDoc["color_mode"] = "rgb";
  stateDoc["effect"] = currentEffect;
  stateDoc["color"]["r"] = (effectColor >> 16) & 0xFF;
  stateDoc["color"]["g"] = (effectColor >> 8) & 0xFF;
  stateDoc["color"]["b"] = effectColor & 0xFF;

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
      strip.clear();
      strip.show();
    }
  }
  // Brightness control
  else if (topicStr.endsWith("/brightness/cmd")) {
    int brightness = atoi(message);
    brightness = constrain(brightness, 0, 100);
    strip.setBrightness((brightness * 255) / 100);
  }
  // Color control (RGB)
  else if (topicStr.endsWith("/color/cmd")) {
    DynamicJsonDocument doc(256);
    if (deserializeJson(doc, message) == DeserializationError::Ok) {
      uint8_t r = doc["r"] | 0;
      uint8_t g = doc["g"] | 0;
      uint8_t b = doc["b"] | 0;
      effectColor = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
      
      char colorStr[7];
      sprintf(colorStr, "%02X%02X%02X", r, g, b);
      Firebase.RTDB.setString(&fbdoUpload, (basePath + "/color").c_str(), colorStr);
    }
  }
  // Effect selection
  else if (topicStr.endsWith("/effect/cmd")) {
    int effect = atoi(message);
    effect = constrain(effect, 0, 32);
    currentEffect = effect;
    Firebase.RTDB.setInt(&fbdoUpload, (basePath + "/effect").c_str(), effect);
  }
  // Speed control
  else if (topicStr.endsWith("/speed/cmd")) {
    int speed = atoi(message);
    speed = constrain(speed, 10, 200);
    effectSpeed = speed;
    Firebase.RTDB.setInt(&fbdoUpload, (basePath + "/speed").c_str(), speed);
  }
  // Timer control
  else if (topicStr.endsWith("/timer/cmd")) {
    timerEnabled = strcmp(message, "ON") == 0;
    Firebase.RTDB.setBool(&fbdoUpload, (basePath + "/timer_enabled").c_str(), timerEnabled);
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