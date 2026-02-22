#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_VEML7700.h>
#include <driver/i2s.h>
#include <arduinoFFT.h>

// I2S Microphone Defines (ICS-43434)
#ifndef I2S_SCK_PIN
#define I2S_SCK_PIN 14
#endif

#ifndef I2S_WS_PIN
#define I2S_WS_PIN  15
#endif

#ifndef I2S_SD_PIN
#define I2S_SD_PIN  32
#endif

#define I2S_PORT I2S_NUM_0

// Audio Processing Defines
#define SAMPLE_RATE 16000
#define N_SAMPLES 512
#define BUFFER_LEN 512
#define I2S_DMA_BUF_LEN 512

// Analog Microphone Defines (MAX9814)
#ifndef ANALOG_MIC_PIN
#define ANALOG_MIC_PIN 36 // VP pin on many ESP32 boards
#endif

// Microphone Selection
#define MIC_I2S_ICS43434 0
#define MIC_ANALOG_MAX9814 1

// Calibration & Gain
#define CALIBRATION_DURATION 3000
#define TARGET_PEAK_LEVEL 1000.0
#define INITIAL_GAIN_MULTIPLIER 1.0
#define GAIN_ADJUSTMENT_RATE 0.05
#define NOISE_FLOOR_MULTIPLIER 1.2

// Freq Bands
#define NUM_FREQ_BANDS 8

extern volatile int activeMicrophone;
extern volatile double noiseFloor;
extern volatile double gainMultiplier;
extern volatile bool calibrationComplete;
extern volatile bool triggerMicCalibration;
extern volatile bool isCalibrating;

// Audio levels
extern volatile double globalAudioLevel;
extern volatile double bassLevel;
extern volatile double midLevel;
extern volatile double trebleLevel;
extern volatile bool beatDetected;
extern volatile float beatEnergy;

extern volatile float currentLux;
extern volatile bool sensorAvailable;
extern volatile float luxThreshold;

void setupVEML7700();
void updateSensorData();
bool shouldTurnOffDueToDarkness();

void setupFrequencyDetection();
void updateFrequencyDetection();
uint32_t frequencyToColor(double freq);
double getAudioLevelSmoothed(int index, double currentValue);

void selectMicrophone(int micType);
void calibrateMicrophone();
void saveMicCalibration();
bool loadMicCalibration();

// NEW:
void handleSensorsAndAutomation();

// Helpers
void analyzeAudioBands();
void detectBeat();
void normalizeAudioLevels();
void performAutoGainControl();
void checkAndRecalibrate();

#endif
