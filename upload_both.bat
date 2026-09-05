@echo off
title Tile Robot Dual Board Uploader
color 0B
echo ========================================================
echo   TILE ROBOT - SIMULTANEOUS DUAL BOARD FLASH
echo   Motor Board: COM9  |  AI-Thinker ESP32-CAM: COM10
echo ========================================================
echo.
powershell.exe -ExecutionPolicy Bypass -File "%~dp0upload_both.ps1"
echo.
pause
