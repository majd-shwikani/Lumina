#ifndef MQTT_INTEGRATION_H
#define MQTT_INTEGRATION_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

struct MQTTConfig {
  char broker_address[64];
  int broker_port;
  char username[32];
  char password[32];
  bool enabled;
  char device_name[32];
};

extern MQTTConfig mqttConfig;
extern volatile bool mqttConnected;

bool loadMQTTConfig();
void saveMQTTConfig();
void updateMQTTConfigFromFirebase();
void setupMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void mqttConnectAndSubscribe();
void mqttPublishState();
void mqttPublishSensorData();
void publishHomeAssistantDiscovery();
void handleMQTT(); // Added

#endif
