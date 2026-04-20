@echo off
setlocal enabledelayedexpansion

:: --- DEFAULTS ---
set ADDR_BOOTLOADER=0x0
set ADDR_PARTITIONS=0x8000
set ADDR_FIRMWARE=0x10000
set BAUD=921600
set FLASH_MODE=dio
:: ----------------

echo ========================================================
echo         ESP32-S3 STRICT FIRMWARE UPLOADER
echo ========================================================

:: 1. Set Working Directory
set "WORK_DIR=%~dp0"
if "%~1" NEQ "" (
    if not exist "%~1\" ( set "WORK_DIR=%~dp1" ) else ( set "WORK_DIR=%~f1" )
)
if "%WORK_DIR:~-1%"=="\" set "WORK_DIR=%WORK_DIR:~0,-1%"

:: 2. Dynamic Partition Offset
if exist "!WORK_DIR!\partitions.csv" (
    for /f "usebackq tokens=1,4 delims=," %%A in ("!WORK_DIR!\partitions.csv") do (
        set "p_name=%%A"
        set "p_offset=%%B"
        set "p_name=!p_name: =!"
        set "p_offset=!p_offset: =!"
        if /i "!p_name!"=="app0" (
            set ADDR_FIRMWARE=!p_offset!
            echo [INFO] Dynamic App Offset: !ADDR_FIRMWARE!
        )
    )
)

:: 3. Strict File Validation (Requires all 3)
set "APP_BIN=!WORK_DIR!\firmware.bin"
set "BOOT_BIN=!WORK_DIR!\bootloader.bin"
set "PART_BIN=!WORK_DIR!\partitions.bin"

if "%~1" NEQ "" if not exist "%~1\" set "APP_BIN=%~1"

set "MISSING="
if not exist "!APP_BIN!" set "MISSING=!MISSING! firmware.bin"
if not exist "!BOOT_BIN!" set "MISSING=!MISSING! bootloader.bin"
if not exist "!PART_BIN!" set "MISSING=!MISSING! partitions.bin"

if defined MISSING (
    echo [ERROR] Missing required firmware components:
    echo        !MISSING!
    echo.
    echo Please ensure all three .bin files are in: 
    echo "!WORK_DIR!"
    echo.
    pause
    exit /b
)

:: 4. Find esptool
set "CMD="
python -m esptool version >nul 2>&1 && set "CMD=python -m esptool"
if not defined CMD (esptool version >nul 2>&1 && set "CMD=esptool")
if not defined CMD (echo [ERROR] esptool not found! & pause & exit /b)

echo [INFO] Found all components. Starting flash...
echo.

:: 5. Execute Flash (All 3 files)
set "FLASH_ARGS=--chip esp32s3 --baud %BAUD% --before default_reset --after hard_reset write_flash -z --flash_mode %FLASH_MODE% --flash_freq 80m --flash_size detect"

%CMD% %FLASH_ARGS% %ADDR_BOOTLOADER% "!BOOT_BIN!" %ADDR_PARTITIONS% "!PART_BIN!" %ADDR_FIRMWARE% "!APP_BIN!"

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Upload failed.
) else (
    echo.
    echo [SUCCESS] Upload Complete!
)

pause
