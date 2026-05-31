#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_VEML7700.h>
#include <Adafruit_INA219.h>
#include <driver/i2s.h>
#include <driver/temp_sensor.h> // ESP-IDF Temperature Sensor Driver
#include "esp_dsp.h"
#include <FastLED.h>
#include "config.h"

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

// ============================================================================
// AUDIO SENSOR CONFIGURATION
// ============================================================================

// Microphone type selection
#define MIC_I2S_ICS43434 0
#define MIC_ANALOG_MAX9814 1

// Active microphone selection
extern volatile int activeMicrophone;


// Audio FFT Configuration
#define SAMPLING_FREQ 44100
#define FFT_SAMPLES 512
#define NUM_FREQ_BANDS 16
#define BIN_WIDTH ((float)SAMPLING_FREQ / FFT_SAMPLES)

// ============================================================================
// NOISE FLOOR (continuously adaptive, no explicit calibration)
// ============================================================================

extern volatile float noiseFloor;
extern float windowCoefficients[FFT_SAMPLES];
extern float prevFluxMag[FFT_SAMPLES / 2];
extern int binToBand[FFT_SAMPLES / 2];

// Frequency Band Analysis
extern volatile float bandMagnitudes[NUM_FREQ_BANDS];
extern volatile float smoothedBandMagnitudes[NUM_FREQ_BANDS];
extern volatile float bandPeak[NUM_FREQ_BANDS];

// Audio Analysis Variables
extern volatile float globalAudioLevel; // Compressed — avg of normalized bands (for reporting)
extern volatile float audioVolume;       // Raw peak/AGC ratio — full dynamic range (for effects)
extern volatile float bassLevel;
extern volatile float midLevel;
extern volatile float trebleLevel;
extern volatile bool beatDetected;
extern volatile float beatEnergy;
extern volatile float spectralCentroid;
extern volatile float spectralFlux;

// Multi-band onset detection
extern volatile bool onsetMid;
extern volatile bool onsetHigh;

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

// Audio Sensor Setup
void setupFrequencyDetection();
void selectMicrophone(int micType);

// Audio Processing Functions
void initBandMapping();

// Audio DSP Task (runs on Core 0, dynamic lifecycle)
void audioProcessingTask(void *pvParameters);

// Utility Functions
uint32_t frequencyToColor(float freq);

#endif