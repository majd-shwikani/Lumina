@echo off
echo --- Universal ESP32 Coredump Eraser ---
echo Identifying project and clearing partition...
echo.

python "%~dp0analyze_coredump.py" --clear-only

if %ERRORLEVEL% EQU 0 (
    echo.
    echo [SUCCESS] Flash cleared!
) else (
    echo.
    echo [ERROR] Failed to clear flash. Check connection and close Serial Monitor.
)

pause
