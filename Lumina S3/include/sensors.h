#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_VEML7700.h>
#include <Adafruit_INA219.h>
#include <driver/temp_sensor.h> // ESP-IDF Temperature Sensor Driver
#include <FastLED.h>

// ============================================================================
// LIGHT SENSOR CONFIGURATION
// ============================================================================
extern Adafruit_VEML7700 veml;
extern volatile float currentLux;
extern volatile float currentCpuTemp;
extern volatile bool sensorAvailable;
extern volatile float luxThreshold;

// ============================================================================
// POWER MONITORING CONFIGURATION (INA219)
// ============================================================================
extern Adafruit_INA219 ina219;
extern volatile float currentVoltage;
extern volatile float currentCurrent;
extern volatile float currentPower;
extern volatile bool ina219Available;



// FastLED reference
extern CRGB *leds;
extern volatile bool stripEnabled;


// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

// Light Sensor Functions
void setupVEML7700();
void setupINA219();
void setupInternalTempSensor();
void updateSensorData();
bool shouldTurnOffDueToDarkness();

#endif