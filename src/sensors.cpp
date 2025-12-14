#include "sensors.h"
#include <climits>
#include <cfloat>

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

// Audio Sensitivity Parameters (auto-adjusted)
volatile float micSensitivity = 1.0;
volatile float frequencyThreshold = 500.0;
volatile float beatThreshold = 5000.0;
volatile float bassBoost = 1.2;

// Noise floor and dynamic range tracking
volatile float noiseFloor = 0.0;
volatile float dynamicRange = 0.0;
volatile float peakLevel = 0.0;
volatile bool calibrationActive = false;

// Calibration timing
static unsigned long calibrationStartTime = 0;
static unsigned long lastCalibrationUpdate = 0;
static float maxPeakLevel = 0.0;
static float minPeakLevel = 1e10;
static int calibrationSampleCount = 0;

// FFT Objects and Buffers
int16_t raw_samples[BUFFER_LEN];
ArduinoFFT<double> FFT = ArduinoFFT<double>();
double vReal[N_SAMPLES];
double vImag[N_SAMPLES];

// Frequency Band Analysis
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

// Noise gate and dynamic processing
static double lastBandMagnitudes[NUM_FREQ_BANDS] = {0};
static const double MAGNITUDE_SMOOTHING = 0.4;
static const double NOISE_GATE_THRESHOLD_MULTIPLIER = 0.15;

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
  
  // Stop any existing I2S driver
  i2s_driver_uninstall(I2S_PORT);
  delay(100);
  
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
  // Configure ADC1 for GPIO36 (VP pin)
  // Use Arduino's analogRead which handles all the configuration
  // Configure the ADC pin
  pinMode(ANALOG_MIC_PIN, INPUT);
  
  // Set ADC resolution to 12-bit
  analogReadResolution(12);
  
  // Read once to initialize
  analogRead(ANALOG_MIC_PIN);
  
  Serial.println("Analog microphone (MAX9814) on GPIO36 (VP) initialized successfully");
  Serial.println("Note: Ensure MAX9814 is powered at 3V3 and GND is connected");
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
    Serial.println("Switching to Analog microphone (MAX9814) on GPIO36");
    setupAnalogMicrophone();
  }
}

void setupFrequencyDetection() {
  selectMicrophone(MIC_I2S_ICS43434); // Default to I2S microphone
  
  // Initialize calibration variables
  resetAutoCalibration();
  
  Serial.println("Frequency detection setup complete");
}

// ============================================================================
// AUTO-CALIBRATION FUNCTIONS
// ============================================================================

void startAutoCalibration() {
  Serial.println("Starting auto-calibration...");
  calibrationActive = true;
  calibrationStartTime = millis();
  lastCalibrationUpdate = millis();
  maxPeakLevel = 0.0;
  minPeakLevel = 1e10;
  calibrationSampleCount = 0;
  
  // Reset thresholds to defaults during calibration
  noiseFloor = 0.0;
  peakLevel = 0.0;
}

void resetAutoCalibration() {
  calibrationActive = false;
  calibrationStartTime = 0;
  lastCalibrationUpdate = 0;
  maxPeakLevel = 0.0;
  minPeakLevel = 1e10;
  calibrationSampleCount = 0;
  noiseFloor = 100.0;
  peakLevel = 0.0;
  dynamicRange = 1000.0;
  micSensitivity = 1.0;
  frequencyThreshold = 500.0;
  beatThreshold = 5000.0;
}

bool isCalibrationComplete() {
  return (millis() - calibrationStartTime) >= CALIBRATION_DURATION_MS;
}

float getMeasuredNoiseFloor() {
  return noiseFloor;
}

float getMeasuredPeakLevel() {
  return peakLevel;
}

void updateAutoCalibration() {
  if (!calibrationActive) {
    return;
  }
  
  // Update peak levels during calibration
  if (globalAudioLevel > maxPeakLevel) {
    maxPeakLevel = globalAudioLevel;
  }
  if (globalAudioLevel < minPeakLevel && globalAudioLevel > 0) {
    minPeakLevel = globalAudioLevel;
  }
  calibrationSampleCount++;
  
  // Finalize calibration
  if (isCalibrationComplete()) {
    // Calculate noise floor from minimum peaks
    if (minPeakLevel != 1e10 && minPeakLevel > 0) {
      noiseFloor = minPeakLevel * 0.8;
    } else {
      noiseFloor = 50.0;
    }
    
    // Calculate dynamic range
    if (maxPeakLevel > noiseFloor) {
      dynamicRange = maxPeakLevel - noiseFloor;
    } else {
      dynamicRange = 1000.0;
    }
    
    // Adjust sensitivity based on measured levels
    if (maxPeakLevel > 0) {
      micSensitivity = constrainSensitivity(10000.0 / maxPeakLevel);
    }
    
    // Set frequency threshold to 20% above noise floor
    frequencyThreshold = constrainThreshold(noiseFloor * 1.5);
    
    // Set beat threshold to 30% of dynamic range above noise floor
    beatThreshold = constrainThreshold(noiseFloor + (dynamicRange * 0.3));
    
    Serial.println("\n=== Auto-Calibration Complete ===");
    Serial.printf("Noise Floor: %.2f\n", noiseFloor);
    Serial.printf("Peak Level: %.2f\n", maxPeakLevel);
    Serial.printf("Dynamic Range: %.2f\n", dynamicRange);
    Serial.printf("Mic Sensitivity: %.3f\n", micSensitivity);
    Serial.printf("Frequency Threshold: %.2f\n", frequencyThreshold);
    Serial.printf("Beat Threshold: %.2f\n", beatThreshold);
    Serial.println("===================================\n");
    
    calibrationActive = false;
    lastCalibrationUpdate = millis();
  }
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
  // Read multiple samples and average to reduce noise
  uint32_t sum = 0;
  for (int i = 0; i < ANALOG_READ_SAMPLES; i++) {
    sum += analogRead(ANALOG_MIC_PIN);
  }
  uint16_t avgValue = sum / ANALOG_READ_SAMPLES;
  
  // Convert from 12-bit ADC (0-4095) to signed value
  // Center at 2048 (midpoint of 12-bit range)
  int16_t centeredValue = (int16_t)(avgValue - 2048);
  
  // Scale to approximate 16-bit range
  int16_t scaledSample = centeredValue * 8;
  
  // Fill sample buffer with the same value
  // (In a real implementation, you'd continuously read new samples)
  for (int i = 0; i < N_SAMPLES; i++) {
    vReal[i] = (double)scaledSample * micSensitivity;
    vImag[i] = 0.0;
  }
}

// ============================================================================
// NOISE GATE AND FILTERING
// ============================================================================

void applyNoiseGate() {
  float noiseGateThreshold = noiseFloor * NOISE_GATE_THRESHOLD_MULTIPLIER;
  
  for (int band = 0; band < NUM_FREQ_BANDS; band++) {
    // Apply noise gate - suppress very quiet signals
    if (bandMagnitudes[band] < noiseGateThreshold) {
      bandMagnitudes[band] = 0.0;
    }
    
    // Apply high-pass filtering - suppress very low frequency noise
    if (band == 0 && bandMagnitudes[band] < noiseFloor * 0.5) {
      bandMagnitudes[band] *= 0.5;
    }
  }
}

void smoothAudioData() {
  for (int band = 0; band < NUM_FREQ_BANDS; band++) {
    // Apply exponential moving average for smooth transitions
    bandMagnitudes[band] = lastBandMagnitudes[band] * (1.0 - MAGNITUDE_SMOOTHING) + 
                           bandMagnitudes[band] * MAGNITUDE_SMOOTHING;
    lastBandMagnitudes[band] = bandMagnitudes[band];
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
  
  // Apply noise gate
  applyNoiseGate();
  
  // Smooth audio data
  smoothAudioData();
  
  // Detect beats
  detectBeat();
  
  // Normalize levels
  normalizeAudioLevels();
  
  // Update auto-calibration if active
  if (calibrationActive) {
    updateAutoCalibration();
  }
}

// ============================================================================
// FREQUENCY BAND ANALYSIS
// ============================================================================

void analyzeAudioBands() {
  int samplesPerBand = (N_SAMPLES / 2) / NUM_FREQ_BANDS;
  
  for (int band = 0; band < NUM_FREQ_BANDS; band++) {
    bandMagnitudes[band] = 0.0;
    
    // Apply frequency-specific boost
    double bandBoost = 1.0;
    if (band < 2) bandBoost = bassBoost;
    
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
      bandMaxima[band] *= 0.95;
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
  
  // Update running peak level
  if (globalAudioLevel > peakLevel) {
    peakLevel = globalAudioLevel;
  } else {
    peakLevel *= 0.99;  // Slow decay of peak level
  }
}

// ============================================================================
// BEAT DETECTION
// ============================================================================

void detectBeat() {
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
    if (normalized < 0.0) normalized = 0.0;
    bandMagnitudes[band] = normalized;
  }
  
  // Normalize bass, mid, treble levels
  bassLevel = bassLevel / frequencyThreshold;
  if (bassLevel > 1.0) bassLevel = 1.0;
  if (bassLevel < 0.0) bassLevel = 0.0;
  
  midLevel = midLevel / frequencyThreshold;
  if (midLevel > 1.0) midLevel = 1.0;
  if (midLevel < 0.0) midLevel = 0.0;
  
  trebleLevel = trebleLevel / frequencyThreshold;
  if (trebleLevel > 1.0) trebleLevel = 1.0;
  if (trebleLevel < 0.0) trebleLevel = 0.0;
  
  // Normalize global level
  globalAudioLevel = globalAudioLevel / frequencyThreshold;
  if (globalAudioLevel > 1.0) globalAudioLevel = 1.0;
  if (globalAudioLevel < 0.0) globalAudioLevel = 0.0;
}

// ============================================================================
// CONSTRAINT AND UTILITY FUNCTIONS
// ============================================================================

float constrainSensitivity(float value) {
  if (value < 0.5) return 0.5;
  if (value > 3.0) return 3.0;
  return value;
}

float constrainThreshold(float value) {
  if (value < MIN_THRESHOLD_FLOOR) return MIN_THRESHOLD_FLOOR;
  if (value > MAX_THRESHOLD_CEILING) return MAX_THRESHOLD_CEILING;
  return value;
}

// ============================================================================
// COLOR CONVERSION UTILITIES
// ============================================================================

uint32_t frequencyToColor(double freq) {
  double normalizedFreq = freq / 8000.0;
  if (normalizedFreq > 1.0) normalizedFreq = 1.0;
  
  uint8_t hue = (uint8_t)(normalizedFreq * 255);
  return strip.ColorHSV(hue * 256, 255, 255);
}

double getAudioLevelSmoothed(int index, double currentValue) {
  static double smoothed[NUM_FREQ_BANDS] = {0};
  if (index < NUM_FREQ_BANDS) {
    smoothed[index] = smoothed[index] * 0.6 + currentValue * 0.4;
    return smoothed[index];
  }
  return currentValue;
}