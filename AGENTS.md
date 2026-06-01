# Lumina Agent Instructions

Compact guide for OpenCode agents working on the Lumina multi-node LED system.

## Workflow & Verification
- **Permission First:** Before any `edit` or `write`, describe exactly what will be changed and ask for user permission.
- **Verification:** After every edit, navigate to the relevant project directory (`Lumina S3` or `Lumina-Mini`) and run `pio run` to verify the build. Fix any compilation errors immediately.
- **Commits:** After significant changes, ask the user if they want to commit.
- **Commit Format:** 
    - **Title:** Clear and concise.
    - **Description:** Minimum **200 words**. Explain the "why" and "how" of every change, covering architecture impacts, hardware considerations, and logic flow.

## Project Structure
- `Lumina S3/`: Gateway/Transmitter (ESP32-S3). Core controller for sensors, cloud, and ESP-NOW gateway.
- `Lumina-Mini/`: Receiver/Client (ESP32-C3). Simple node for LED animation.

## Developer Commands

### Build & Upload
Always run `pio` commands within the specific project directory (`Lumina S3` or `Lumina-Mini`).
```powershell
# Build
pio run

# Upload via Serial
pio run -t upload

# Monitor
pio run -t monitor
```

### Specialized Tools (Lumina S3/Tools/)
- `upload_s3.bat`: Strict firmware uploader (requires firmware.bin, bootloader.bin, partitions.bin).
- `upload_ota.bat`: Uploads via IP (requires `ipadress.txt`).
- `analyze_coredump.py`: Decodes ESP32 crash logs. Use `--clear-only` to wipe the partition.
- `lumina_sound_mirror.py`: PC -> S3 sound sync (2MBaud Serial).
- `lumina_screen_mirror.py`: PC -> S3 pixel sync (2MBaud Serial).

## Architecture & Protocols

### USB Data Protocol (2,000,000 Baud)
Header: `LUMI` (4 bytes) + Command (1 byte)
- `0xCC`: Handshake (S3 returns 2-byte LED count).
- `0xBB`: Pixel Stream (Expects `ledCount * 3` bytes).
- `0xAA`: Audio Stream (Expects 8 bytes of frequency band data).

### Communication Flow
1. **Cloud:** S3 syncs state with Firebase RTDB and MQTT.
2. **Local:** S3 broadcasts `LUMINA_CMD` messages via ESP-NOW to Mini nodes.
3. **Sync:** `LuminaMessage` struct must match exactly between S3 and Mini `globals.h`.

## Hardware Constraints
- **Lumina S3:** Uses PSRAM (`ps_malloc` for LED array). Pin 17 (LED), Pin 48 (Onboard), Pin 10 (Button).
- **Lumina-Mini:** ESP32-C3. Pin 7 (LED).
- **Sensors:** I2C for VEML7700 (Pins 8, 9) and INA219. Radar on Pin 14.

## Style & Conventions
- **FreeRTOS Tasks:** Heavy use of `xTaskCreatePinnedToCore`. S3 CloudTask (12KB stack), LEDTask (4KB).
- **Watchdog:** 30s timeout configured in `main.cpp`. Call `esp_task_wdt_reset()` in loops.
