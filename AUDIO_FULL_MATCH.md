# Lumina Audio — Full Advanced Project Match

Fully match the Advanced Audio Reactive LEDs project: 44.1kHz, 512-point SIMD FFT, 16 log-spaced bands, per-band envelope followers, adaptive noise floor + squelch, and 3-zone onset detection.

---

## Audio Config Upgrade

| Parameter | Current | New |
|-----------|---------|-----|
| Sample rate | 16,000 Hz | **44,100 Hz** |
| FFT size | 256 | **512** |
| FFT library | arduinoFFT (double, software) | **esp-dsp (float, SIMD)** |
| Band count | 8 (linear) | **16 (log-spaced)** |
| Band mapping | Equal bin division | **Logarithmic (172 Hz–22 kHz)** |
| AGC | Global pre-FFT gain multiplier | **Per-band normalization against own peak** |
| Noise floor | Static (calibrated once) | **Adaptive per-frame EMA** |
| Smoothing | Uniform 0.7/0.3 EMA | **Per-band attack/release (0.35–0.60 / 0.08–0.20)** |
| Squelch | None | **Force 0 when currentPeak < noiseFloor + 15000** |
| Onset detection | Single bass beat | **3-zone (bass/mid/high) with adaptive baselines** |

---

## Dynamic Task Lifecycle (Stays As-Is)

```
Effects 22–26 active:
    → AudioDSPTask CREATED on Core 0, pri 3, 8KB
    → Inner loop: I2S(44.1kHz) → esp-dsp FFT → 16-band analysis
    → ~75fps, writes shared globals

Effects 0–21 or 47–56 active:
    → AudioDSPTask DELETED via vTaskDelete
    → 8KB + all buffers freed immediately
```

---

## AudioDSPTask Inner Loop (New)

```cpp
void audioProcessingTask(void *pvParameters) {
    dsps_fft2r_init_fc32(NULL, 1024);
    dsps_wind_hann_f32(windowCoefficients, FFT_SAMPLES);
    initBandMapping();

    int32_t dmaBuffer[FFT_SAMPLES];
    float agcMax = 500000.0f;
    float noiseFloor = 40000.0f;
    float bassBaseline = 0, midBaseline = 0, highBaseline = 0;

    for (;;) {
        if (audioMirrorMode) {
            normalizeAudioLevels();
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }

        // 1. I2S read (44.1kHz, 32-bit)
        i2s_read(I2S_PORT, &dmaBuffer, sizeof(dmaBuffer), &bytesRead, portMAX_DELAY);

        // 2. DC removal + Hann window
        float mean = 0;
        for (int i = 0; i < FFT_SAMPLES; i++) {
            fftInput[i*2] = (float)(dmaBuffer[i] >> 8);
            mean += fftInput[i*2];
        }
        mean /= FFT_SAMPLES;
        for (int i = 0; i < FFT_SAMPLES; i++) {
            fftInput[i*2] = (fftInput[i*2] - mean) * windowCoefficients[i];
            fftInput[i*2+1] = 0.0f;
        }

        // 3. SIMD FFT
        dsps_fft2r_fc32(fftInput, FFT_SAMPLES);
        dsps_bit_rev_fc32(fftInput, FFT_SAMPLES);

        // 4. Magnitudes → 16 log-spaced bands + spectral features
        float bandEnergy[NUM_FREQ_BANDS] = {0};
        float fluxDelta = 0, fluxTotal = 0.001f;
        float centroidWeighted = 0, centroidTotal = 0.001f;

        for (int i = 2; i < FFT_SAMPLES/2; i++) {
            float mag = sqrtf(fftInput[i*2]*fftInput[i*2] + fftInput[i*2+1]*fftInput[i*2+1]);
            bandEnergy[binToBand[i]] += mag;

            float diff = mag - prevFluxMag[i];
            if (diff > 0) fluxDelta += diff;
            prevFluxMag[i] = mag;
            fluxTotal += mag;

            centroidWeighted += mag * (i * BIN_WIDTH);
            centroidTotal += mag;
        }

        spectralCentroid = constrain((centroidWeighted/centroidTotal - 200.0f) / 19800.0f, 0.0f, 1.0f);
        spectralFlux = constrain((fluxDelta / fluxTotal) * 4.0f, 0.0f, 1.0f);

        // 5. Per-band envelope followers + peak tracking
        float currentPeak = 0;
        for (int b = 0; b < NUM_FREQ_BANDS; b++) {
            float attack  = 0.35f + (b / 15.0f) * 0.25f;
            float release = 0.08f + (1.0f - b / 15.0f) * 0.12f;
            if (bandEnergy[b] > bandSmoothed[b])
                bandSmoothed[b] += (bandEnergy[b] - bandSmoothed[b]) * attack;
            else
                bandSmoothed[b] += (bandEnergy[b] - bandSmoothed[b]) * release;
            if (bandSmoothed[b] < 0) bandSmoothed[b] = 0;
            if (bandSmoothed[b] > bandPeak[b]) bandPeak[b] = bandSmoothed[b];
            else bandPeak[b] *= 0.9993f;
            if (bandPeak[b] < 10000.0f) bandPeak[b] = 10000.0f;
            if (bandSmoothed[b] > currentPeak) currentPeak = bandSmoothed[b];
        }

        // 6. Adaptive noise floor + squelch
        if (currentPeak < noiseFloor * 1.6f)
            noiseFloor = (noiseFloor * 0.995f) + (currentPeak * 0.005f);
        bool silent = (currentPeak < (noiseFloor + 15000.0f));
        if (!silent) {
            if (currentPeak > agcMax) agcMax = currentPeak;
            else agcMax = (agcMax * 0.9997f) + (currentPeak * 0.0003f);
            if (agcMax < 100000.0f) agcMax = 100000.0f;
        }

        // 7. Normalize bands against own peaks
        globalAudioLevel = silent ? 0.0f : constrain(currentPeak / agcMax, 0.0f, 1.0f);
        for (int b = 0; b < NUM_FREQ_BANDS; b++)
            bandMagnitudes[b] = silent ? 0.0 : constrain(bandSmoothed[b] / (bandPeak[b] * 1.2), 0.0, 1.0);

        // 8. Bass/mid/treble
        bassLevel = (bandMagnitudes[0]+bandMagnitudes[1]+bandMagnitudes[2]+bandMagnitudes[3]) / 4.0;
        midLevel = (bandMagnitudes[4]+bandMagnitudes[5]+bandMagnitudes[6]+bandMagnitudes[7]+bandMagnitudes[8]+bandMagnitudes[9]) / 6.0;
        trebleLevel = (bandMagnitudes[10]+bandMagnitudes[11]+bandMagnitudes[12]+bandMagnitudes[13]+bandMagnitudes[14]+bandMagnitudes[15]) / 6.0;

        // 9. Multi-band onset detection
        float bassSum = 0, midSum = 0, highSum = 0;
        for (int b = 0; b < NUM_FREQ_BANDS; b++) {
            if (b <= 3)      bassSum  += bandSmoothed[b];
            else if (b <= 9) midSum   += bandSmoothed[b];
            else             highSum  += bandSmoothed[b];
        }
        beatDetected = (bassSum > bassBaseline * 1.35f && bassSum > noiseFloor * 2.0f);
        onsetMid     = (midSum  > midBaseline  * 1.25f && midSum  > noiseFloor * 1.5f);
        onsetHigh    = (highSum > highBaseline * 1.25f && highSum > noiseFloor);
        bassBaseline = (bassBaseline * 0.7f) + (bassSum * 0.3f);
        midBaseline  = (midBaseline  * 0.7f) + (midSum  * 0.3f);
        highBaseline = (highBaseline * 0.7f) + (highSum * 0.3f);

        vTaskDelay(pdMS_TO_TICKS(2));
    }
}
```

---

## Log-Spaced Band Mapping

16 bands from ~172 Hz to ~22 kHz, computed by `initBandMapping()`:

```
binToBand[bin] = constrain((int)(norm * 15 + 0.5f), 0, 15)
  where norm = (log10f(freq) - log10f(minFreq)) / (log10f(maxFreq) - log10f(minFreq))
  and   freq = bin * (44100 / 512)
```

| Band | Bin Range | Frequency Range |
|------|-----------|----------------|
| 0 | 2–3 | 172–431 Hz |
| 1 | 4–5 | 431–689 Hz |
| 2 | 6–8 | 689–1034 Hz |
| 3 | 9–11 | 1034–1466 Hz |
| 4 | 12–15 | 1466–1983 Hz |
| 5 | 16–20 | 1983–2672 Hz |
| 6 | 21–26 | 2672–3532 Hz |
| 7 | 27–34 | 3532–4564 Hz |
| 8 | 35–43 | 4564–5796 Hz |
| 9 | 44–54 | 5796–7242 Hz |
| 10 | 55–67 | 7242–8926 Hz |
| 11 | 68–83 | 8926–10880 Hz |
| 12 | 84–102 | 10880–13152 Hz |
| 13 | 103–125 | 13152–15820 Hz |
| 14 | 126–153 | 15820–19085 Hz |
| 15 | 154–255 | 19085–22050 Hz |

---

## Files to Modify (8)

| # | File | Changes |
|---|------|---------|
| 1 | `platformio.ini` | Add `espressif/esp-dsp`, remove `arduinoFFT` |
| 2 | `sensors.h` | `SAMPLE_RATE→44100`, `N_SAMPLES→512`, `NUM_FREQ_BANDS→16`. Replace `arduinoFFT.h`→`esp_dsp.h`. Replace `vReal/vImag` with `float fftInput[1024]`. All `[8]` arrays→`[16]`. Remove AGC macros, add `initBandMapping()`, `windowCoefficients[512]`, `binToBand[256]` |
| 3 | `effects.h` | `NUM_FREQ_BANDS→16`, `bandMagnitudes[16]`, `bandMaxima[16]` |
| 4 | `globals.h` | Remove `#include <arduinoFFT.h>` |
| 5 | `sensors.cpp` | Rewrite I2S config (44.1kHz, 32-bit). Replace arduinoFFT with esp-dsp. Add `initBandMapping()`. Replace AGC with per-band envelope followers + adaptive noise floor + squelch. Inline centroid/flux/onset in audio task loop |
| 6 | `effects.cpp` | Re-adapt 5 effects to 16 bands (bass=0–3, mid=4–9, high=10–15, `ledsPerBand=ledCount/16`, `bandIdx=pos×16`) |
| 7 | `tasks.cpp` | USB audio mirror: expect 16 bytes, map to 16 bands |
| 8 | `Tools/lumina_sound_mirror.py` | Update PC FFT from 8 to 16 bands, send 16 bytes |

---

## Effects — 16-Band Adaptation

| Effect | Bass | Mid | High | Bars | Verve |
|--------|------|-----|------|------|-------|
| Bands used | 0–3 | 4–9 | 10–15 | 16 groups | `pos×16` |
| `ledsPerBand` | — | — | — | `ledCount/16` | — |

Spectrum Bars peak tracker and Spectral Verve zone onsets update to match 16-band zones (left third = bass onset, middle = mid onset, right = high onset).

---

## Build

```powershell
Set-Location "Lumina S3"; pio run
```
