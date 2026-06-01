#include "sensors.h"
#include "globals.h"
#include <esp_task_wdt.h>

// ============================================================================
// LIGHT SENSOR OBJECTS AND VARIABLES
// ============================================================================
Adafruit_VEML7700 veml = Adafruit_VEML7700();
volatile float currentLux = 0;
volatile float currentCpuTemp = 0;
volatile bool sensorAvailable = false;
volatile float luxThreshold = 1.0;

// ============================================================================
// POWER MONITORING OBJECTS AND VARIABLES (INA219)
// ============================================================================
Adafruit_INA219 ina219;
volatile float currentVoltage = 0;
volatile float currentCurrent = 0;
volatile float currentPower = 0;
volatile bool ina219Available = false;


// ============================================================================
// LIGHT SENSOR INITIALIZATION
// ============================================================================

void setupVEML7700() {
  if (!veml.begin()) {
    Serial.println("VEML7700 sensor not found, continuing without light sensor");
    sensorAvailable = false;
    return;
  }
  
  sensorAvailable = true;
  veml.setGain(VEML7700_GAIN_1_8);
  veml.setIntegrationTime(VEML7700_IT_100MS);
  Serial.println("VEML7700 light sensor initialized successfully");
}

void setupINA219() {
  if (!ina219.begin()) {
    Serial.println("INA219 sensor not found, continuing without power monitor");
    ina219Available = false;
    return;
  }
  
  ina219Available = true;
  Serial.println("INA219 power monitor initialized successfully");
}

void setupInternalTempSensor() {
  temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
  temp_sensor.dac_offset = TSENS_DAC_L2;
  temp_sensor_set_config(temp_sensor);
  temp_sensor_start();
  Serial.println("Internal ESP32-S3 temperature sensor driver initialized");
}

void updateSensorData() {
  esp_task_wdt_reset();
  if (sensorAvailable) {
    float newLux = veml.readLux();
    if (newLux >= 0 && newLux < 10000.0) {
      currentLux = (newLux * 0.3f) + (currentLux * 0.7f);
    }
  }

  if (ina219Available) {
    currentVoltage = ina219.getBusVoltage_V();
    currentCurrent = ina219.getCurrent_mA();
    currentPower = ina219.getPower_mW();
  }

  static uint32_t lastTempUpdate = 0;
  if (millis() - lastTempUpdate >= 2000 || lastTempUpdate == 0) {
    lastTempUpdate = millis();
    float tsens_out;
    temp_sensor_read_celsius(&tsens_out);
    currentCpuTemp = tsens_out;
  }
}

bool shouldTurnOffDueToDarkness() {
  float effectiveThreshold = luxThreshold;
  if (stripEnabled) {
    effectiveThreshold = luxThreshold + 8.0f;
  }
  return currentLux < effectiveThreshold;
}

