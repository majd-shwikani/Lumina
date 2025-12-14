#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_VEML7700.h>
#include <driver/i2s.h>
#include <driver/adc.h>
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

// Microphone type selection
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
#define ANALOG_MIC_PIN 27  
#define ANALOG_READ_SAMPLES 64

// Audio FFT Configuration
#define SAMPLE_RATE 16000
#define N_SAMPLES 256
#define BUFFER_LEN N_SAMPLES
#define NUM_FREQ_BANDS 8

// ============================================================================
// AUTO-CALIBRATION CONFIGURATION
// ============================================================================

// Calibration parameters
#define CALIBRATION_DURATION_MS 3000  // 3 seconds of calibration
#define CALIBRATION_UPDATE_INTERVAL_MS 30000  // Update every 30 seconds
#define MIN_THRESHOLD_FLOOR 100.0
#define MAX_THRESHOLD_CEILING 50000.0

// Audio sensitivity and thresholds (managed by auto-calibration)
extern volatile float micSensitivity;          // Dynamic sensitivity (0.5 - 3.0)
extern volatile float frequencyThreshold;      // Dynamic threshold (auto-adjusted)
extern volatile float beatThreshold;           // Dynamic beat threshold (auto-adjusted)
extern volatile float bassBoost;               // Fixed bass boost (1.0 - 2.0)

// Noise floor and dynamic range tracking
extern volatile float noiseFloor;              // Automatically measured
extern volatile float dynamicRange;            // Automatically measured
extern volatile float peakLevel;               // Running peak level
extern volatile bool calibrationActive;        // Calibration state indicator

// ============================================================================
// FFT AND AUDIO BUFFERS
// ============================================================================

// I2S samples
extern int16_t raw_samples[BUFFER_LEN];

// FFT objects
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

// Smoothing and temporal analysis
extern double smoothedBandMagnitudes[NUM_FREQ_BANDS];
extern unsigned long lastAudioUpdate;
extern const unsigned long AUDIO_UPDATE_INTERVAL;

// NeoPixel reference
extern Adafruit_NeoPixel strip;

// ============================================================================
// FUNCTION DECLARATIONS - LIGHT SENSOR
// ============================================================================

void setupVEML7700();
void updateSensorData();
bool shouldTurnOffDueToDarkness();

// ============================================================================
// FUNCTION DECLARATIONS - AUDIO SENSOR SETUP
// ============================================================================

void setupFrequencyDetection();
void selectMicrophone(int micType);
void setupI2SMicrophone();
void setupAnalogMicrophone();

// ============================================================================
// FUNCTION DECLARATIONS - AUTO-CALIBRATION
// ============================================================================

void startAutoCalibration();
void updateAutoCalibration();
bool isCalibrationComplete();
void resetAutoCalibration();
float getMeasuredNoiseFloor();
float getMeasuredPeakLevel();

// ============================================================================
// FUNCTION DECLARATIONS - AUDIO PROCESSING
// ============================================================================

void updateFrequencyDetection();
void readI2SSamples();
void readAnalogSamples();
void analyzeAudioBands();
void detectBeat();
void normalizeAudioLevels();
void applyNoiseGate();
void smoothAudioData();

// ============================================================================
// FUNCTION DECLARATIONS - UTILITY
// ============================================================================

uint32_t frequencyToColor(double freq);
double getAudioLevelSmoothed(int index, double currentValue);
float constrainSensitivity(float value);
float constrainThreshold(float value);

#endif