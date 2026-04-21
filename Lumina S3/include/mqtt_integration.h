// ============================================================================
// mqtt_integration.h
// ============================================================================

#ifndef MQTT_INTEGRATION_H
#define MQTT_INTEGRATION_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <Firebase_ESP_Client.h>
#include <FastLED.h>

// Forward declarations for external globals
extern volatile int currentEffect;
extern volatile uint32_t effectSpeed;
extern volatile uint32_t effectColor;
extern volatile bool stripEnabled;
extern volatile bool firebaseConnected;
extern bool timerEnabled;
extern String basePath;
extern String deviceID;
extern FirebaseData fbdoUpload;
extern CRGB *leds;
extern volatile bool sensorAvailable;
extern volatile double globalAudioLevel;
extern volatile float currentLux;

// NEW: Timer and automation parameters
extern char timerOnTime[6];
extern char timerOffTime[6];
extern volatile bool autoDarknessControl;
extern volatile float luxThreshold;
extern volatile bool turnedOffByDarkness;
extern volatile bool triggerMicCalibration;

// NEW: Define Effect Names Array
extern const char* EFFECT_NAMES[];
extern const int NUM_EFFECTS;

// MQTT Configuration Structure
typedef struct {
  char broker_address[256];
  uint16_t broker_port;
  char username[128];
  char password[128];
  bool enabled;
  char device_name[64];
} MQTTConfig;

// Function Declarations
void setupMQTT();
void mqttConnectAndSubscribe();
void mqttPublishState();
void mqttPublishSensorData();
void publishHomeAssistantDiscovery();
void mqttCallback(char* topic, byte* payload, unsigned int length);
bool connectToMQTT();
void mqttTask(void *parameter);

// Global MQTT objects
extern WiFiClient espClient;
extern PubSubClient mqttClient;
extern MQTTConfig mqttConfig;
extern volatile bool mqttConnected;
extern String deviceTopic;

#endif