#include "voice_recognition.h"
#include "globals.h"
#include "sensors.h"
#include "driver/i2s.h"

void setupVoiceRecognition() {
  Serial.println("🎤 Local Voice Detection (VAD) initialized");
}

void voiceRecognitionTask(void *pvParameters) {
  int16_t buffer[256];
  size_t bytes_read = 0;
  const int threshold = 5000; // Energy threshold for "Voice" detection
  
  Serial.println("🎤 Voice Detection Task started");
  
  while (true) {
    if (i2sMutex != NULL && xSemaphoreTake(i2sMutex, portMAX_DELAY) == pdTRUE) {
      i2s_read(I2S_PORT, buffer, sizeof(buffer), &bytes_read, portMAX_DELAY);
      xSemaphoreGive(i2sMutex);
    }
    
    if (bytes_read > 0) {
      long energy = 0;
      for (int i = 0; i < 256; i++) {
        energy += abs(buffer[i]);
      }
      energy /= 256;
      
      if (energy > threshold) {
        if (!isListening) {
          Serial.println("🔔 Local sound detected - Listening mode active");
          isListening = true;
          // In this mode, we expect commands via Alexa/Google Home
          // or we can implement simple clap detection here
        }
        // Keep listening as long as there is sound
      } else {
        if (isListening) {
          // Wait for a bit of silence before stopping the animation
          static unsigned long lastSoundTime = 0;
          if (energy > threshold/2) {
            lastSoundTime = millis();
          }
          if (millis() - lastSoundTime > 3000) {
            isListening = false;
            Serial.println("🎤 Silence detected - Listening mode stopped");
          }
        }
      }
      
      // Update global samples for audio reactive effects
      // This ensures effects still dance while we detect voice
      memcpy(raw_samples, buffer, sizeof(raw_samples));
    }
    
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}
