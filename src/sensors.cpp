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

// ============================================================================
// AUTO-CALIBRATION AND GAIN CONTROL VARIABLES
// ============================================================================

volatile bool calibrationComplete = false;
volatile double noiseFloor = 100.0;
volatile double gainMultiplier = INITIAL_GAIN_MULTIPLIER;
unsigned long lastCalibrationTime = 0;

// Manual calibration trigger from Firebase
volatile bool triggerMicCalibration = false;
volatile bool isCalibrating = false;

// Calibration buffers
double calibrationSamples[256] = {0};
int calibrationSampleCount = 0;
unsigned long calibrationStartTime = 0;

// ============================================================================
// FFT OBJECTS AND BUFFERS
// ============================================================================

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
const unsigned long AUDIO_UPDATE_INTERVAL = 20;

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
  selectMicrophone(MIC_I2S_ICS43434);
}

// ============================================================================
// AUDIO DATA ACQUISITION
// ============================================================================

void readI2SSamples() {
  size_t bytes_read = 0;
  i2s_read(I2S_PORT, raw_samples, sizeof(raw_samples), &bytes_read, 0);
  
  if (bytes_read == sizeof(raw_samples)) {
    for (int i = 0; i < N_SAMPLES; i++) {
      vReal[i] = (double)raw_samples[i];
      vImag[i] = 0.0;
    }
  }
}

void readAnalogSamples() {
  for (int i = 0; i < N_SAMPLES; i++) {
    int rawValue = analogRead(ANALOG_MIC_PIN);
    int16_t sample = (int16_t)((rawValue - 2048) * 16);
    vReal[i] = (double)sample;
    vImag[i] = 0.0;
  }
}

// ============================================================================
// MICROPHONE CALIBRATION FUNCTIONS
// ============================================================================

void calibrateMicrophone() {
  if (calibrationStartTime == 0) {
    calibrationStartTime = millis();
    isCalibrating = true;
    Serial.println("\n========== MICROPHONE CALIBRATION STARTED ==========");
    Serial.println("Keep the environment quiet for the next 3 seconds...");
  }
  
  if (millis() - calibrationStartTime < CALIBRATION_DURATION) {
    if (activeMicrophone == MIC_I2S_ICS43434) {
      readI2SSamples();
    } else {
      readAnalogSamples();
    }
    
    for (int i = 0; i < N_SAMPLES; i++) {
      vReal[i] *= gainMultiplier;
      vImag[i] = 0.0;
    }
    
    FFT.compute(vReal, vImag, N_SAMPLES, FFT_FORWARD);
    FFT.complexToMagnitude(vReal, vImag, N_SAMPLES);
    
    double peakMagnitude = 0;
    for (int i = 1; i < N_SAMPLES / 2; i++) {
      if (vReal[i] > peakMagnitude) {
        peakMagnitude = vReal[i];
      }
    }
    
    if (calibrationSampleCount < 256) {
      calibrationSamples[calibrationSampleCount] = peakMagnitude;
      calibrationSampleCount++;
    }
    
    if (calibrationSampleCount % 32 == 0) {
      Serial.printf("Calibration: %d/256 samples (Gain: %.2f, Peak: %.0f)\n", 
                    calibrationSampleCount, gainMultiplier, peakMagnitude);
    }
    
  } else if (!calibrationComplete) {
    calibrationComplete = true;
    isCalibrating = false;
    
    double sum = 0;
    double maxSample = 0;
    double minSample = 999999;
    
    for (int i = 0; i < calibrationSampleCount; i++) {
      sum += calibrationSamples[i];
      if (calibrationSamples[i] > maxSample) {
        maxSample = calibrationSamples[i];
      }
      if (calibrationSamples[i] < minSample) {
        minSample = calibrationSamples[i];
      }
    }
    double averageMagnitude = sum / max(calibrationSampleCount, 1);
    
    noiseFloor = averageMagnitude;
    
    if (averageMagnitude > 0) {
      double targetGain = (TARGET_PEAK_LEVEL * gainMultiplier) / (averageMagnitude / 1000.0);
      if (targetGain > INITIAL_GAIN_MULTIPLIER * 10) {
        targetGain = INITIAL_GAIN_MULTIPLIER * 10;
      }
      if (targetGain < 0.5) {
        targetGain = 0.5;
      }
      gainMultiplier = targetGain;
    }
    
    Serial.println("\n========== CALIBRATION COMPLETE ==========");
    Serial.printf("Noise floor: %.2f\n", noiseFloor);
    Serial.printf("Average peak: %.2f\n", averageMagnitude);
    Serial.printf("Max peak: %.2f\n", maxSample);
    Serial.printf("Min peak: %.2f\n", minSample);
    Serial.printf("Final gain multiplier: %.2f\n", gainMultiplier);
    Serial.println("Effects will now respond to audio input!");
    Serial.println("=========================================\n");
    
    lastCalibrationTime = millis();
    triggerMicCalibration = false;
  }
}

void performAutoGainControl() {
  if (!calibrationComplete) {
    return;
  }
  
  double maxBandMagnitude = 0;
  for (int band = 0; band < NUM_FREQ_BANDS; band++) {
    if (bandMagnitudes[band] > maxBandMagnitude) {
      maxBandMagnitude = bandMagnitudes[band];
    }
  }
  
  if (maxBandMagnitude > 0.01) {
    double gainAdjustment = 1.0 + (TARGET_PEAK_LEVEL - maxBandMagnitude) * GAIN_ADJUSTMENT_RATE;
    
    if (gainAdjustment > 1.05) gainAdjustment = 1.05;
    if (gainAdjustment < 0.95) gainAdjustment = 0.95;
    
    gainMultiplier *= gainAdjustment;
    
    if (gainMultiplier > 20.0) gainMultiplier = 20.0;
    if (gainMultiplier < 0.5) gainMultiplier = 0.5;
  }
}

void checkAndRecalibrate() {
  if (triggerMicCalibration) {
    if (!isCalibrating) {
      Serial.println("\n*** MANUAL CALIBRATION TRIGGERED VIA FIREBASE ***");
      calibrationComplete = false;
      calibrationSampleCount = 0;
      calibrationStartTime = 0;
    }
  }
}

// ============================================================================
// FFT AND FREQUENCY ANALYSIS
// ============================================================================

void updateFrequencyDetection() {
  if (!calibrationComplete) {
    calibrateMicrophone();
    return;
  }
  
  checkAndRecalibrate();
  
  if (activeMicrophone == MIC_I2S_ICS43434) {
    readI2SSamples();
  } else {
    readAnalogSamples();
  }
  
  for (int i = 0; i < N_SAMPLES; i++) {
    vReal[i] *= gainMultiplier;
    vImag[i] = 0.0;
  }
  
  FFT.compute(vReal, vImag, N_SAMPLES, FFT_FORWARD);
  FFT.complexToMagnitude(vReal, vImag, N_SAMPLES);
  
  for (int i = 0; i < N_SAMPLES / 2; i++) {
    frequencyResponse[i] = vReal[i];
  }
  
  analyzeAudioBands();
  performAutoGainControl();
  detectBeat();
  normalizeAudioLevels();
}

// ============================================================================
// FREQUENCY BAND ANALYSIS WITH NOISE FILTERING
// ============================================================================

void analyzeAudioBands() {
  int samplesPerBand = (N_SAMPLES / 2) / NUM_FREQ_BANDS;
  
  for (int band = 0; band < NUM_FREQ_BANDS; band++) {
    bandMagnitudes[band] = 0.0;
    
    int startBin = band * samplesPerBand;
    int endBin = startBin + samplesPerBand;
    
    for (int bin = startBin; bin < endBin && bin < N_SAMPLES / 2; bin++) {
      double magnitude = vReal[bin];
      
      if (magnitude > noiseFloor * NOISE_FLOOR_MULTIPLIER) {
        if (magnitude > bandMagnitudes[band]) {
          bandMagnitudes[band] = magnitude;
        }
      }
    }
    
    if (bandMagnitudes[band] > bandMaxima[band]) {
      bandMaxima[band] = bandMagnitudes[band];
    } else {
      bandMaxima[band] *= 0.95;
    }
    
    smoothedBandMagnitudes[band] = smoothedBandMagnitudes[band] * 0.7 + bandMagnitudes[band] * 0.3;
  }
  
  bassLevel = (bandMagnitudes[0] + bandMagnitudes[1]) / 2.0;
  midLevel = (bandMagnitudes[2] + bandMagnitudes[3] + bandMagnitudes[4]) / 3.0;
  trebleLevel = (bandMagnitudes[5] + bandMagnitudes[6] + bandMagnitudes[7]) / 3.0;
  
  globalAudioLevel = 0;
  for (int band = 0; band < NUM_FREQ_BANDS; band++) {
    globalAudioLevel += smoothedBandMagnitudes[band];
  }
  globalAudioLevel /= NUM_FREQ_BANDS;
  
  if (globalAudioLevel < noiseFloor * 0.8) {
    globalAudioLevel = 0;
  }
}

// ============================================================================
// BEAT DETECTION WITH NOISE FILTERING
// ============================================================================

void detectBeat() {
  static double previousBassLevel = 0;
  static unsigned long lastBeatTime = 0;
  
  double bassIncrease = bassLevel - previousBassLevel;
  previousBassLevel = bassLevel;
  
  beatDetected = false;
  beatEnergy = 0;
  
  if (bassLevel > noiseFloor && bassIncrease > (noiseFloor * 0.5) && 
      (millis() - lastBeatTime) > 100) {
    beatDetected = true;
    beatEnergy = bassIncrease / (noiseFloor * 2.0);
    if (beatEnergy > 1.0) beatEnergy = 1.0;
    lastBeatTime = millis();
  }
}

// ============================================================================
// AUDIO LEVEL NORMALIZATION
// ============================================================================

void normalizeAudioLevels() {
  double normalizeReference = noiseFloor * NOISE_FLOOR_MULTIPLIER;
  
  if (normalizeReference < 50.0) {
    normalizeReference = 50.0;
  }
  
  for (int band = 0; band < NUM_FREQ_BANDS; band++) {
    double normalized = bandMagnitudes[band] / normalizeReference;
    if (normalized > 1.0) normalized = 1.0;
    if (normalized < 0.0) normalized = 0.0;
    bandMagnitudes[band] = normalized;
  }
  
  bassLevel = bassLevel / normalizeReference;
  if (bassLevel > 1.0) bassLevel = 1.0;
  if (bassLevel < 0.0) bassLevel = 0.0;
  
  midLevel = midLevel / normalizeReference;
  if (midLevel > 1.0) midLevel = 1.0;
  if (midLevel < 0.0) midLevel = 0.0;
  
  trebleLevel = trebleLevel / normalizeReference;
  if (trebleLevel > 1.0) trebleLevel = 1.0;
  if (trebleLevel < 0.0) trebleLevel = 0.0;
  
  globalAudioLevel = globalAudioLevel / normalizeReference;
  if (globalAudioLevel > 1.0) globalAudioLevel = 1.0;
  if (globalAudioLevel < 0.0) globalAudioLevel = 0.0;
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