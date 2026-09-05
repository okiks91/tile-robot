// ============================================================================
// Tile Robot — Web Host & Mechanism Controller (ESP32)
// WiFi Access Point Mode + DNS Server for tile.robot.control
// Central WebSockets Hub (Port 81) + Sub-millisecond UDP Drive Bridge
// Hosts Full Responsive Web UI + Controls Cleaning Motors & Spray Relay
// ============================================================================

#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "web_ui.h"

// ===========================
// WiFi Access Point Configuration
// ===========================
const char *ap_ssid = "robot control";
// No password (open network)

// AP IP Configuration
IPAddress ap_IP(192, 168, 4, 1);
IPAddress ap_gateway(192, 168, 4, 1);
IPAddress ap_subnet(255, 255, 255, 0);

// ===========================
// DNS Server (resolves tile.robot.control -> 192.168.4.1)
// ===========================
DNSServer dnsServer;
const byte DNS_PORT = 53;

// ===========================
// UDP Drive Bridge (ESP #3 <-> ESP #1 Motor Controller)
// ===========================
WiFiUDP udp;
const int UDP_DRIVE_PORT = 8888; // ESP #1 listens on 8888
const int UDP_TELEM_PORT = 8889; // ESP #3 listens on 8889
IPAddress broadcastIP(192, 168, 4, 255);

// ===========================
// WebSockets Server (Port 81) & AUTO Clean Loop (30s Clean / 10s Rest)
// ===========================
WebSocketsServer webSocket = WebSocketsServer(81);

bool hostAutoMode = false;
enum AutoCleanState { AUTO_CLEAN_RUNNING, AUTO_CLEAN_RESTING };
AutoCleanState autoCleanState = AUTO_CLEAN_RUNNING;
unsigned long autoCleanStateStartTime = 0;
const unsigned long AUTO_CLEAN_DURATION = 30000; // 30s CLEAN
const unsigned long AUTO_REST_DURATION  = 10000; // 10s REST

void setCleanMotors(bool active);

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.printf("[WS] Client #%u Connected\n", num);
      break;
    case WStype_DISCONNECTED:
      Serial.printf("[WS] Client #%u Disconnected\n", num);
      // Send safety stop to motor controller
      udp.beginPacket(broadcastIP, UDP_DRIVE_PORT);
      udp.write((const uint8_t *)"X", 1);
      udp.endPacket();
      break;
    case WStype_TEXT: {
      String msg = String((char *)payload).substring(0, length);
      if (msg == "M:auto") {
        hostAutoMode = true;
        autoCleanState = AUTO_CLEAN_RUNNING;
        autoCleanStateStartTime = millis();
        setCleanMotors(true);
        Serial.println("[AUTO] Mode ON -> 30s CLEAN / 10s REST loop started");
      } else if (msg == "M:manual") {
        hostAutoMode = false;
        setCleanMotors(false);
        Serial.println("[AUTO] Mode OFF -> Switched to MANUAL");
      }

      // Forward command to Motor Controller over UDP (<1ms)
      udp.beginPacket(broadcastIP, UDP_DRIVE_PORT);
      udp.write(payload, length);
      udp.endPacket();
      break;
    }
    default:
      break;
  }
}

// ===========================
// SPRAY Relay Pin (GPIO 33 - Active LOW / Pulled to GND)
// ===========================
const int RELAY_PIN = 33;
unsigned long sprayEndTime = 0;
bool sprayActive = false;

// ===========================
// L298N Cleaning Motor Pins (Mechanism: Counter-Rotating Opposite Direction)
// ===========================
const int CLEAN_IN1        = 23;  // Motor 1 Direction A
const int CLEAN_IN2        = 22;  // Motor 1 Direction B
const int CLEAN_IN3        = 21;  // Motor 2 Direction A (Opposite)
const int CLEAN_IN4        = 19;  // Motor 2 Direction B (Opposite)
// Cleaning Motors State
bool cleanActive           = false;
bool cleanWaitingForSpray  = false;

/*
// ============================================================================
// DISABLED / COMMENTED OUT: Lift Motor & SG90 Servo Mechanisms (UP / UPP / DOWN)
// ============================================================================
const int SERVO_PIN        = 13;
const int SERVO_PWM_FREQ   = 50;
const int SERVO_PWM_RES    = 14;
const int SERVO_UP_ANGLE   = 68;
const int SERVO_DOWN_ANGLE = 165;
const int AUX_ENA_PIN      = 14;
const int AUX_IN1_PIN      = 26;
const int AUX_IN2_PIN      = 27;
const int AUX_PWM_FREQ     = 1000;
const int AUX_PWM_RES      = 8;
const int AUX_SPEED_82     = 209;

enum AuxSequence { SEQ_NONE, SEQ_RUNNING_UP, SEQ_RUNNING_DOWN };
volatile AuxSequence currentSeq = SEQ_NONE;
unsigned long seqStartTime = 0;
bool servoStepDone = false;
bool motorRevStepDone = false;

void setServoAngle(int angle) {}
void auxMotorForward(int speed) {}
void auxMotorReverse(int speed) {}
void auxMotorStop() {}
void setUppMotor(bool active) {}
void startUpSequence() {}
void startDownSequence() {}
void stopSequence() {}
void handleSequenceLoop() {}
// ============================================================================
*/

// ===========================
// Web Server (Port 80)
// ===========================
WebServer server(80);

// ===========================
// Hardware Driver Functions
// ===========================

void stopCleaningMotorsHardware() {
  digitalWrite(CLEAN_IN1, LOW);
  digitalWrite(CLEAN_IN2, LOW);
  digitalWrite(CLEAN_IN3, LOW);
  digitalWrite(CLEAN_IN4, LOW);
}

void startCleaningMotorsHardware() {
  digitalWrite(CLEAN_IN1, HIGH);
  digitalWrite(CLEAN_IN2, LOW);
  digitalWrite(CLEAN_IN3, HIGH);
  digitalWrite(CLEAN_IN4, LOW);
}

// Spray Relay Control (Pull to GND for durationMs)
void activateSpray(unsigned long durationMs) {
  sprayActive = true;
  sprayEndTime = millis() + durationMs;
  digitalWrite(RELAY_PIN, LOW); // Pull to GND (Relay Active)
  Serial.printf("[SPRAY RELAY] Active (GND) for %lu ms\n", durationMs);
}

void stopSpray() {
  sprayActive = false;
  digitalWrite(RELAY_PIN, HIGH); // Release to HIGH (Relay Inactive)
  Serial.println("[SPRAY RELAY] Inactive (HIGH)");
}

void handleRelayLoop() {
  if (sprayActive && millis() >= sprayEndTime) {
    stopSpray();

    // If CLEAN was waiting for the spray to finish, start rotation now!
    if (cleanActive && cleanWaitingForSpray) {
      cleanWaitingForSpray = false;
      startCleaningMotorsHardware();
      Serial.println("[CLEAN] Phase 2: Spray complete -> Cleaning motors STARTED!");
    }
  }
}

// Cleaning Motors Control: Sprays for 3s first, then starts counter-rotation
void setCleanMotors(bool active) {
  cleanActive = active;
  if (active) {
    // 1. Ensure motors are stopped during initial spray
    stopCleaningMotorsHardware();
    // 2. Start 3-second spray FIRST
    cleanWaitingForSpray = true;
    activateSpray(3000);
    Serial.println("[CLEAN] Phase 1: Spray started for 3s (Motors waiting...)");
  } else {
    // Stop cleaning & cancel spray wait
    cleanWaitingForSpray = false;
    stopCleaningMotorsHardware();
    Serial.println("[CLEAN] OFF -> Motors stopped");
  }
}

// ===========================
// HTTP Request Handlers
// ===========================
void handleRoot() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/html", INDEX_HTML);
}

void handleSpray() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  // SPRAY button pulls GPIO 33 to GND for 5 seconds
  activateSpray(5000);
  server.send(200, "application/json", "{\"status\":\"ok\",\"spray\":true,\"duration\":5000}");
}

void handleClean() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (server.hasArg("state")) {
    String st = server.arg("state");
    if (st == "on" || st == "1") setCleanMotors(true);
    else if (st == "off" || st == "0") setCleanMotors(false);
    else if (st == "toggle") setCleanMotors(!cleanActive);
  } else {
    setCleanMotors(!cleanActive);
  }
  server.send(200, "application/json", String("{\"clean\":") + (cleanActive ? "true" : "false") + ",\"spray\":" + (sprayActive ? "true" : "false") + "}");
}

void handleStatus() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  String json = "{";
  json += "\"ip\":\"" + WiFi.softAPIP().toString() + "\",";
  json += "\"ap_ssid\":\"robot control\",";
  json += "\"clients\":" + String(WiFi.softAPgetStationNum()) + ",";
  json += "\"clean\":" + String(cleanActive ? "true" : "false") + ",";
  json += "\"spray\":" + String(sprayActive ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

void handleCmd() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  String action = server.arg("action");
  if (action == "stop") {
    udp.beginPacket(broadcastIP, UDP_DRIVE_PORT);
    udp.write((const uint8_t *)"X", 1);
    udp.endPacket();
  } else if (action == "steer") {
    int angle = server.hasArg("angle") ? server.arg("angle").toInt() : 0;
    int throttle = server.hasArg("throttle") ? server.arg("throttle").toInt() : 0;
    String pkt = "D:" + String(angle) + "," + String(throttle);
    udp.beginPacket(broadcastIP, UDP_DRIVE_PORT);
    udp.write((const uint8_t *)pkt.c_str(), pkt.length());
    udp.endPacket();
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleAutoCleanLoop() {
  if (!hostAutoMode) return;

  unsigned long now = millis();
  if (autoCleanState == AUTO_CLEAN_RUNNING) {
    if (now - autoCleanStateStartTime >= AUTO_CLEAN_DURATION) {
      // 30s Clean completed -> Switch to 10s REST
      autoCleanState = AUTO_CLEAN_RESTING;
      autoCleanStateStartTime = now;
      stopCleaningMotorsHardware();
      cleanActive = false;
      cleanWaitingForSpray = false;
      Serial.println("[AUTO CYCLE] 30s Clean complete -> 10s REST phase started");
    }
  } else if (autoCleanState == AUTO_CLEAN_RESTING) {
    if (now - autoCleanStateStartTime >= AUTO_REST_DURATION) {
      // 10s Rest completed -> Restart 30s CLEAN
      autoCleanState = AUTO_CLEAN_RUNNING;
      autoCleanStateStartTime = now;
      setCleanMotors(true);
      Serial.println("[AUTO CYCLE] 10s Rest complete -> 30s CLEAN phase restarted");
    }
  }
}

void handleMode() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (server.hasArg("set")) {
    String m = server.arg("set");
    if (m == "auto") {
      hostAutoMode = true;
      autoCleanState = AUTO_CLEAN_RUNNING;
      autoCleanStateStartTime = millis();
      setCleanMotors(true);
      Serial.println("[AUTO] Mode ON via HTTP -> 30s CLEAN / 10s REST loop started");
    } else if (m == "manual") {
      hostAutoMode = false;
      setCleanMotors(false);
      Serial.println("[AUTO] Mode OFF via HTTP -> Switched to MANUAL");
    }
    String pkt = "M:" + m;
    udp.beginPacket(broadcastIP, UDP_DRIVE_PORT);
    udp.write((const uint8_t *)pkt.c_str(), pkt.length());
    udp.endPacket();
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleNotFound() {
  server.sendHeader("Location", "http://tile.robot.control/", true);
  server.send(302, "text/plain", "Redirecting to tile.robot.control");
}

// ===========================
// Setup
// ===========================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n========================================================");
  Serial.println(">>> Tile Robot Web Host & Central Hub (ESP32) <<<");
  Serial.println("========================================================\n");

  // 1. Setup SPRAY Relay Pin (GPIO 33) - Inactive HIGH on boot
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Default OFF (not grounded)

  // 2. Setup L298N Cleaning Motor Pins - Stopped on boot
  pinMode(CLEAN_IN1, OUTPUT);
  pinMode(CLEAN_IN2, OUTPUT);
  pinMode(CLEAN_IN3, OUTPUT);
  pinMode(CLEAN_IN4, OUTPUT);
  setCleanMotors(false);

  // 3. Start WiFi Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(ap_IP, ap_gateway, ap_subnet);
  WiFi.softAP(ap_ssid); // Open network, no password

  Serial.println("WiFi Access Point started!");
  Serial.print("  SSID: ");
  Serial.println(ap_ssid);
  Serial.print("  AP IP: ");
  Serial.println(WiFi.softAPIP());

  // 4. Start DNS Server (resolve tile.robot.control -> 192.168.4.1)
  dnsServer.setTTL(300);
  dnsServer.start(DNS_PORT, "tile.robot.control", ap_IP);
  Serial.println("DNS Server started: tile.robot.control -> 192.168.4.1");

  // 5. Start WebSockets Server on Port 81 (Same Origin)
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
  Serial.println("WebSockets Server running on port 81 (Same Origin)");

  // 6. Start UDP Telemetry Listener on Port 8889
  udp.begin(UDP_TELEM_PORT);
  Serial.println("UDP Telemetry Bridge listening on port 8889");

  // 7. Setup Web Server Endpoints
  server.on("/", HTTP_GET, handleRoot);
  server.on("/clean", HTTP_GET, handleClean);
  server.on("/spray", HTTP_GET, handleSpray);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/cmd", HTTP_GET, handleCmd);
  server.on("/mode", HTTP_GET, handleMode);
  server.onNotFound(handleRoot);

  server.begin();
  Serial.println("HTTP Web Server running on port 80");

  // 8. ArduinoOTA Setup
  ArduinoOTA.setHostname("tile-robot-webhost");
  ArduinoOTA.begin();
  Serial.println("ArduinoOTA ready (tile-robot-webhost)");
  Serial.println(">>> System Ready <<<");
}

// ===========================
// Main Loop
// ===========================
void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  webSocket.loop();
  ArduinoOTA.handle();
  handleRelayLoop();
  handleAutoCleanLoop();

  // Receive UDP telemetry from ESP #1 and broadcast to WebSockets
  int packetSize = udp.parsePacket();
  if (packetSize) {
    char telemBuf[128];
    int len = udp.read(telemBuf, sizeof(telemBuf) - 1);
    if (len > 0) {
      telemBuf[len] = 0;
      webSocket.broadcastTXT((uint8_t *)telemBuf, len);
    }
  }
}
