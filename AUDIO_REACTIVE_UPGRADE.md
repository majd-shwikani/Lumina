# Audio Reactive Upgrade — Lumina S3

Replace the 11 primitive audio-reactive effects (IDs 22–32) with 5 sophisticated modes from the Advanced Audio Reactive LEDs project, backed by a **dynamically-created FreeRTOS audio processing task** that only exists when an audio effect is selected.

---

## Architecture Change — FreeRTOS Audio Task

### Current (broken)

```
Core 0: CloudTask, USBTask (dynamic)
Core 1: LEDTask → effectX() → updateFrequencyDetection() → I2S → FFT → analysis → render
         SystemTask, IOTask, loop()
```

Each audio effect calls `updateFrequencyDetection()` **synchronously** every frame — FFT blocks rendering, runs at `effectSpeed` rate, Core 0 is idle.

### New

```
Core 0: CloudTask, USBTask (dynamic)
        AudioDSPTask [CREATED ONLY WHEN EFFECTS 22–26 ACTIVE] → I2S → FFT → analysis → shared globals
Core 1: LEDTask → effectX() → read shared globals → render
         SystemTask, IOTask, loop()
```

Audio processing runs **continuously at ~75fps** on Core 0 (priority 3) **while the task exists**. Effects on Core 1 just read the pre-computed globals — no FFT calls inside effects, no blocking.

### Data Flow

```
AudioDSPTask ─── writes ──→ bandMagnitudes[8], bassLevel, midLevel, trebleLevel,
                              globalAudioLevel, beatDetected, beatEnergy,
                              spectralCentroid, spectralFlux, onsetMid, onsetHigh,
                              bandPeak[8]
                                   │
                            reads (no mutex — volatile, atomically written on ESP32)
                                   ▼
                          effects 22–26 on LEDTask
```

---

## AudioDSPTask Lifecycle

Managed in `updateLEDs()` in `tasks.cpp`. **Precedent:** identical pattern to existing `toggleUsbMirror()` at `tasks.cpp:542` which dynamically creates/deletes `USBTask`.

```
EVERY FRAME IN updateLEDs():
  isAudio = (currentEffect ∈ [22, 23, 24, 25, 26])

  if isAudio AND AudioDSPTask NOT running →
      xTaskCreatePinnedToCore(audioProcessingTask, "AudioDSP", 8192, NULL, 3,
                              &audioTaskHandle, 0)
      Serial.println("🎵 Audio DSP Task started on Core 0")

  if NOT isAudio AND AudioDSPTask IS running →
      vTaskDelete(audioTaskHandle)
      audioTaskHandle = NULL
      Serial.println("🎵 Audio DSP Task stopped — RAM freed")
```

| Scenario | AudioDSPTask |
|----------|-------------|
| Boot, effect 0–21 or 33–62 | **Does not exist** — 0 RAM overhead |
| User selects effect 22–26 | **Created** on Core 0, priority 3, 8KB stack |
| User switches from audio effect to non-audio | **Deleted** — 8KB stack + ~1KB state freed |
| USB audio mirror active (`audioMirrorMode = true`) | Stays alive, skips I2S & FFT, just normalizes USB-provided `bandMagnitudes[]` |
| Auto-mode cycling (if any) | Created/deleted per frame as needed |

---

## AudioDSPTask Inner Loop

```cpp
void audioProcessingTask(void *pvParameters) {
    for (;;) {
        if (audioMirrorMode) {
            // USB mirror active — usbDataTask wrote bandMagnitudes[] from serial
            normalizeAudioLevels();
        } else {
            if (!calibrationComplete) { calibrateMicrophone(); continue; }
            checkAndRecalibrate();
            readI2SSamples();
            applyGain();
            FFT.compute(); FFT.complexToMagnitude();
            analyzeAudioBands();         // bandMagnitudes[8], levels, bandPeak[8]
            computeSpectralFeatures();   // spectralCentroid + spectralFlux [NEW]
            detectBeat();                // beatDetected + onsetMid + onsetHigh [NEW]
            normalizeAudioLevels();
            performAutoGainControl();
        }
        vTaskDelay(pdMS_TO_TICKS(2));    // ~75fps — independent of effectSpeed
    }
}
```

**Initial calibration** runs inside the task if `calibrationComplete == false` (happens at first boot or if `triggerMicCalibration` is set). Once done, calibration state persists in globals even after task deletion.

---

## New Audio Features (sensors.h / sensors.cpp)

| Feature | Variable | Type | Computation |
|---------|----------|------|-------------|
| **Spectral Centroid** | `spectralCentroid` | `volatile float` | `Σ(mag[i] × freq[i]) / Σ(mag[i])`, normalized 0–1 over 0–8kHz |
| **Spectral Flux** | `spectralFlux` | `volatile float` | Sum of positive ∆mag between frames ÷ total energy, ×4 gain, clamped 0–1. Requires `prevFluxMag[128]` static buffer |
| **Per-band Peak** | `bandPeak[8]` | `double[8]` | Running max per band, decays ×0.9993/frame, floor 0.01 |
| **Mid Onset** | `onsetMid` | `volatile bool` | True when midSum > baseline×1.25 + noise threshold |
| **High Onset** | `onsetHigh` | `volatile bool` | True when highSum > baseline×1.25 + noise threshold |

All computed from the existing FFT data. **No new libraries needed.** No mutex — volatile 32-bit types are atomic on ESP32, and a single frame of stale data is visually imperceptible.

---

## The 5 New Effects

Each adapted from the advanced project's 16-band system to Lumina's 8-band system. All effects read globals directly — **no `updateFrequencyDetection()` calls**.

### Effect 22 — Spectrum Ripple

| Source (16-band) | Lumina Equivalent |
|------------------|-------------------|
| `frame.bands[0..3]` (bass) | `(bandMagnitudes[0] + bandMagnitudes[1]) / 2.0` |
| `frame.bands[4..9]` (mid) | `(bandMagnitudes[2] + bandMagnitudes[3] + bandMagnitudes[4]) / 3.0` |
| `frame.bands[10..15]` (high) | `(bandMagnitudes[5] + bandMagnitudes[6] + bandMagnitudes[7]) / 3.0` |
| `frame.flux` | `spectralFlux` |
| `frame.centroid` | `spectralCentroid` |

`fadeToBlackBy(40)`. Bass drives warm orange (HSV 16) ripple width from center. Mid > 0.25 adds green (HSV 130) accent at ripple edge. High > 0.4 adds random white sparkles in outer quarter. `spectralFlux` > 0.3 triggers center burst colored by `spectralCentroid`.

### Effect 23 — Kinetic Plasma

| Source (16-band) | Lumina Equivalent |
|------------------|-------------------|
| `frame.volume` | `globalAudioLevel` |
| `frame.flux` | `spectralFlux` |
| `frame.centroid` | `spectralCentroid` |
| `frame.bands[10]` | `bandMagnitudes[5]` (highest band) |

`plasmaIndex += 2 + globalAudioLevel * 35`. Per LED: `inoise8(i * 12, plasmaIndex)` for organic noise. Brightness = `noiseValue * globalAudioLevel * (1.0 + spectralFlux * 2.0)`. Hue = `noiseValue + spectralCentroid * 100 + bandMagnitudes[5] * 40`.

### Effect 24 — Transient Pulse

| Source (16-band) | Lumina Equivalent |
|------------------|-------------------|
| `frame.onset.bass` | `beatDetected` |
| `frame.onset.mid` | `onsetMid` |
| `frame.onset.high` | `onsetHigh` |
| `frame.centroid` | `spectralCentroid` |
| `frame.volume` | `globalAudioLevel` |

`fadeToBlackBy(60)`. Bass onset (`beatDetected`): travel head starts at center, decays ×0.92/frame, colors LEDs at expanding radius with hue from `spectralCentroid`. Mid onset (`onsetMid`): flash center 5 LEDs green (HSV 130). High onset (`onsetHigh`): 2 random white pixels. Idle: breathing blue glow — hue = `160 + spectralCentroid * 60`, brightness modulated by `globalAudioLevel`.

### Effect 25 — Spectrum Bars

| Source (16-band) | Lumina Equivalent |
|------------------|-------------------|
| `frame.bands[0..15]` | `bandMagnitudes[0..7]` |
| `ledsPerBand = 9` (144/16) | `ledsPerBand = ledCount / 8` |
| `peakHold[b]`, `peakTimer[b]` | `bandPeak[b]` + local frame counter |

`fadeToBlackBy(15)`. Divide strip into 8 equal groups. Bar height = `bandMagnitudes[b] * ledsPerBand`. Hue sweeps 0→160 across bands (red→blue→green). Brightness gradient per bar: 80 at bottom → 255 at top. Peak dot: `bandPeak[b]` tracked by audio task, rendered as white dot at peak position, decays after 20 frames.

### Effect 26 — Spectral Verve

| Source (16-band) | Lumina Equivalent |
|------------------|-------------------|
| `frame.bands[0..15]` | `bandMagnitudes[0..7]` |
| `pos * 16` → bandIdx | `pos * 8` → bandIdx |
| `frame.flux` | `spectralFlux` |
| `frame.volume` | `globalAudioLevel` |
| `frame.centroid` | `spectralCentroid` |
| `frame.onset.bass/mid/high` | `beatDetected` / `onsetMid` / `onsetHigh` |

`scrollPhase += 0.3 + spectralFlux * 6.0`. Per LED: `bandIdx = (i / ledCount) * 8`. Brightness = `bandMagnitudes[bandIdx] * 0.7 + sineWave * 0.3` where sineWave is `sin16(pos * 480 + scrollPhase * 80)`. Hue flows from position, `spectralCentroid`, and scrollPhase. 3-zone onset overpower: left third full brightness on `beatDetected`, middle third on `onsetMid`, right third on `onsetHigh`.

---

## Files Modified

### File 1: `Lumina S3/include/sensors.h`

- Add: `extern volatile float spectralCentroid;`
- Add: `extern volatile float spectralFlux;`
- Add: `extern volatile bool onsetMid;`
- Add: `extern volatile bool onsetHigh;`
- Add: `extern double bandPeak[NUM_FREQ_BANDS];`
- Add: `void computeSpectralFeatures();` declaration

### File 2: `Lumina S3/src/sensors.cpp`

- Add new globals: `spectralCentroid`, `spectralFlux`, `onsetMid`, `onsetHigh`, `bandPeak[8]`, `prevFluxMag[128]`
- Add `computeSpectralFeatures()` — centroid + flux from `frequencyResponse[]`
- Enhance `detectBeat()` — add `onsetMid`/`onsetHigh` with baseline tracking (70/30 EMA)
- Add per-band peak tracking in `analyzeAudioBands()` — `bandPeak[b]` decay ×0.9993
- Add `audioProcessingTask(void *pvParameters)` — wraps the existing pipeline in an infinite loop with `vTaskDelay(2)`

### File 3: `Lumina S3/include/effects.h`

Replace lines 68–82 (11 old prototypes) with:

```cpp
void effectSpectrumRipple();     // 22
void effectKineticPlasma();      // 23
void effectTransientPulse();     // 24
void effectSpectrumBars();       // 25
void effectSpectralVerve();      // 26
```

### File 4: `Lumina S3/src/effects.cpp`

- Delete lines 863–1288 (old effects 22–32)
- Insert 5 new functions (~350 lines total)
- **No `updateFrequencyDetection()` calls** — effects read globals directly

### File 5: `Lumina S3/src/tasks.cpp`

In `updateLEDs()`:
- Add audio task lifecycle management (static bool for running state)
- Before the switch: create or delete `AudioDSPTask` based on `currentEffect`
- Update case labels: 22→`effectSpectrumRipple`, 23→`effectKineticPlasma`, 24→`effectTransientPulse`, 25→`effectSpectrumBars`, 26→`effectSpectralVerve`
- Remove cases 27–32

### File 6: `Lumina S3/include/globals.h`

- Add: `extern TaskHandle_t audioTaskHandle;`
- Add: `extern void audioProcessingTask(void *pvParameters);`

### File 7: `Lumina S3/src/setup_utils.cpp`

- Add: `TaskHandle_t audioTaskHandle = NULL;`
- Update task stack tracking table to include `AudioDSP`

### File 8: `Lumina S3/src/mqtt_integration.cpp`

Replace EFFECT_NAMES[22–32]:

```cpp
"Spectrum Ripple", "Kinetic Plasma", "Transient Pulse",
"Spectrum Bars", "Spectral Verve"
```

Remove entries 27–32 (6 entries removed). `NUM_EFFECTS` auto-calculated from `sizeof` — drops from 63 to 57.

---

## Files NOT Changed

| File | Reason |
|------|--------|
| `Lumina-Mini/` (any) | Mini has no mic, no audio effects in switch |
| `Lumina S3/src/main.cpp` | No new tasks in setup — AudioDSP is created dynamically in `updateLEDs()` |
| `Lumina S3/Tools/` | USB protocol unchanged |
| `platformio.ini` (both) | No new library dependencies |
| `Lumina S3/include/config.h` | I2S pins, LED pin, sample rate unchanged |

---

## Build & Verify

```powershell
Set-Location "Lumina S3"
pio run
```

Expected: 0 errors, 0 warnings. Stack impact: AudioDSPTask 8KB (only while audio effects are active, freed immediately when switching to non-audio effects).
