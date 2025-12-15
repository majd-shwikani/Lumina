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
volatile double gainMultiplier = INITIAL_GAIN_MULTIPLIER;  // Start with higher gain
unsigned long lastCalibrationTime = 0;

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
  
  // Reset calibration when switching microphones
  calibrationComplete = false;
  lastCalibrationTime = millis();
  calibrationStartTime = millis();
  gainMultiplier = INITIAL_GAIN_MULTIPLIER;  // Reset to higher initial gain
  noiseFloor = 100.0;
  calibrationSampleCount = 0;
  Serial.println("Microphone calibration reset - will recalibrate on next audio processing");
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
  // Initialize start time on first call
  if (calibrationStartTime == 0) {
    calibrationStartTime = millis();
    Serial.println("Starting microphone calibration...");
  }
  
  if (millis() - calibrationStartTime < CALIBRATION_DURATION) {
    // Still in calibration window - collect noise samples
    
    // Read samples
    if (activeMicrophone == MIC_I2S_ICS43434) {
      readI2SSamples();
    } else {
      readAnalogSamples();
    }
    
    // Apply current gain for collection
    for (int i = 0; i < N_SAMPLES; i++) {
      vReal[i] *= gainMultiplier;
      vImag[i] = 0.0;
    }
    
    // Perform FFT on this chunk
    FFT.compute(vReal, vImag, N_SAMPLES, FFT_FORWARD);
    FFT.complexToMagnitude(vReal, vImag, N_SAMPLES);
    
    // Store the peak magnitude
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
    
    Serial.printf("Calibration: %d/256 samples collected (Gain: %.2f, Peak: %.0f)\r", 
                  calibrationSampleCount, gainMultiplier, peakMagnitude);
    
  } else if (!calibrationComplete) {
    // Calibration complete - calculate noise floor
    calibrationComplete = true;
    
    // Find average of collected samples
    double sum = 0;
    double maxSample = 0;
    for (int i = 0; i < calibrationSampleCount; i++) {
      sum += calibrationSamples[i];
      if (calibrationSamples[i] > maxSample) {
        maxSample = calibrationSamples[i];
      }
    }
    double averageMagnitude = sum / max(calibrationSampleCount, 1);
    
    // Noise floor is the average noise level
    noiseFloor = averageMagnitude;
    
    // Adjust gain to reach target peak level
    if (averageMagnitude > 0) {
      double targetGain = (TARGET_PEAK_LEVEL * gainMultiplier) / (averageMagnitude / 1000.0);
      if (targetGain > INITIAL_GAIN_MULTIPLIER * 10) {
        targetGain = INITIAL_GAIN_MULTIPLIER * 10;  // Cap maximum gain
      }
      if (targetGain < 0.5) {
        targetGain = 0.5;  // Floor for gain
      }
      gainMultiplier = targetGain;
    }
    
    Serial.println("\n");
    Serial.printf("Calibration complete!\n");
    Serial.printf("Noise floor: %.2f\n", noiseFloor);
    Serial.printf("Average peak: %.2f\n", averageMagnitude);
    Serial.printf("Max peak: %.2f\n", maxSample);
    Serial.printf("Gain multiplier: %.2f\n", gainMultiplier);
    
    lastCalibrationTime = millis();
  }
}

void performAutoGainControl() {
  // Auto-adjust gain to maintain target peak level
  
  if (!calibrationComplete) {
    return;
  }
  
  // Calculate current peak level in bands
  double maxBandMagnitude = 0;
  for (int band = 0; band < NUM_FREQ_BANDS; band++) {
    if (bandMagnitudes[band] > maxBandMagnitude) {
      maxBandMagnitude = bandMagnitudes[band];
    }
  }
  
  // Adjust gain based on deviation from target (already normalized 0-1)
  if (maxBandMagnitude > 0.01) { // Only adjust if there's actual signal
    double gainAdjustment = 1.0 + (TARGET_PEAK_LEVEL - maxBandMagnitude) * GAIN_ADJUSTMENT_RATE;
    
    // Clamp gain adjustment to reasonable range (0.95x to 1.05x per cycle)
    if (gainAdjustment > 1.05) gainAdjustment = 1.05;
    if (gainAdjustment < 0.95) gainAdjustment = 0.95;
    
    // Apply gain adjustment
    gainMultiplier *= gainAdjustment;
    
    // Clamp overall gain to reasonable range (allow higher max gain)
    if (gainMultiplier > 20.0) gainMultiplier = 20.0;
    if (gainMultiplier < 0.5) gainMultiplier = 0.5;
  }
}

void checkAndRecalibrate() {
  // Periodically recalibrate in case environment changes
  if (millis() - lastCalibrationTime > CALIBRATION_CHECK_INTERVAL) {
    Serial.println("\nStarting environmental recalibration...");
    calibrationComplete = false;
    calibrationSampleCount = 0;
    calibrationStartTime = millis();
  }
}

// ============================================================================
// FFT AND FREQUENCY ANALYSIS
// ============================================================================

void updateFrequencyDetection() {
  // Perform calibration if not complete
  if (!calibrationComplete) {
    calibrateMicrophone();
    return; // Skip normal processing during calibration
  }
  
  // Check for environmental recalibration
  checkAndRecalibrate();
  
  // Read samples based on active microphone
  if (activeMicrophone == MIC_I2S_ICS43434) {
    readI2SSamples();
  } else {
    readAnalogSamples();
  }
  
  // Apply gain multiplier and apply noise gate
  for (int i = 0; i < N_SAMPLES; i++) {
    vReal[i] *= gainMultiplier;
    vImag[i] = 0.0;
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
  
  // Perform auto gain adjustment
  performAutoGainControl();
  
  // Detect beats
  detectBeat();
  
  // Normalize levels
  normalizeAudioLevels();
}

// ============================================================================
// FREQUENCY BAND ANALYSIS WITH NOISE FILTERING
// ============================================================================

void analyzeAudioBands() {
  // Divide spectrum into 8 frequency bands
  int samplesPerBand = (N_SAMPLES / 2) / NUM_FREQ_BANDS;
  
  // Reset band magnitudes
  for (int band = 0; band < NUM_FREQ_BANDS; band++) {
    bandMagnitudes[band] = 0.0;
    
    int startBin = band * samplesPerBand;
    int endBin = startBin + samplesPerBand;
    
    // Find maximum magnitude in this band
    for (int bin = startBin; bin < endBin && bin < N_SAMPLES / 2; bin++) {
      double magnitude = vReal[bin];
      
      // Simple noise gate: ignore signals below noise floor * multiplier
      if (magnitude > noiseFloor * NOISE_FLOOR_MULTIPLIER) {
        if (magnitude > bandMagnitudes[band]) {
          bandMagnitudes[band] = magnitude;
        }
      }
    }
    
    // Update peak maximum with decay
    if (bandMagnitudes[band] > bandMaxima[band]) {
      bandMaxima[band] = bandMagnitudes[band];
    } else {
      bandMaxima[band] *= 0.95; // Decay peak over time
    }
    
    // Smooth the magnitude values (exponential moving average)
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
  
  // Apply noise gate to global level - more lenient now
  if (globalAudioLevel < noiseFloor * 0.8) {
    globalAudioLevel = 0;
  }
}

// ============================================================================
// BEAT DETECTION WITH NOISE FILTERING
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
  // Only detect if above noise floor
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
  // Normalize band magnitudes to 0-1 range
  // Use a dynamic normalization reference based on recent peaks
  double normalizeReference = noiseFloor * NOISE_FLOOR_MULTIPLIER;
  
  // Use a more generous scaling to preserve detail
  if (normalizeReference < 50.0) {
    normalizeReference = 50.0;  // Minimum reference to avoid over-amplification
  }
  
  for (int band = 0; band < NUM_FREQ_BANDS; band++) {
    double normalized = bandMagnitudes[band] / normalizeReference;
    if (normalized > 1.0) normalized = 1.0;
    if (normalized < 0.0) normalized = 0.0;
    bandMagnitudes[band] = normalized;
  }
  
  // Normalize bass, mid, treble levels
  bassLevel = bassLevel / normalizeReference;
  if (bassLevel > 1.0) bassLevel = 1.0;
  if (bassLevel < 0.0) bassLevel = 0.0;
  
  midLevel = midLevel / normalizeReference;
  if (midLevel > 1.0) midLevel = 1.0;
  if (midLevel < 0.0) midLevel = 0.0;
  
  trebleLevel = trebleLevel / normalizeReference;
  if (trebleLevel > 1.0) trebleLevel = 1.0;
  if (trebleLevel < 0.0) trebleLevel = 0.0;
  
  // Normalize global level
  globalAudioLevel = globalAudioLevel / normalizeReference;
  if (globalAudioLevel > 1.0) globalAudioLevel = 1.0;
  if (globalAudioLevel < 0.0) globalAudioLevel = 0.0;
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