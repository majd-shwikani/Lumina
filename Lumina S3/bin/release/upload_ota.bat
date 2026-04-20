@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo         ESP32-S3 OTA FIRMWARE UPLOADER
echo ========================================================

:: 1. Handle IP Address File
set "IP_FILE=ipadress.txt"
if not exist "%IP_FILE%" (
    echo [INFO] %IP_FILE% not found. Creating...
    echo 192.168.1.100 > "%IP_FILE%"
    pause & exit /b
)

set /p DEVICE_IP=<"%IP_FILE%"
set "DEVICE_IP=%DEVICE_IP: =%"

echo [INFO] Targeting IP: %DEVICE_IP%

:: 2. Find firmware.bin
set "APP_BIN=firmware.bin"
if not exist "%APP_BIN%" set "APP_BIN=.pio\build\esp32-s3-devkitc-1\firmware.bin"

if not exist "%APP_BIN%" (
    echo [ERROR] firmware.bin not found!
    pause & exit /b
)

:: 3. Find espota.py
set "ESPOTA_PY="

:: Check common location without extra quotes in variable
if exist "%USERPROFILE%\.platformio\packages\framework-arduinoespressif32\tools\espota.py" (
    set "ESPOTA_PY=%USERPROFILE%\.platformio\packages\framework-arduinoespressif32\tools\espota.py"
)

if not defined ESPOTA_PY (
    echo [INFO] Searching for espota.py...
    for /r "%USERPROFILE%\.platformio\packages" %%F in (espota.py) do (
        if exist "%%F" (
            set "ESPOTA_PY=%%F"
            goto :found_ota
        )
    )
)

:found_ota
if not defined ESPOTA_PY (
    echo [ERROR] espota.py not found!
    pause & exit /b
)

echo [INFO] Tool: "%ESPOTA_PY%"
echo.

:: 4. Execute OTA Upload
:: Use quotes around the script path and the binary path
python "!ESPOTA_PY!" -i "%DEVICE_IP%" -f "%APP_BIN%"

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] OTA Upload failed.
) else (
    echo.
    echo [SUCCESS] OTA Upload Complete!
)

pause
