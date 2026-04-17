@echo off
set PORT=COM6
set ADDRESS=0xFF0000
set SIZE=0x10000

echo --- ESP32 Coredump Eraser ---
echo Port: %PORT%
echo Address: %ADDRESS%
echo.

python -m esptool --port %PORT% erase_region %ADDRESS% %SIZE%

if %ERRORLEVEL% EQU 0 (
    echo.
    echo [SUCCESS] Coredump partition has been cleared!
) else (
    echo.
    echo [ERROR] Failed to clear partition. Make sure Serial Monitor is closed.
)

pause
