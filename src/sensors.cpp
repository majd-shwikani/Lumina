#include "sensors.h"


// Light sensor objects and variables
Adafruit_VEML7700 veml = Adafruit_VEML7700();
volatile float currentLux = 0;
volatile bool sensorAvailable = false;
volatile float luxThreshold = 1.0;

// Frequency detection objects and variables
int16_t raw_samples[BUFFER_LEN];
ArduinoFFT<double> FFT = ArduinoFFT<double>();
double vReal[N_SAMPLES];
double vImag[N_SAMPLES];

const int NUM_FREQ_BANDS = 8;
double bandMagnitudes[NUM_FREQ_BANDS] = {0};
double frequencyThreshold = 1000.0;

volatile double detectedFrequency = 0;
volatile double frequencyMagnitude = 0;

// Initialize VEML7700 light sensor
void setupVEML7700() {
  if (!veml.begin()) {
    Serial.println("VEML7700 sensor not found, continuing without light sensor");
    sensorAvailable = false;
    return;
  }
  
  sensorAvailable = true;
  veml.setGain(VEML7700_GAIN_1_8);
  veml.setIntegrationTime(VEML7700_IT_100MS);
  Serial.println("VEML7700 initialized");
}

// Read current light level from sensor
void updateSensorData() {
  currentLux = veml.readLux();
}

// Check if light level is below threshold
bool shouldTurnOffDueToDarkness() {
  return currentLux < luxThreshold;
}

// Setup I2S for frequency detection
void setupFrequencyDetection() {
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
  
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
}

// Update frequency detection using FFT
void updateFrequencyDetection() {
  size_t bytes_read = 0;
  i2s_read(I2S_PORT, raw_samples, sizeof(raw_samples), &bytes_read, 0);

  if (bytes_read == sizeof(raw_samples)) {
    // Convert samples to double for FFT
    for (int i = 0; i < N_SAMPLES; i++) {
      vReal[i] = (double)raw_samples[i];
      vImag[i] = 0.0;
    }
    
    // Perform FFT
    FFT.compute(vReal, vImag, N_SAMPLES, FFT_FORWARD);
    FFT.complexToMagnitude(vReal, vImag, N_SAMPLES);
    
    // Reset band magnitudes
    for (int band = 0; band < NUM_FREQ_BANDS; band++) {
      bandMagnitudes[band] = 0.0;
    }
    
    // Analyze frequency bands (divide spectrum into NUM_FREQ_BANDS bands)
    int samplesPerBand = (N_SAMPLES / 2) / NUM_FREQ_BANDS;
    
    for (int band = 0; band < NUM_FREQ_BANDS; band++) {
      double maxInBand = 0.0;
      int startBin = band * samplesPerBand;
      int endBin = startBin + samplesPerBand;
      
      // Find maximum magnitude in this frequency band
      for (int bin = startBin; bin < endBin && bin < N_SAMPLES/2; bin++) {
        if (vReal[bin] > maxInBand) {
          maxInBand = vReal[bin];
        }
      }
      
      bandMagnitudes[band] = maxInBand;
    }
  }
}

// Convert frequency to color (example implementation)
uint32_t frequencyToColor(double freq) {
  // Simple mapping of frequency to hue
  uint8_t hue = (uint8_t)(fmod(freq, 1000.0) / 1000.0 * 255);
  // Convert HSV to RGB (simplified - you might want to use a proper conversion)
  return strip.ColorHSV(hue * 256, 255, 255);
}