#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_VEML7700.h>
#include <driver/i2s.h>
#include <arduinoFFT.h>
#include <Adafruit_NeoPixel.h> // **ADD THIS LINE**

// Light sensor configuration
extern Adafruit_VEML7700 veml;
extern volatile float currentLux;
extern volatile bool sensorAvailable;
extern volatile float luxThreshold;

// Frequency detection configuration
#define I2S_WS   25
#define I2S_SD   32  
#define I2S_SCK  33
#define I2S_PORT I2S_NUM_0

#define SAMPLE_RATE 16000
#define N_SAMPLES 256
#define BUFFER_LEN N_SAMPLES

// Frequency detection variables
extern int16_t raw_samples[BUFFER_LEN];
extern ArduinoFFT<double> FFT;
extern double vReal[N_SAMPLES];
extern double vImag[N_SAMPLES];

extern const int NUM_FREQ_BANDS;
extern double bandMagnitudes[];
extern double frequencyThreshold;

extern volatile double detectedFrequency;
extern volatile double frequencyMagnitude;

extern Adafruit_NeoPixel strip;

// Function declarations
void setupVEML7700();
void updateSensorData();
bool shouldTurnOffDueToDarkness();
void setupFrequencyDetection();
void updateFrequencyDetection();
uint32_t frequencyToColor(double freq, uint32_t baseColor = 0xFF0000);

#endif