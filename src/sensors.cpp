#include "sensors.h"

// ============================================================================
// LIGHT SENSOR OBJECTS AND VARIABLES
// ============================================================================
Adafruit_VEML7700 veml = Adafruit_VEML7700();
volatile float currentLux = 0;
volatile bool sensorAvailable = false;
volatile float luxThreshold = 1.0;

// ============================================================================
// AUDIO SENSOR OBJECTS AND VARIABLES
// ============================================================================

// Microphone Selection
volatile int activeMicrophone = MIC_I2S_ICS43434;

// Audio Sensitivity Parameters (adjustable via Firebase)
volatile float micSensitivity = 1.0;
volatile float frequencyThreshold = 500.0;
volatile float beatThreshold = 5000.0;
volatile float bassBoost = 1.2;

// FFT Objects and Buffers
int16_t raw_samples[BUFFER_LEN];
ArduinoFFT<double> FFT = ArduinoFFT<double>();
double vReal[N_SAMPLES];
double vImag[N_SAMPLES];

// Frequency Band Analysis
// REMOVED: const int NUM_FREQ_BANDS = 8; // Already defined in sensors.h
double bandMagnitudes[NUM_FREQ_BANDS] = {0};
double bandMaxima[NUM_FREQ_BANDS] = {0};
double frequencyResponse[N_SAMPLES] = {0};

// Audio Analysis Variables
volatile double detectedFrequency = 0;
volatile double frequencyMagnitude = 0;
volatile double globalAudioLevel = 0;
volatile double bassLevel = 0;
volatile double midLevel = 0;
volatile double trebleLevel = 0;
volatile bool beatDetected = false;
volatile float beatEnergy = 0;

// Audio Smoothing
double smoothedBandMagnitudes[NUM_FREQ_BANDS] = {0};
unsigned long lastAudioUpdate = 0;
const unsigned long AUDIO_UPDATE_INTERVAL = 20; // ms

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

void updateSensorData() {
  if (sensorAvailable) {
    currentLux = veml.readLux();
  }
}

bool shouldTurnOffDueToDarkness() {
  return currentLux < luxThreshold;
}

// ============================================================================
// I2S MICROPHONE (ICS43434) SETUP
// ============================================================================

void setupI2SMicrophone() {
  const i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = BUFFER_LEN,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  
  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = -1,
    .data_in_num = I2S_SD
  };
  
  esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("I2S driver install failed: %d\n", err);
    return;
  }
  
  err = i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("I2S set pin failed: %d\n", err);
    return;
  }
  
  Serial.println("I2S microphone (ICS43434) initialized successfully");
}

// ============================================================================
// ANALOG MICROPHONE (MAX9814) SETUP
// ============================================================================

void setupAnalogMicrophone() {
  analogSetAttenuation(ADC_11db);
  analogSetClockDiv(1);
  Serial.println("Analog microphone (MAX9814) initialized successfully");
}

// ============================================================================
// MICROPHONE SELECTION AND SETUP
// ============================================================================

void selectMicrophone(int micType) {
  activeMicrophone = micType;
  
  if (micType == MIC_I2S_ICS43434) {
    Serial.println("Switching to I2S microphone (ICS43434)");
    setupI2SMicrophone();
  } else if (micType == MIC_ANALOG_MAX9814) {
    Serial.println("Switching to Analog microphone (MAX9814)");
    setupAnalogMicrophone();
  }
}

void setupFrequencyDetection() {
  selectMicrophone(MIC_I2S_ICS43434); // Default to I2S microphone
}

// ============================================================================
// AUDIO DATA ACQUISITION
// ============================================================================

void readI2SSamples() {
  size_t bytes_read = 0;
  i2s_read(I2S_PORT, raw_samples, sizeof(raw_samples), &bytes_read, 0);
  
  if (bytes_read == sizeof(raw_samples)) {
    for (int i = 0; i < N_SAMPLES; i++) {
      vReal[i] = (double)raw_samples[i] * micSensitivity;
      vImag[i] = 0.0;
    }
  }
}

void readAnalogSamples() {
  for (int i = 0; i < N_SAMPLES; i++) {
    int rawValue = analogRead(ANALOG_MIC_PIN);
    // Convert 0-4095 ADC range to signed 16-bit equivalent
    int16_t sample = (int16_t)((rawValue - 2048) * 16) * micSensitivity;
    vReal[i] = (double)sample;
    vImag[i] = 0.0;
  }
}

// ============================================================================
// FFT AND FREQUENCY ANALYSIS
// ============================================================================

void updateFrequencyDetection() {
  // Read samples based on active microphone
  if (activeMicrophone == MIC_I2S_ICS43434) {
    readI2SSamples();
  } else {
    readAnalogSamples();
  }
  
  // Perform FFT
  FFT.compute(vReal, vImag, N_SAMPLES, FFT_FORWARD);
  FFT.complexToMagnitude(vReal, vImag, N_SAMPLES);
  
  // Store frequency response
  for (int i = 0; i < N_SAMPLES / 2; i++) {
    frequencyResponse[i] = vReal[i];
  }
  
  // Analyze frequency bands
  analyzeAudioBands();
  
  // Detect beats
  detectBeat();
  
  // Normalize levels
  normalizeAudioLevels();
}

// ============================================================================
// FREQUENCY BAND ANALYSIS
// ============================================================================

void analyzeAudioBands() {
  // Divide spectrum into 8 frequency bands
  int samplesPerBand = (N_SAMPLES / 2) / NUM_FREQ_BANDS;
  
  // Reset band magnitudes
  for (int band = 0; band < NUM_FREQ_BANDS; band++) {
    bandMagnitudes[band] = 0.0;
    
    // Apply frequency-specific boost
    double bandBoost = 1.0;
    if (band < 2) bandBoost = bassBoost;  // Boost bass frequencies
    
    int startBin = band * samplesPerBand;
    int endBin = startBin + samplesPerBand;
    
    // Find maximum magnitude in this band
    for (int bin = startBin; bin < endBin && bin < N_SAMPLES / 2; bin++) {
      double magnitude = vReal[bin] * bandBoost;
      if (magnitude > bandMagnitudes[band]) {
        bandMagnitudes[band] = magnitude;
      }
    }
    
    // Update peak maximum with decay
    if (bandMagnitudes[band] > bandMaxima[band]) {
      bandMaxima[band] = bandMagnitudes[band];
    } else {
      bandMaxima[band] *= 0.95;  // Decay peak over time
    }
    
    // Smooth the magnitude values
    smoothedBandMagnitudes[band] = smoothedBandMagnitudes[band] * 0.7 + bandMagnitudes[band] * 0.3;
  }
  
  // Calculate frequency-range specific levels
  bassLevel = (bandMagnitudes[0] + bandMagnitudes[1]) / 2.0;
  midLevel = (bandMagnitudes[2] + bandMagnitudes[3] + bandMagnitudes[4]) / 3.0;
  trebleLevel = (bandMagnitudes[5] + bandMagnitudes[6] + bandMagnitudes[7]) / 3.0;
  
  // Calculate global audio level
  globalAudioLevel = 0;
  for (int band = 0; band < NUM_FREQ_BANDS; band++) {
    globalAudioLevel += smoothedBandMagnitudes[band];
  }
  globalAudioLevel /= NUM_FREQ_BANDS;
  globalAudioLevel *= micSensitivity;
}

// ============================================================================
// BEAT DETECTION
// ============================================================================

void detectBeat() {
  // Simple beat detection based on bass energy sudden increase
  static double previousBassLevel = 0;
  static unsigned long lastBeatTime = 0;
  
  double bassIncrease = bassLevel - previousBassLevel;
  previousBassLevel = bassLevel;
  
  beatDetected = false;
  beatEnergy = 0;
  
  // Detect beat if bass increased significantly and enough time has passed
  if (bassIncrease > beatThreshold && (millis() - lastBeatTime) > 100) {
    beatDetected = true;
    beatEnergy = bassIncrease / (beatThreshold * 2.0);
    if (beatEnergy > 1.0) beatEnergy = 1.0;
    lastBeatTime = millis();
  }
}

// ============================================================================
// AUDIO LEVEL NORMALIZATION
// ============================================================================

void normalizeAudioLevels() {
  // Normalize band magnitudes to 0-1 range based on threshold
  for (int band = 0; band < NUM_FREQ_BANDS; band++) {
    double normalized = bandMagnitudes[band] / frequencyThreshold;
    if (normalized > 1.0) normalized = 1.0;
    bandMagnitudes[band] = normalized;
  }
  
  // Normalize bass, mid, treble levels
  bassLevel = bassLevel / frequencyThreshold;
  if (bassLevel > 1.0) bassLevel = 1.0;
  
  midLevel = midLevel / frequencyThreshold;
  if (midLevel > 1.0) midLevel = 1.0;
  
  trebleLevel = trebleLevel / frequencyThreshold;
  if (trebleLevel > 1.0) trebleLevel = 1.0;
  
  // Normalize global level
  globalAudioLevel = globalAudioLevel / frequencyThreshold;
  if (globalAudioLevel > 1.0) globalAudioLevel = 1.0;
}

// ============================================================================
// COLOR CONVERSION UTILITIES
// ============================================================================

uint32_t frequencyToColor(double freq) {
  // Map frequency (0-8000Hz typical) to hue (0-360)
  double normalizedFreq = freq / 8000.0;
  if (normalizedFreq > 1.0) normalizedFreq = 1.0;
  
  uint8_t hue = (uint8_t)(normalizedFreq * 255);
  return strip.ColorHSV(hue * 256, 255, 255);
}

double getAudioLevelSmoothed(int index, double currentValue) {
  // Returns smoothed audio level with exponential moving average
  static double smoothed[NUM_FREQ_BANDS] = {0};
  if (index < NUM_FREQ_BANDS) {
    smoothed[index] = smoothed[index] * 0.6 + currentValue * 0.4;
    return smoothed[index];
  }
  return currentValue;
}