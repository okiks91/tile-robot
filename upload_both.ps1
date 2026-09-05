# PowerShell Script: High-Speed Simultaneous Dual Board Uploader
# Board 1: Normal ESP32 Motor Controller on COM9 (esp32:esp32:esp32)
# Board 2: AI-Thinker ESP32-CAM on COM10 (esp32:esp32:esp32cam)

param (
    [string]$MotorPort = "COM9",
    [string]$CamPort   = "COM10"
)

$cliPath = "C:\Users\johnf\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
$espTool = "C:\Users\johnf\AppData\Local\Programs\Python\Python311\Scripts\esptool.exe"
$baseDir = Split-Path -Parent $MyInvocation.MyCommand.Path

$motorBuildDir = Join-Path $baseDir "build_motor"
$camBuildDir   = Join-Path $baseDir "build_cam"

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host ">>> TILE ROBOT: HIGH-SPEED PARALLEL DUAL FLASH <<<" -ForegroundColor Cyan
Write-Host " Motor Board Port : $MotorPort (Normal ESP32)" -ForegroundColor Yellow
Write-Host " Camera Board Port: $CamPort (AI-Thinker ESP32-CAM)" -ForegroundColor Yellow
Write-Host "========================================================" -ForegroundColor Cyan

# 1. Compile Motor Board
Write-Host "`n[1/3] Compiling Motor Board Firmware..." -ForegroundColor Green
$motorSketch = Join-Path $baseDir "main"
& "$cliPath" compile --fqbn esp32:esp32:esp32 --output-dir "$motorBuildDir" "$motorSketch"
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Motor board compilation failed!" -ForegroundColor Red
    exit 1
}
Write-Host "  -> Motor Board Compiled Successfully!" -ForegroundColor Green

# 2. Compile Camera Board
Write-Host "`n[2/3] Compiling Camera Board Firmware..." -ForegroundColor Green
$camSketch = Join-Path $baseDir "ESPCAM"
& "$cliPath" compile --fqbn esp32:esp32:esp32cam --output-dir "$camBuildDir" "$camSketch"
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Camera board compilation failed!" -ForegroundColor Red
    exit 1
}
Write-Host "  -> Camera Board Compiled Successfully!" -ForegroundColor Green

# 3. Launch Simultaneous Parallel Flashing
Write-Host "`n[3/3] Launching Parallel Uploads to $MotorPort and $CamPort..." -ForegroundColor Cyan

$motorJob = Start-Job -ScriptBlock {
    param($tool, $port, $dir)
    Write-Host "[MOTOR] Flashing $port at 921600 baud..."
    $bootloader = Join-Path $dir "main.ino.bootloader.bin"
    $partitions = Join-Path $dir "main.ino.partitions.bin"
    $appBin     = Join-Path $dir "main.ino.bin"
    & "$tool" --chip esp32 --port "$port" --baud 921600 write-flash 0x1000 "$bootloader" 0x8000 "$partitions" 0x10000 "$appBin"
    return $LASTEXITCODE
} -ArgumentList $espTool, $MotorPort, $motorBuildDir

$camJob = Start-Job -ScriptBlock {
    param($tool, $port, $dir)
    Write-Host "[CAM] Flashing $port at 460800 baud..."
    $bootloader = Join-Path $dir "ESPCAM.ino.bootloader.bin"
    $partitions = Join-Path $dir "ESPCAM.ino.partitions.bin"
    $appBin     = Join-Path $dir "ESPCAM.ino.bin"
    & "$tool" --chip esp32 --port "$port" --baud 460800 write-flash 0x1000 "$bootloader" 0x8000 "$partitions" 0x10000 "$appBin"
    return $LASTEXITCODE
} -ArgumentList $espTool, $CamPort, $camBuildDir

Write-Host "  -> Uploading to both boards simultaneously. Please wait..." -ForegroundColor Yellow

$jobs = @($motorJob, $camJob)
Wait-Job $jobs | Out-Null

Write-Host "`n========================================================" -ForegroundColor Cyan
Write-Host ">>> UPLOAD RESULTS <<<" -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan

$motorOutput = Receive-Job $motorJob
$camOutput   = Receive-Job $camJob

Write-Host "`n--- MOTOR BOARD ($MotorPort) ---" -ForegroundColor Magenta
$motorOutput | Out-String | Write-Host

Write-Host "`n--- CAMERA BOARD ($CamPort) ---" -ForegroundColor Magenta
$camOutput | Out-String | Write-Host

Remove-Job $jobs
