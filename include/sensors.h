#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_VEML7700.h>
#include <driver/i2s.h>
#include <arduinoFFT.h>
#include <Adafruit_NeoPixel.h>

// ============================================================================
// LIGHT SENSOR CONFIGURATION
// ============================================================================
extern Adafruit_VEML7700 veml;
extern volatile float currentLux;
extern volatile bool sensorAvailable;
extern volatile float luxThreshold;

// ============================================================================
// AUDIO SENSOR CONFIGURATION
// ============================================================================

// Microphone type selection - Use int instead of enum for Firebase
#define MIC_I2S_ICS43434 0
#define MIC_ANALOG_MAX9814 1

// Active microphone selection
extern volatile int activeMicrophone;

// I2S Configuration (for ICS43434)
#define I2S_WS   25
#define I2S_SD   32  
#define I2S_SCK  33
#define I2S_PORT I2S_NUM_0

// Analog Configuration (for MAX9814)
#define ANALOG_MIC_PIN 27  // ADC pin for MAX9814

// Audio FFT Configuration
#define SAMPLE_RATE 16000
#define N_SAMPLES 256
#define BUFFER_LEN N_SAMPLES
#define NUM_FREQ_BANDS 8

// Audio Sensitivity Adjustment (adjustable via Firebase)
extern volatile float micSensitivity;
extern volatile float frequencyThreshold;
extern volatile float beatThreshold;
extern volatile float bassBoost;

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

// NeoPixel reference
extern Adafruit_NeoPixel strip;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

// Light Sensor Functions
void setupVEML7700();
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

// Utility Functions
uint32_t frequencyToColor(double freq);
double getAudioLevelSmoothed(int index, double currentValue);

#endif