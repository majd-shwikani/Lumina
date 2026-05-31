#include "sensors.h"
#include "globals.h"
#include <esp_task_wdt.h>
#include <math.h>
#include <string.h>

// ============================================================================
// LIGHT SENSOR OBJECTS AND VARIABLES
// ============================================================================
Adafruit_VEML7700 veml = Adafruit_VEML7700();
volatile float currentLux = 0;
volatile float currentCpuTemp = 0;
volatile bool sensorAvailable = false;
volatile float luxThreshold = 1.0;

// ============================================================================
// POWER MONITORING OBJECTS AND VARIABLES (INA219)
// ============================================================================
Adafruit_INA219 ina219;
volatile float currentVoltage = 0;
volatile float currentCurrent = 0;
volatile float currentPower = 0;
volatile bool ina219Available = false;

// ============================================================================
// AUDIO SENSOR OBJECTS AND VARIABLES
// ============================================================================

// Microphone Selection
volatile int activeMicrophone = MIC_I2S_ICS43434;

// ============================================================================
// NOISE FLOOR (continuously adaptive, no explicit calibration)
// ============================================================================

volatile float noiseFloor = 40000.0f;

// ============================================================================
// FFT OBJECTS AND BUFFERS (esp-dsp)
// ============================================================================

__attribute__((aligned(16))) float fftInput[FFT_SAMPLES * 2];
__attribute__((aligned(16))) float windowCoefficients[FFT_SAMPLES];
float prevFluxMag[FFT_SAMPLES / 2];
int binToBand[FFT_SAMPLES / 2];

// Frequency Band Analysis
volatile float bandMagnitudes[NUM_FREQ_BANDS] = {0};
volatile float smoothedBandMagnitudes[NUM_FREQ_BANDS] = {0};
volatile float bandPeak[NUM_FREQ_BANDS] = {0};

// Audio Analysis Variables
volatile float globalAudioLevel = 0;
volatile float bassLevel = 0;
volatile float midLevel = 0;
volatile float trebleLevel = 0;
volatile bool beatDetected = false;
volatile float beatEnergy = 0;
volatile float spectralCentroid = 0;
volatile float spectralFlux = 0;
volatile float audioVolume = 0;

// Multi-band onset detection
volatile bool onsetMid = false;
volatile bool onsetHigh = false;

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

void setupINA219() {
  if (!ina219.begin()) {
    Serial.println("INA219 sensor not found, continuing without power monitor");
    ina219Available = false;
    return;
  }
  
  ina219Available = true;
  Serial.println("INA219 power monitor initialized successfully");
}

void setupInternalTempSensor() {
  temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
  temp_sensor.dac_offset = TSENS_DAC_L2;
  temp_sensor_set_config(temp_sensor);
  temp_sensor_start();
  Serial.println("Internal ESP32-S3 temperature sensor driver initialized");
}

void updateSensorData() {
  esp_task_wdt_reset();
  if (sensorAvailable) {
    float newLux = veml.readLux();
    if (newLux >= 0 && newLux < 10000.0) {
      currentLux = (newLux * 0.3f) + (currentLux * 0.7f);
    }
  }

  if (ina219Available) {
    currentVoltage = ina219.getBusVoltage_V();
    currentCurrent = ina219.getCurrent_mA();
    currentPower = ina219.getPower_mW();
  }

  static uint32_t lastTempUpdate = 0;
  if (millis() - lastTempUpdate >= 2000 || lastTempUpdate == 0) {
    lastTempUpdate = millis();
    float tsens_out;
    temp_sensor_read_celsius(&tsens_out);
    currentCpuTemp = tsens_out;
  }
}

bool shouldTurnOffDueToDarkness() {
  float effectiveThreshold = luxThreshold;
  if (stripEnabled) {
    effectiveThreshold = luxThreshold + 8.0f;
  }
  return currentLux < effectiveThreshold;
}

// ============================================================================
// I2S MICROPHONE (ICS43434) SETUP
// ============================================================================

void setupI2SMicrophone() {
  const i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLING_FREQ,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  
  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK_PIN,
    .ws_io_num = I2S_WS_PIN,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD_PIN
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
  
  Serial.println("I2S microphone (ICS43434) at 44.1kHz initialized");
}

// ============================================================================
// ANALOG MICROPHONE (MAX9814) SETUP
// ============================================================================

void setupAnalogMicrophone() {
  analogSetAttenuation(ADC_11db);
  analogSetClockDiv(1);
  Serial.println("Analog microphone (MAX9814) initialized");
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
  initBandMapping();
  selectMicrophone(MIC_I2S_ICS43434);
}

// ============================================================================
// LOG-SPACED BAND MAPPING INITIALIZATION
// ============================================================================

void initBandMapping() {
  float minFreq = BIN_WIDTH * 2.0f;
  float maxFreq = BIN_WIDTH * (FFT_SAMPLES / 2 - 1);
  float logMin = log10f(minFreq);
  float logMax = log10f(maxFreq);
  float logRange = logMax - logMin;

  for (int bin = 0; bin < FFT_SAMPLES / 2; bin++) {
    if (bin < 2) {
      binToBand[bin] = 0;
    } else {
      float freq = bin * BIN_WIDTH;
      float norm = (log10f(freq) - logMin) / logRange;
      int band = (int)(norm * (NUM_FREQ_BANDS - 1) + 0.5f);
      binToBand[bin] = constrain(band, 0, NUM_FREQ_BANDS - 1);
    }
  }
  
  Serial.printf("Band mapping initialized: %.1fHz - %.1fHz, %.1fHz/bin\n",
                minFreq, maxFreq, BIN_WIDTH);
}



// ============================================================================
// AUDIO PROCESSING PIPELINE (Highly Optimized Single-Pass)
// ============================================================================

void audioProcessingTask(void *pvParameters) {
  Serial.println("🎵 Audio DSP Task started on Core " + String(xPortGetCoreID()));

  if (dsps_fft2r_init_fc32(NULL, 1024) != ESP_OK) {
    Serial.println("FATAL: SIMD FFT init failed!");
    vTaskDelete(NULL);
    return;
  }
  dsps_wind_hann_f32(windowCoefficients, FFT_SAMPLES);
  initBandMapping();
  memset(prevFluxMag, 0, sizeof(prevFluxMag));

  // Global AGC & tracking state
  float agcMax = 500000.0f;
  float bassBaseline = 0, midBaseline = 0, highBaseline = 0;

  for (;;) {
    if (audioMirrorMode) {
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }

    int32_t dmaBuffer[FFT_SAMPLES];
    size_t bytesRead;
    i2s_read(I2S_PORT, dmaBuffer, sizeof(dmaBuffer), &bytesRead, portMAX_DELAY);
    int samplesRead = bytesRead / sizeof(int32_t);
    if (samplesRead == 0) continue;

    // --- 1. Pre-processing: DC removal & Windowing ---
    float mean = 0;
    for (int i = 0; i < samplesRead; i++) {
      fftInput[i * 2] = (float)(dmaBuffer[i] >> 8);
      mean += fftInput[i * 2];
    }
    mean /= samplesRead;
    for (int i = 0; i < samplesRead; i++) {
      fftInput[i * 2] = (fftInput[i * 2] - mean) * windowCoefficients[i];
      fftInput[i * 2 + 1] = 0;
    }
    for (int i = samplesRead; i < FFT_SAMPLES; i++) {
      fftInput[i * 2] = 0;
      fftInput[i * 2 + 1] = 0;
    }

    // --- 2. SIMD FFT Execution ---
    dsps_fft2r_fc32(fftInput, FFT_SAMPLES);
    dsps_bit_rev_fc32(fftInput, FFT_SAMPLES);

    // --- 3. Single-Pass Feature Extraction ---
    float rawBands[NUM_FREQ_BANDS] = {0};
    float fluxDelta = 0, fluxTotal = 0.001f;
    float centroidWeighted = 0, centroidTotal = 0.001f;

    for (int i = 2; i < FFT_SAMPLES / 2; i++) {
      float real = fftInput[i * 2];
      float imag = fftInput[i * 2 + 1];
      float mag = sqrtf(real * real + imag * imag);

      // Accumulate bands
      rawBands[binToBand[i]] += mag;

      // Spectral Flux
      float diff = mag - prevFluxMag[i];
      if (diff > 0) fluxDelta += diff;
      prevFluxMag[i] = mag;
      fluxTotal += mag;

      // Spectral Centroid
      centroidWeighted += mag * (i * BIN_WIDTH);
      centroidTotal += mag;
    }

    // --- 4. Spectral Feature Normalization ---
    float fluxNorm = fluxDelta / fluxTotal;
    float centroidHz = centroidWeighted / centroidTotal;
    spectralCentroid = constrain((centroidHz - 200.0f) / (20000.0f - 200.0f), 0.0f, 1.0f);
    spectralFlux = constrain(fluxNorm * 4.0f, 0.0f, 1.0f);

    // --- 5. Per-Band Envelope Following & Peak Tracking ---
    float currentPeak = 0;
    for (int b = 0; b < NUM_FREQ_BANDS; b++) {
      float attack = 0.35f + (b / (float)(NUM_FREQ_BANDS - 1)) * 0.25f;
      float release = 0.08f + (1.0f - b / (float)(NUM_FREQ_BANDS - 1)) * 0.12f;

      if (rawBands[b] > smoothedBandMagnitudes[b]) {
        smoothedBandMagnitudes[b] += (rawBands[b] - smoothedBandMagnitudes[b]) * attack;
      } else {
        smoothedBandMagnitudes[b] += (rawBands[b] - smoothedBandMagnitudes[b]) * release;
      }
      
      if (smoothedBandMagnitudes[b] < 0) smoothedBandMagnitudes[b] = 0;

      // Track peaks for normalization
      if (smoothedBandMagnitudes[b] > bandPeak[b]) bandPeak[b] = smoothedBandMagnitudes[b];
      else bandPeak[b] *= 0.9993f;
      
      if (bandPeak[b] < 10000.0f) bandPeak[b] = 10000.0f;
      if (smoothedBandMagnitudes[b] > currentPeak) currentPeak = smoothedBandMagnitudes[b];
    }

    // --- 6. Adaptive Noise Floor & AGC ---
    if (currentPeak < noiseFloor * 1.6f) {
      noiseFloor = (noiseFloor * 0.995f) + (currentPeak * 0.005f);
    }
    bool environmentIsSilent = (currentPeak < (noiseFloor + 15000.0f));

    if (!environmentIsSilent) {
      if (currentPeak > agcMax) agcMax = currentPeak;
      else agcMax = (agcMax * 0.9997f) + (currentPeak * 0.0003f);
      if (agcMax < 100000.0f) agcMax = 100000.0f;
    }

    // --- 7. Final Normalization for Effects ---
    audioVolume = environmentIsSilent ? 0.0f : constrain(currentPeak / agcMax, 0.0f, 1.0f);
    
    for (int b = 0; b < NUM_FREQ_BANDS; b++) {
      if (environmentIsSilent) {
        bandMagnitudes[b] = 0.0f;
      } else {
        bandMagnitudes[b] = constrain(smoothedBandMagnitudes[b] / (bandPeak[b] * 1.2f), 0.0f, 1.0f);
      }
    }

    // --- 8. Multi-Band Onset Detection (Using Raw Energy) ---
    float bassSum = 0, midSum = 0, highSum = 0;
    for (int b = 0; b < NUM_FREQ_BANDS; b++) {
      if (b <= 3)      bassSum += smoothedBandMagnitudes[b];
      else if (b <= 9) midSum  += smoothedBandMagnitudes[b];
      else             highSum += smoothedBandMagnitudes[b];
    }

    beatDetected = (bassSum > bassBaseline * 1.35f && bassSum > noiseFloor * 2.0f);
    onsetMid     = (midSum  > midBaseline  * 1.25f && midSum  > noiseFloor * 1.5f);
    onsetHigh    = (highSum > highBaseline * 1.25f && highSum > noiseFloor);

    bassBaseline = (bassBaseline * 0.7f) + (bassSum * 0.3f);
    midBaseline  = (midBaseline  * 0.7f) + (midSum  * 0.3f);
    highBaseline = (highBaseline * 0.7f) + (highSum * 0.3f);

    // --- 9. Update Reporting Variables ---
    bassLevel = 0;
    for (int b = 0; b < 4; b++) bassLevel += bandMagnitudes[b];
    bassLevel /= 4.0f;

    midLevel = 0;
    for (int b = 4; b < 10; b++) midLevel += bandMagnitudes[b];
    midLevel /= 6.0f;

    trebleLevel = 0;
    for (int b = 10; b < NUM_FREQ_BANDS; b++) trebleLevel += bandMagnitudes[b];
    trebleLevel /= 6.0f;

    globalAudioLevel = (bassLevel + midLevel + trebleLevel) / 3.0f;

    // --- 10. Human-Readable Periodic Status ---
    static uint32_t lastPrint = 0;
    if (millis() - lastPrint > 2000) {
      lastPrint = millis();
      Serial.printf("🎵 [Audio] Vol: %.2f | Centroid: %.2f | Flux: %.2f | Onsets: [%s%s%s]\n",
                    audioVolume, spectralCentroid, spectralFlux,
                    beatDetected ? "B" : "-",
                    onsetMid     ? "M" : "-",
                    onsetHigh    ? "H" : "-");
      
      Serial.print("   Bands: ");
      for (int b = 0; b < NUM_FREQ_BANDS; b++) {
        Serial.printf("%d ", (int)(bandMagnitudes[b] * 9));
      }
      Serial.println();
    }

    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

// ============================================================================
// COLOR CONVERSION UTILITIES
// ============================================================================

uint32_t frequencyToColor(float freq) {
  float normalizedFreq = freq / 8000.0f;
  if (normalizedFreq > 1.0f) normalizedFreq = 1.0f;
  uint8_t hue = (uint8_t)(normalizedFreq * 255);
  CRGB color = CHSV(hue, 255, 255);
  return ((uint32_t)color.r << 16) | ((uint32_t)color.g << 8) | color.b;
}

