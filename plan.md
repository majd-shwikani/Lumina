# STRICT AI AGENT EXECUTION PROTOCOL: LUMINA FASTLED CONCURRENCY FIX

## ⚠️ SYSTEM DIRECTIVE FOR AI AGENT
You are an AI coding assistant. You are about to refactor the Lumina LED Controller codebase to fix a critical RTOS hardware crash. **You must assume you are acting on a delicate system. You are forbidden from improvising.** Read the rules, understand the architecture, and execute the steps linearly from Module 1 to Module 7. Do not skip steps. Do not jump to the end. Do not write code outside of these specific instructions.

### 🛑 ABSOLUTE RULES OF ENGAGEMENT
1. **ZERO IMPROVISATION:** Do not optimize, reformat, or "clean up" surrounding code. Change only the lines explicitly targeted in this document.
2. **NO EXTERNAL LIBRARIES:** You will solve this using standard `volatile bool` flags. Do not import `FreeRTOS` mutexes, queues, or other libraries to solve the LED issue.
3. **ISOLATION OF POWER:** The functions `FastLED.show()`, `FastLED.clear()`, and `FastLED.setBrightness()` are now **BANNED** in all files EXCEPT `tasks.cpp` and the initial setup block in `main.cpp`.
4. **EXACT MATCH REPLACEMENT:** Use the exact variable names and code snippets provided below.

---

## 🧠 THE ARCHITECTURAL SHIFT: "DEFERRED EXECUTION"
**The Problem:** The ESP32-S3's RMT driver freezes because `mqttCallback`, `toggleUsbMirror`, and `sinricPro` callbacks (running in `IOTask` or `CloudTask`) are calling `FastLED.show()` at the exact same time that `ledTask` is calling it.
**The Solution:** Callbacks are no longer allowed to touch FastLED. Instead, when a network command arrives, the callback will set a `volatile bool` flag to `true`. The `ledTask` (which owns the hardware) will check these flags at the start of its loop, execute the FastLED commands safely, and reset the flags to `false`.

---

## 🛠️ EXECUTION MANUAL: FROM A TO Z

### MODULE 1: Define the Global Hardware Flags
We must establish the global flags that network tasks will use to communicate with the LED task.

**Target File 1:** `globals.h`
* **Action:** Add the following external declarations at the bottom of your global variables list.
```cpp
// --- FASTLED DEFERRED EXECUTION FLAGS ---
extern volatile bool flag_forceLedClear;
extern volatile bool flag_forceBrightnessUpdate;
extern volatile bool flag_forceEffectUpdate;
Target File 2: setup_utils.cpp

Action: Define the actual variables near your other volatile globals (like volatile bool stripEnabled).

C++
// --- FASTLED DEFERRED EXECUTION FLAGS ---
volatile bool flag_forceLedClear = false;
volatile bool flag_forceBrightnessUpdate = false;
volatile bool flag_forceEffectUpdate = false;
MODULE 2: Sanitize MQTT Callbacks
Network commands cannot touch the LED strip directly.

Target File: mqtt_integration.cpp

Action 1: Find the /light/cmd block for isGatewayTarget.

Find this line: if (!newState) { FastLED.clear(); FastLED.show(); }

Replace exactly with: if (!newState) { flag_forceLedClear = true; }

Action 2: Find the /brightness/cmd block for isGatewayTarget.

Find this line: FastLED.setBrightness(brightness);

Replace exactly with: flag_forceBrightnessUpdate = true;

Action 3: Find the /effect/cmd block.

Find this line: currentEffect = newEffect;

Add immediately below it: flag_forceEffectUpdate = true;

MODULE 3: Sanitize Smart Home (SinricPro)
SinricPro runs in the IO task and causes identical crashes.

Target File: smart_home.cpp

Action 1: Locate the onBrightness function.

Find this line: FastLED.setBrightness(br);

Replace exactly with: globalBrightness = br; flag_forceBrightnessUpdate = true;

Action 2: Check onPowerState and onColor. Ensure there are absolutely zero calls to FastLED.show() or FastLED.clear(). If they exist, delete them and rely on the flags.

MODULE 4: Sanitize Firebase
Firebase streams trigger updates asynchronously.

Target File: firebase_functions.cpp

Action 1: Locate streamCallback, specifically under if (subPath == "/brightness").

Find this line: FastLED.setBrightness(globalBrightness);

Replace exactly with: flag_forceBrightnessUpdate = true;

Action 2: In the same function, under subPath == "/enabled", ensure FastLED.clear() is NOT called. (If it is, replace it with flag_forceLedClear = true;).

MODULE 5: Sanitize USB Mirror Modes
Turning off mirror mode currently force-clears the strip directly from the USB task.

Target File: setup_utils.cpp

Action 1: Locate the toggleUsbMirror function.

Find this block:

C++
// Clear LEDs when exiting mirror mode
FastLED.clear();
FastLED.show();
Replace exactly with:

C++
// Safely signal the LED task to clear the strip
flag_forceLedClear = true;
MODULE 6: The Master LED Task Refactor
Now we rewrite the core LED loop to read these flags and safely apply the hardware updates within the single allowed thread.

Target File: tasks.cpp

Action 1: Locate the updateLEDs() function.

Action 2: Rewrite the top portion of the function to safely consume the flags before calculating any effects.

Instruction: Keep the stripMux logic, but inject the flag handling immediately after.

Copy and paste this EXACT structure for the top of updateLEDs():

C++
void updateLEDs() {
  static bool lastStripEnabled = true;
  
  bool currentEnabled;
  portENTER_CRITICAL(&stripMux);
  currentEnabled = stripEnabled;
  portEXIT_CRITICAL(&stripMux);

  // ==========================================================
  // DEFERRED HARDWARE EXECUTION BLOCK (Thread-Safe)
  // ==========================================================
  if (flag_forceBrightnessUpdate) {
    FastLED.setBrightness(globalBrightness);
    flag_forceBrightnessUpdate = false;
  }

  if (flag_forceLedClear) {
    FastLED.clear();
    FastLED.show();
    flag_forceLedClear = false;
  }
  
  if (flag_forceEffectUpdate) {
    FastLED.clear(); // Clear trails from previous effects
    flag_forceEffectUpdate = false;
  }
  // ==========================================================
  
  if (!currentEnabled && lastStripEnabled) {
    FastLED.clear();
    FastLED.show();
    lastStripEnabled = false;
    return;
  }
  
  if (!currentEnabled) {
    return;
  }
  
  if (currentEnabled && !lastStripEnabled) {
    lastStripEnabled = true;
  }

  // ... [Leave the rest of the Audio DSP Lifecycle and switch(currentEffect) EXACTLY AS IT WAS] ...
MODULE 7: Safe DSP Task Deletion
You cannot aggressively delete an audio task if it is holding a hardware mutex or reading from I2S. This causes secondary driver panics.

Target File: tasks.cpp

Action 1: Inside updateLEDs(), locate the block that deletes the Audio DSP Task.

Find this block:

C++
if (audioTaskHandle != NULL) {
  vTaskDelete(audioTaskHandle);
  audioTaskHandle = NULL;
}
Replace exactly with this safe shutdown sequence:

C++
if (audioTaskHandle != NULL) {
  // Wait briefly so the DSP task releases I2S and mutexes safely
  vTaskDelay(pdMS_TO_TICKS(50)); 
  vTaskDelete(audioTaskHandle);
  audioTaskHandle = NULL;

  // Uninstall the driver so it can be re-installed cleanly later
  i2s_driver_uninstall(I2S_PORT);
}
🔎 FINAL COMPLIANCE VERIFICATION
AI AGENT: You must verify your work. Run a conceptual grep across the codebase.
If you see FastLED.show(), FastLED.clear(), or FastLED.setBrightness() inside mqtt_integration.cpp, smart_home.cpp, setup_utils.cpp, or firebase_functions.cpp, YOU HAVE FAILED. These three functions are now strictly quarantined to tasks.cpp (and the setup() function in main.cpp). All other files MUST use the flag_ variables.

Execute this plan immediately.