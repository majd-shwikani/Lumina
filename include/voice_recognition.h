#ifndef VOICE_RECOGNITION_H
#define VOICE_RECOGNITION_H

#include <Arduino.h>

void setupVoiceRecognition();
void voiceRecognitionTask(void *pvParameters);

#endif
