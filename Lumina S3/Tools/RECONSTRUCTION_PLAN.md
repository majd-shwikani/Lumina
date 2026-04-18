# Lumina LED Controller: Architectural Reconstruction & Stability Plan
**Version:** 1.0  
**Status:** PROPOSAL  
**Author:** Gemini CLI / Engineering Lead  
**Subject:** Resolution of Core 0 Starvation and Watchdog Failures (Crashes 3 & 4)

---

## 1. Executive Summary
Following a deep-dive analysis of recent coredumps, the Lumina firmware has been diagnosed with **Core 0 Congestion Syndrome**. The current architecture overloads Core 0 with high-priority networking tasks (WiFi, TCP/IP, Firebase, MQTT) while running low-priority sensor logic that holds critical system locks. This creates a "perfect storm" for Priority Inversion and Task Watchdog Timer (TWDT) timeouts.

This plan outlines a 3-phase transition to a high-reliability, asynchronous, dual-core architecture.

---

## 2. Root Cause Consolidated Analysis
The system is failing because the ESP32's Core 0 (the "Pro" core) is being asked to handle both the underlying infrastructure and the heavy application logic.

| Root Cause | Technical Impact |
| :--- | :--- |
| **Asymmetric Core Loading** | Core 0 usage often spikes to 100% during TLS handshakes, starving the IDLE task. |
| **Priority Inversion** | `SystemTask` (Priority 0) holds the I2C semaphore while being preempted by `CloudTask` (Priority 1), which then blocks waiting for the same semaphore. |
| **Blocking Network I/O** | `delay()` calls in MQTT discovery prevent the FreeRTOS scheduler from context switching during I/O waits. |
| **Semaphore Contention** | Multiple tasks calling `updateSensorData()` create "lock-stepping" where tasks wait on hardware rather than processing data. |

---

## 3. Phase 1: Immediate Stabilization (Critical Fixes)
*Target: Stop the resets within the next 24 hours.*

### 3.1 Core Migration
Relocate application-level tasks to Core 1 to isolate them from the WiFi/BT stack.
- **Move `CloudTask`** to Core 1.
- **Move `IOTask`** to Core 1.
- **Move `SystemTask`** to Core 1.
- *Keep Core 0 reserved strictly for WiFi, ESP-NOW, and System Events.*

### 3.2 Priority Realignment
Standardize priorities to ensure the Watchdog and critical background tasks are never starved.
- **Infrastructure (WiFi/IP):** 23 (System Default)
- **LED Animation:** 3 (Core 1)
- **Firebase Stream:** 3 (Core 0)
- **System Maintenance:** 2 (Core 1) — *Raised from 0*
- **Cloud/IO Tasks:** 1 (Core 1)

---

## 4. Phase 2: Architectural Refactoring (Pro-Level)
*Target: Professional-grade reliability and performance.*

### 4.1 Global Sensor Provider Pattern
Eliminate redundant I2C calls. Currently, `CloudTask` and `SystemTask` both poll the hardware.
- **Implementation:** Implement a "Producer-Consumer" model.
- `SystemTask` (Producer) polls sensors every 500ms and updates a global `SensorState` struct.
- `CloudTask` (Consumer) simply reads the last known good values from the struct.
- **Benefit:** Reduces I2C bus traffic by 50% and eliminates 100% of I2C semaphore contention between these tasks.

### 4.2 Non-Blocking Discovery & Setup
Refactor `src/mqtt_integration.cpp` to remove blocking logic.
- Replace all `delay(x)` with `vTaskDelay(pdMS_TO_TICKS(x))`.
- Wrap MQTT Discovery in a dedicated, low-priority initialization sequence to ensure it doesn't block the start of the LED animations.

### 4.3 I2C Resilience
- Implement a `SafeWire` wrapper with a 50ms timeout.
- If a sensor fails to respond, the system should mark it as "OFFLINE" in the telemetry and skip it, rather than hanging the task.

---

## 5. Phase 3: Reliability & Monitoring
*Target: Long-term observability.*

### 5.1 Enhanced Watchdog Management
Instead of just resetting the system, implement a "Soft Recovery" mode.
- If `CloudTask` hangs, the system should attempt to restart only that task before resorting to a full CPU reboot.

### 5.2 CPU Load Telemetry
Add a "Core Utilization" metric to the Firebase stats.
- Track the High Water Mark of the IDLE tasks to detect "near-starvation" events before they cause a crash.

---

## 6. Implementation Roadmap

### Step 1: `main.cpp` Update
```cpp
// Revised Task Pins
xTaskCreatePinnedToCore(systemTask, "SystemTask", 4000,  NULL, 2, &systemTaskHandle, 1); // Core 1
xTaskCreatePinnedToCore(cloudTask,  "CloudTask",  12288, NULL, 1, &cloudTaskHandle,  1); // Core 1
xTaskCreatePinnedToCore(ioTask,     "IOTask",     12288, NULL, 1, &ioTaskHandle,     1); // Core 1
```

### Step 2: `sensors.cpp` Refactor
```cpp
// Thread-safe sensor access
void updateSensorData() {
    // 1. Lock Mutex
    // 2. Poll hardware
    // 3. Update global struct
    // 4. Release Mutex
}
```

---

## 7. Conclusion
The Lumina system has outgrown its current "monolithic" task structure on Core 0. By implementing this Dual-Core isolation strategy, we move from a hobbyist-level implementation to a production-grade industrial IoT architecture. This plan eliminates the root causes of Crashes 3 and 4 and provides a scalable foundation for future features.

**Recommendation:** Proceed with Phase 1 migration immediately.
