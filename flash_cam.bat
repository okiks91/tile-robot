@echo off
title AI-Thinker ESP32-CAM Flasher (COM10)
color 0E
echo ========================================================
echo   AI-THINKER ESP32-CAM FLASHER (COM10 @ 460800)
echo ========================================================
echo.
echo IMPORTANT: Make sure IO0 is connected to GND, then press RST.
echo.
pause
"C:\Users\johnf\AppData\Local\Programs\Python\Python311\Scripts\esptool.exe" --chip esp32 --port COM10 --baud 460800 write-flash 0x1000 "%~dp0build_cam\ESPCAM.ino.bootloader.bin" 0x8000 "%~dp0build_cam\ESPCAM.ino.partitions.bin" 0x10000 "%~dp0build_cam\ESPCAM.ino.bin"
echo.
if %ERRORLEVEL% EQU 0 (
    color 0A
    echo [SUCCESS] Camera board flashed successfully!
    echo Disconnect IO0 from GND and press RST to start the camera.
) else (
    color 0C
    echo [FAILED] Could not connect to ESP32 on COM10.
    echo Please make sure IO0 is connected to GND and press RST before flashing.
)
echo.
pause
