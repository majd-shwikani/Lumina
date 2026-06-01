
# 🚀 LUMINA MASTER PLAN: ADVANCED AUDIO INTEGRATION

## 🎯 OBJECTIVE
You are an AI coding assistant. Your task is to completely strip out the legacy audio processing from the "Lumina" smart home LED project and replace it 100% with the hardware-accelerated DSP audio engine and visualizers from the "Advanced Audio Reactive" project. 

Follow these instructions **exactly as written**. Do not improvise. Do not leave old audio logic behind.

---

## 🛑 PHASE 1: PURGE THE OLD AUDIO LOGIC

You must first remove all traces of the old microphone, calibration, and FFT logic from Lumina. 

### 1. `platformio.ini`
* **DELETE** `arduinoFFT` from the `lib_deps` list. 

### 2. `globals.h`
* **DELETE** all references to old audio variables: `bandMagnitudes`, `globalAudioLevel`, `beatDetected`, `beatEnergy`, `bassLevel`, `midLevel`, `trebleLevel`.
* **DELETE** the function declarations: `setupFrequencyDetection();`, `updateFrequencyDetection();`.

### 3. `setup_utils.cpp` / `main.cpp`
* **DELETE** any variables associated with the deleted globals above.
* **DELETE** the entire `setupFrequencyDetection()` function (including any microphone calibration logic inside it).
* **DELETE** the call to `setupFrequencyDetection();` inside `main.cpp`'s `setup()` function.

### 4. `effects.cpp` & `tasks.cpp`
* **DELETE** the old `updateFrequencyDetection()` function entirely.
* **DELETE** all old audio reactive effects (Effects 22 through 32) in `effects.cpp`.
* **REMOVE** the switch cases 22-32 in the `updateLEDs()` function inside `tasks.cpp`.

---

## 🏗️ PHASE 2: INJECT ADVANCED AUDIO GLOBALS

You must add the new hardware configuration, memory buffers, and structures from the Advanced project into Lumina.

### 1. `globals.h`
Add the following definitions, macros, and structures at the top of the file (below the includes). Include `"esp_dsp.h"` at the top of the file.

```cpp
#include "esp_dsp.h"
#include <math.h>

// --- ADVANCED AUDIO HARDWARE DEFINITIONS ---
#define I2S_WS_PIN       5
#define I2S_SD_PIN       4
#define I2S_SCK_PIN      2
#define I2S_PORT         I2S_NUM_0

#define FFT_SAMPLES      512
#define SAMPLING_FREQ    44100
#define NUM_BANDS        16
#define BIN_WIDTH        ((float)SAMPLING_FREQ / FFT_SAMPLES)

// --- CROSS-CORE AUDIO STRUCTURES ---
struct OnsetFlags {
    bool bass;
    bool mid;
    bool high;
};

struct AudioData {
    float bands[NUM_BANDS];
    float volume;
    OnsetFlags onset;
    float centroid;
    float flux;
};

// --- GLOBAL EXPORTS ---
extern __attribute__((aligned(16))) float fft_input_output[FFT_SAMPLES * 2];
extern __attribute__((aligned(16))) float window_coefficients[FFT_SAMPLES];
extern float prevMagnitudes[FFT_SAMPLES / 2];
extern int binToBand[FFT_SAMPLES / 2];

extern float bandEnergy[NUM_BANDS];
extern float bandSmoothed[NUM_BANDS];
extern float bandPeak[NUM_BANDS];

extern AudioData sharedAudio;

// Task declarations
void audioProcessingTask(void *pvParameters);
void initBandMapping();

```

### 2. `tasks.cpp` (Global Variable Initialization)

At the top of `tasks.cpp` (where variables are defined), define the memory buffers:

```cpp
__attribute__((aligned(16))) float fft_input_output[FFT_SAMPLES * 2];
__attribute__((aligned(16))) float window_coefficients[FFT_SAMPLES];
float prevMagnitudes[FFT_SAMPLES / 2] = {0};
int binToBand[FFT_SAMPLES / 2];

float bandEnergy[NUM_BANDS] = {0};
float bandSmoothed[NUM_BANDS] = {0};
float bandPeak[NUM_BANDS] = {0};

AudioData sharedAudio;

```

---

## ⚙️ PHASE 3: THE DSP AUDIO TASK

You will now copy the highly optimized `audioProcessingTask` and its helper `initBandMapping` directly into Lumina.

### 1. `tasks.cpp`

Copy the exact `initBandMapping()` function from the Advanced project and paste it above your task definitions.

```cpp
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
            int band = (int)(norm * (NUM_BANDS - 1) + 0.5f);
            binToBand[bin] = constrain(band, 0, NUM_BANDS - 1);
        }
    }
}

```

Copy the **ENTIRE** `audioProcessingTask(void *pvParameters)` from the Advanced project and paste it into `tasks.cpp`.

* *CRITICAL AI INSTRUCTION:* Lumina already has `i2sMutex`. In the `audioProcessingTask`, replace the variable `audioMutex` with `i2sMutex` anywhere it appears.

### 2. `main.cpp` (Setup integration)

Inside `setup()`, right before you create the other FreeRTOS tasks (Cloud, LED, IO), insert this:

```cpp
  Serial.println("🎤 [6/9] Initializing Advanced DSP Audio...");
  initBandMapping();
  
  // Create DSP task pinned to Core 0 (Priority 3 to ensure audio processing isn't starved)
  xTaskCreatePinnedToCore(audioProcessingTask, "AudioDSPTask", 8192, NULL, 3, NULL, 0);
  Serial.println("      ✅ I2S SIMD FFT task started on Core 0");

```

---

## 🎨 PHASE 4: PORTING THE ADVANCED EFFECTS

The Advanced project has 5 specific rendering functions. We need to move these into Lumina's `effects.cpp`.

### 1. Code Adaptation Rules for the AI:

* The Advanced project uses `#define NUM_LEDS 144`. Lumina uses a dynamic integer called `ledCount`.
* *CRITICAL:* When you copy the 5 rendering functions (`renderSpectrumRipple`, `renderKineticPlasma`, `renderTransientPulse`, `renderSpectrumBars`, `renderSpectralVerve`), you MUST change every instance of `NUM_LEDS` to `ledCount`.
* Change every instance of `audioMutex` to `i2sMutex`.

### 2. `effects.cpp`

Paste the 5 modified functions into `effects.cpp`. Rename them to match Lumina's naming convention:

* `renderSpectrumRipple()` -> `effectSpectrumRipple()`
* `renderKineticPlasma()` -> `effectKineticPlasma()`
* `renderTransientPulse()` -> `effectTransientPulse()`
* `renderSpectrumBars()` -> `effectSpectrumBars()`
* `renderSpectralVerve()` -> `effectSpectralVerve()`

*Example Modification for `effectSpectrumRipple()`:*

```cpp
void effectSpectrumRipple() {
    AudioData frame;
    if (xSemaphoreTake(i2sMutex, pdMS_TO_TICKS(2)) == pdTRUE) {
        frame = sharedAudio;
        xSemaphoreGive(i2sMutex);
    }

    fadeToBlackBy(leds, ledCount, 40);
    int center = ledCount / 2;
    // ... complete the rest of the function substituting NUM_LEDS with ledCount ...
}

```

*(Agent: Apply this logic strictly to all 5 functions).*

### 3. `tasks.cpp` (Effect Routing)

In the `updateLEDs()` function switch statement, add the new effects to slots 22-26:

```cpp
    // ---- Advanced Sound-Reactive Effects (22-26) -----------------------------
    case 22: effectSpectrumRipple();     break;
    case 23: effectKineticPlasma();      break;
    case 24: effectTransientPulse();     break;
    case 25: effectSpectrumBars();       break;
    case 26: effectSpectralVerve();      break;

```

---

## ✅ PHASE 5: VERIFICATION CHECKLIST FOR THE AI

Before outputting your code, verify:

1. Did you remove `arduinoFFT` from `platformio.ini`?
2. Are `NUM_LEDS` calculations fully replaced with `ledCount` in the visualizer functions?
3. Is `audioMutex` renamed to `i2sMutex`?
4. Is `setupFrequencyDetection()` completely gone from `main.cpp`?
5. Did you include `initBandMapping()` in `setup()`?

If all 5 are YES, you have successfully integrated the Advanced Audio Reactive engine into Lumina. Generate the code.

```

```