#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_VEML7700.h>
#include <Adafruit_INA219.h>
#include <driver/i2s.h>
#include <arduinoFFT.h>
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
#define SAMPLE_RATE 16000
#define N_SAMPLES 256
#define BUFFER_LEN N_SAMPLES
#define NUM_FREQ_BANDS 8

// ============================================================================
// AUTO-CALIBRATION CONFIGURATION
// ============================================================================

// Calibration constants
#define CALIBRATION_DURATION 3000        // 3 seconds of calibration
#define NOISE_FLOOR_MULTIPLIER 1.5       // Noise floor * this = detection threshold (lowered for sensitivity)
#define TARGET_PEAK_LEVEL 0.8             // Target peak magnitude (0-1 scale) - aim higher
#define GAIN_ADJUSTMENT_RATE 0.05         // Rate of gain adjustment per cycle (faster adaptation)
#define INITIAL_GAIN_MULTIPLIER 5.0       // Start with higher gain (instead of 1.0)

// Manual calibration trigger
extern volatile bool triggerMicCalibration;
extern volatile bool isCalibrating;

// Microphone calibration state
extern volatile bool calibrationComplete;
extern volatile double noiseFloor;
extern volatile double gainMultiplier;
extern unsigned long lastCalibrationTime;

// FFT Variables
extern int16_t raw_samples[BUFFER_LEN];
extern ArduinoFFT<double> FFT;
extern double vReal[N_SAMPLES];
extern double vImag[N_SAMPLES];

// Frequency Band Analysis
extern double bandMagnitudes[NUM_FREQ_BANDS];
extern double bandMaxima[NUM_FREQ_BANDS];
extern double frequencyResponse[N_SAMPLES];

// Audio Analysis Variables
extern volatile double detectedFrequency;
extern volatile double frequencyMagnitude;
extern volatile double globalAudioLevel;
extern volatile double bassLevel;
extern volatile double midLevel;
extern volatile double trebleLevel;
extern volatile bool beatDetected;
extern volatile float beatEnergy;

// Smoothing variables
extern double smoothedBandMagnitudes[NUM_FREQ_BANDS];

// FastLED reference
extern CRGB *leds;
extern volatile bool stripEnabled;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

// Light Sensor Functions
void setupVEML7700();
void setupINA219();
void updateSensorData();
bool shouldTurnOffDueToDarkness();

// Audio Sensor Setup
void setupFrequencyDetection();
void selectMicrophone(int micType);

// Audio Processing Functions
void updateFrequencyDetection();
void analyzeAudioBands();
void detectBeat();
void normalizeAudioLevels();

// Auto-calibration Functions
void calibrateMicrophone();
void performAutoGainControl();
void checkAndRecalibrate();
void saveMicCalibration();
bool loadMicCalibration();

// Utility Functions
uint32_t frequencyToColor(double freq);
double getAudioLevelSmoothed(int index, double currentValue);

#endif