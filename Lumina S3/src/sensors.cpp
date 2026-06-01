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
  // By default the INA219 will be calibrated with a range of 32V, 2A.
  // However, you can change this with a different calibration code.
  // ina219.setCalibration_32V_1A();
  Serial.println("INA219 power monitor initialized successfully");
}

void setupInternalTempSensor() {
  temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
  temp_sensor.dac_offset = TSENS_DAC_L2; // -10℃ ~ 80℃, error < 1℃
  temp_sensor_set_config(temp_sensor);
  temp_sensor_start();
  Serial.println("Internal ESP32-S3 temperature sensor driver initialized");
}

// Update this section in sensors.cpp

void updateSensorData() {
  esp_task_wdt_reset();
  if (sensorAvailable) {
    float newLux = veml.readLux();
    
    // SANITY CHECK: The VEML7700 often returns 30,000+ or 65,000+ when it 
    // encounters an I2C error or sensor saturation from the LEDs.
    // We only accept values that make sense for an indoor environment.
    if (newLux >= 0 && newLux < 10000.0) {
      // Apply a smoothing filter (Low Pass Filter) 
      // This makes the transition 30% new data and 70% old data to prevent jumps
      currentLux = (newLux * 0.3) + (currentLux * 0.7);
    } else {
      // If the sensor goes "crazy", we ignore the reading and keep the last good value
      // Serial.printf("⚠️ Glitch detected (%.2f lux). Ignoring.\n", newLux);
    }
  }

  if (ina219Available) {
    currentVoltage = ina219.getBusVoltage_V();
    currentCurrent = ina219.getCurrent_mA();
    currentPower = ina219.getPower_mW();
  }

  // Read internal CPU temperature every 2000ms using the ESP-IDF driver
  static uint32_t lastTempUpdate = 0;
  if (millis() - lastTempUpdate >= 2000 || lastTempUpdate == 0) {
    lastTempUpdate = millis();
    float tsens_out;
    temp_sensor_read_celsius(&tsens_out);
    currentCpuTemp = tsens_out;
  }
}

// Improved logic to prevent "Optical Feedback" (LEDs turning themselves off)
bool shouldTurnOffDueToDarkness() {
  // Use a hysteresis buffer:
  // If LEDs are OFF, they turn ON at 'luxThreshold' (e.g., 1.0)
  // If LEDs are ON, they only turn OFF if lux is 'luxThreshold + 8.0'
  float effectiveThreshold = luxThreshold;

  if (stripEnabled) {
    // Adding 8.0 lux allows the LEDs to be bright without the sensor 
    // thinking the sun came out.
    effectiveThreshold = luxThreshold + 8.0; 
  }

  // Return true if it is dark enough to warrant LEDs being ON
  return currentLux < effectiveThreshold;
}

