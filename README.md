# Tile Robot

Autonomous and teleoperated multi-microcontroller tile cleaning robot system powered by three coordinated ESP32 microcontrollers.

---

## System Architecture

The robot coordinates three dedicated ESP32 modules over a local high-speed ad-hoc Wi-Fi network:

```
                      +-----------------------------+
                      |   Client Web Browser UI     |
                      | (Phone / Tablet / Computer) |
                      +--------------+--------------+
                                     |
                           Wi-Fi (192.168.4.1)
                                     v
+-------------------------------------------------------------------------+
| ESP #1: Web Host & Mechanism Controller (`web_host/`)                   |
| - Chip: ESP32                                                           |
| - Network: SoftAP "robot control" (192.168.4.1)                         |
| - Services: Captive DNS, Web Server (Port 80), WebSockets Hub (Port 81) |
| - Functions: Controls cleaning motors (L298N) and spray pump relay      |
| - Network Bridge: Low-latency UDP packet bridge (Port 8888 / 8889)      |
+-------------------+---------------------------------+-------------------+
                    |                                 |
         UDP Drive Commands (8888)            MJPEG Video Stream
         UDP Telemetry Back (8889)               (192.168.4.3)
                    |                                 |
                    v                                 v
+------------------------------------+  +--------------------------------+
| ESP #2: Motor Controller (`main/`) |  | ESP #3: Video Stream (`ESPCAM/`)|
| - Chip: ESP32-S3 (or ESP32)        |  | - Chip: AI-Thinker ESP32-CAM   |
| - Functions: Motor PWM drive,      |  | - IP: 192.168.4.3 (Static)     |
|   4x Ultrasonic distance sensors,  |  | - Camera: OV2640 (SVGA 800x600)|
|   obstacle avoidance, watchdog     |  | - Output: Low-latency /stream  |
+------------------------------------+  +--------------------------------+
```

---

## The 3 Firmware Packages

### 1. Motor Controller (`main/main.ino`)
- **Target Board**: ESP32-S3 (or ESP32)
- **Primary Function**: Drive motor PWM control, 4-direction obstacle avoidance, safety watchdog.
- **Ultrasonic Pin Configuration**:
  | Sensor Direction | Trigger Pin (TRIG) | Echo Pin (ECHO) |
  | ---------------- | ------------------ | --------------- |
  | **Front**        | `GPIO 5`           | `GPIO 15`       |
  | **Left**         | `GPIO 9`           | `GPIO 8`        |
  | **Right**        | `GPIO 4`           | `GPIO 2`        |
  | **Rear**         | `GPIO 16`          | `GPIO 17`       |
- **Motor Control Pins**:
  - Left Motor: `GPIO 10` (Forward / RPWM), `GPIO 11` (Reverse / LPWM)
  - Right Motor: `GPIO 12` (Forward / RPWM), `GPIO 13` (Reverse / LPWM)

### 2. Web Host & Mechanism Controller (`web_host/`)
- **Files**: `web_host/web_host.ino`, `web_host/web_ui.h`
- **Target Board**: ESP32 Dev Module
- **Primary Function**:
  - Broadcasts the Wi-Fi Access Point `robot control`.
  - Serves responsive web interface (`web_ui.h`) featuring touch controls, telemetry HUD, camera feed, and auto/manual toggle.
  - Controls cleaning motors (L298N H-Bridge) and high-pressure water spray pump via relay.
  - Relays drive packets via UDP to the Motor Controller.

### 3. Dedicated Camera Streamer (`ESPCAM/ESPCAM.ino`)
- **Target Board**: AI-Thinker ESP32-CAM
- **Primary Function**:
  - Connects to `robot control` Wi-Fi AP with static IP `192.168.4.3`.
  - Streams real-time MJPEG video (800x600 SVGA) from the OV2640 camera module via `http://192.168.4.3/stream`.

---

## Required Libraries

Ensure the following libraries are installed in your Arduino environment:
- `WebSockets` by Markus Sattler (v2.4.1+)
- `ArduinoOTA` (built into ESP32 Arduino Core)
- `esp32-camera` (included with ESP32 board support)
- `WiFi`, `WiFiUdp`, `WebServer`, `DNSServer` (built into ESP32 Arduino Core)

---

## Board FQBN Reference (Arduino CLI)

- **Motor Controller**: `esp32:esp32:esp32s3` (or `esp32:esp32:esp32`)
- **Web Host**: `esp32:esp32:esp32`
- **Camera Board**: `esp32:esp32:esp32cam`
