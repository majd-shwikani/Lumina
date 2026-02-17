#include "voice_recognition.h"
#include "globals.h"
#include "sensors.h"
#include "driver/i2s.h"
#include <WiFiClientSecure.h>
#include <cmath>

// --- USER CONFIG ---
#define WIT_AI_TOKEN  "NTWMUOLJAPOF3QLEGBJCT4QA56NH45FR"

// --- AUDIO PARAMETERS ---
#define SAMPLE_RATE         16000
// I2S_PORT is defined in config.h

// Chunk size: 20 ms @ 16 kHz = 320 samples
#define CHUNK_SAMPLES       320
#define CHUNK_BYTES         (CHUNK_SAMPLES * sizeof(int16_t))

// Pre-buffer: 1 second of audio kept rolling before wake
#define PREBUF_CHUNKS       50    // 50 × 20 ms = 1 second
#define PREBUF_SAMPLES      (PREBUF_CHUNKS * CHUNK_SAMPLES)

// Max recording after wake: 6 seconds
#define RECORD_MAX_CHUNKS   300   // 300 × 20 ms = 6 seconds

// Stop recording after this many consecutive silent chunks (0.5 s of silence)
#define SILENCE_CHUNKS      25

// Calibration: 3 seconds of ambient noise measurement
#define CALIB_CHUNKS        150

// Wake threshold multiplier over ambient baseline
#define WAKE_MULTIPLIER     4.0f

// --- GLOBALS ---
float   wakeThresh   = 500.0f;

// Pre-buffer (circular)
int16_t *preBuf      = nullptr;
int      preBufHead  = 0;   // oldest chunk index (circular)
int      preBufCount = 0;

// Recording buffer: pre-buffer + max recording
int16_t *recBuf      = nullptr;
size_t   recSamples  = 0;

// --- FORWARD DECLARATIONS ---
float readChunk(int16_t *dst);
void  doCalibration();
bool  sendToWitAI(const int16_t *buf, size_t samples, String &result);

void setupVoiceRecognition() {
    Serial.println("🎤 Initializing Advanced Voice Recognition (Wit.ai)...");
    
    // Allocate buffers in PSRAM
    preBuf = (int16_t *)ps_malloc(PREBUF_SAMPLES * sizeof(int16_t));
    if (!preBuf) preBuf = (int16_t *)malloc(PREBUF_SAMPLES * sizeof(int16_t));
    if (!preBuf) { Serial.println("[ERR] preBuf malloc failed"); return; }
    memset(preBuf, 0, PREBUF_SAMPLES * sizeof(int16_t));

    size_t recBufSamples = (PREBUF_CHUNKS + RECORD_MAX_CHUNKS) * CHUNK_SAMPLES;
    recBuf = (int16_t *)ps_malloc(recBufSamples * sizeof(int16_t));
    if (!recBuf) recBuf = (int16_t *)malloc(recBufSamples * sizeof(int16_t));
    if (!recBuf) { Serial.println("[ERR] recBuf malloc failed"); return; }

    doCalibration();
    Serial.printf("[OK] Wake threshold: %.0f RMS\n", wakeThresh);
}

float readChunk(int16_t *dst) {
    int64_t sumSq = 0;
    size_t rb = 0;
    
    for (int i = 0; i < CHUNK_SAMPLES; i++) {
        int32_t raw = 0;
        if (i2sMutex != NULL && xSemaphoreTake(i2sMutex, portMAX_DELAY) == pdTRUE) {
            i2s_read(I2S_PORT, &raw, sizeof(raw), &rb, portMAX_DELAY);
            xSemaphoreGive(i2sMutex);
        }
        int16_t s = (int16_t)(raw >> 16);
        if (dst) dst[i] = s;
        sumSq += (int64_t)s * s;
    }
    return sqrtf((float)(sumSq / CHUNK_SAMPLES));
}

void doCalibration() {
    Serial.println("[CAL] Measuring ambient noise (3 s) – stay quiet...");
    static int16_t chunk[CHUNK_SAMPLES];
    float sum = 0;
    for (int i = 0; i < CALIB_CHUNKS; i++) {
        sum += readChunk(chunk);
    }
    float baseline = sum / CALIB_CHUNKS;
    wakeThresh = std::max(baseline * WAKE_MULTIPLIER, 300.0f);
    Serial.printf("[CAL] Baseline: %.0f -> Threshold: %.0f\n", baseline, wakeThresh);
}

bool sendToWitAI(const int16_t *buf, size_t samples, String &result) {
    const char *HOST    = "api.wit.ai";
    const int   PORT    = 443;
    const char *API_VER = "20240101";

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(10000); // 10 seconds timeout

    if (!client.connect(HOST, PORT)) {
        Serial.println("[STT] TCP connect failed");
        return false;
    }

    size_t contentLen = samples * sizeof(int16_t);

    client.printf("POST /speech?v=%s HTTP/1.1\r\n", API_VER);
    client.printf("Host: %s\r\n", HOST);
    client.printf("Authorization: Bearer %s\r\n", WIT_AI_TOKEN);
    client.printf("Content-Type: audio/raw;encoding=signed-integer;bits=16;rate=16000;endian=little\r\n");
    client.printf("Content-Length: %u\r\n", (unsigned)contentLen);
    client.printf("Connection: close\r\n\r\n");

    const uint8_t *ptr  = (const uint8_t *)buf;
    size_t         left = contentLen;
    while (left > 0) {
        size_t n = std::min(left, (size_t)4096);
        size_t w = client.write(ptr, n);
        if (w == 0) {
            Serial.println("[STT] Write failed");
            client.stop();
            return false;
        }
        ptr  += w;
        left -= w;
    }

    unsigned long t0 = millis();
    while (!client.available() && millis() - t0 < 15000) delay(50);

    String resp = "";
    while (client.available()) resp += (char)client.read();
    client.stop();

    int bodyAt = resp.indexOf("\r\n\r\n");
    String body = (bodyAt >= 0) ? resp.substring(bodyAt + 4) : resp;

    int ti = -1, lastTi = -1;
    while ((ti = body.indexOf("\"text\":", ti + 1)) >= 0) lastTi = ti;

    if (lastTi < 0) {
        Serial.println("[STT] No 'text' in response");
        return false;
    }

    int q1 = body.indexOf('"', lastTi + 7);
    int q2 = body.indexOf('"', q1 + 1);
    if (q1 < 0 || q2 < 0) return false;

    result = body.substring(q1 + 1, q2);
    result.trim();
    return result.length() > 0;
}

void voiceRecognitionTask(void *pvParameters) {
    static int16_t chunk[CHUNK_SAMPLES];
    Serial.println("🎤 Voice Recognition Task started");

    while (true) {
        float rms = readChunk(chunk);

        // Store this chunk into the circular pre-buffer
        int slot = (preBufHead + preBufCount) % PREBUF_CHUNKS;
        if (preBufCount < PREBUF_CHUNKS) {
            preBufCount++;
        } else {
            preBufHead = (preBufHead + 1) % PREBUF_CHUNKS;
        }
        memcpy(preBuf + slot * CHUNK_SAMPLES, chunk, CHUNK_BYTES);

        // Check for wake
        if (rms < wakeThresh) {
            vTaskDelay(1 / portTICK_PERIOD_MS);
            continue;
        }

        // WAKE DETECTED
        Serial.printf("\n>>> Wake detected! (RMS %.0f > threshold %.0f) <<<\n", rms, wakeThresh);
        isListening = true;

        // Copy pre-buffer into recBuf
        recSamples = 0;
        for (int i = 0; i < preBufCount; i++) {
            int idx = (preBufHead + i) % PREBUF_CHUNKS;
            memcpy(recBuf + recSamples, preBuf + idx * CHUNK_SAMPLES, CHUNK_BYTES);
            recSamples += CHUNK_SAMPLES;
        }
        preBufHead = 0; preBufCount = 0;
        memset(preBuf, 0, PREBUF_SAMPLES * sizeof(int16_t));

        // RECORD until silence or max length
        int silentChunks = 0;
        int totalChunks  = 0;
        while (totalChunks < RECORD_MAX_CHUNKS) {
            float chunkRms = readChunk(recBuf + recSamples);
            recSamples += CHUNK_SAMPLES;
            totalChunks++;

            if (chunkRms < wakeThresh * 0.6f) {
                silentChunks++;
                if (silentChunks >= SILENCE_CHUNKS) break;
            } else {
                silentChunks = 0;
            }
            vTaskDelay(1 / portTICK_PERIOD_MS);
        }
        
        Serial.println("Captured audio, sending to Wit.ai...");

        if (WiFi.status() == WL_CONNECTED) {
            String transcript;
            if (sendToWitAI(recBuf, recSamples, transcript)) {
                Serial.printf("You said: %s\n", transcript.c_str());
                transcript.toLowerCase();

                if (transcript.indexOf("lumina on") != -1) {
                    Serial.println("Lumina turning ON");
                    stripEnabled = true;
                    manuallyTurnedOff = false;
                    // Trigger Firebase sync if needed
                } else if (transcript.indexOf("lumina off") != -1) {
                    Serial.println("Lumina turning OFF");
                    stripEnabled = false;
                    manuallyTurnedOff = true;
                    // Trigger Firebase sync if needed
                }
            }
        } else {
            Serial.println("[WARN] WiFi not connected - skipping STT");
        }

        isListening = false;
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
