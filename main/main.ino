// ============================================================================
// Tile Robot â€” Real-Time Motor Controller (ESP32-WROOM-32)
// Ultra-Low-Latency WebSockets (Port 81) + Web Server (Port 80)
// ============================================================================

#include <ArduinoOTA.h>
#include <Update.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// ===========================
// WiFi Credentials & UDP Ports
// ===========================
const char *ssid = "robot control";
const char *password = "";

WiFiUDP udp;
const int UDP_DRIVE_PORT = 8888;
const int UDP_TELEM_PORT = 8889;
IPAddress webHostIP(192, 168, 4, 1);

// ===========================
// Ultrasonic Sensor Pins
// ===========================
// FRONT Ultrasonic
const int FRONT_TRIG = 5;
const int FRONT_ECHO = 15;

// LEFT  Ultrasonic
const int LEFT_TRIG = 9;
const int LEFT_ECHO = 8;

// RIGHT Ultrasonic
const int RIGHT_TRIG = 4;
const int RIGHT_ECHO = 2;

// REAR  Ultrasonic
const int REAR_TRIG = 16;
const int REAR_ECHO = 17;

const int trigPins[4] = {FRONT_TRIG, LEFT_TRIG, RIGHT_TRIG, REAR_TRIG};
const int echoPins[4] = {FRONT_ECHO, LEFT_ECHO, RIGHT_ECHO, REAR_ECHO};

// ===========================
// Motor Driver Pins (LEDC PWM)
// ===========================
// Left motor:  IN1 (forward), IN2 (reverse)
// Right motor: IN3 (forward), IN4 (reverse)
const int MOTOR_IN1 = 10; // Left  Motor Forward RPWM
const int MOTOR_IN2 = 11; // Left  Motor Reverse LPWM
const int MOTOR_IN3 = 12; // Right Motor Forward RPWM
const int MOTOR_IN4 = 13; // Right Motor Reverse LPWM




// ===========================
// PWM Configuration
// ===========================
const int PWM_FREQ = 20000;   // 20 kHz
const int PWM_RESOLUTION = 8; // 8-bit (0â€“255)
const int BASE_SPEED = 200;   // Base speed
const int MAX_SPEED = 255;    // Top speed on outer wheel

// ===========================
// Navigation Mode: Strictly starts in MANUAL on boot
// ===========================
enum NavMode { MODE_MANUAL, MODE_AUTO };
volatile NavMode currentMode = MODE_MANUAL;

// ===========================
// Safety Watchdog
// ===========================
unsigned long lastPacketTime = 0;
const unsigned long WATCHDOG_TIMEOUT =
    250; // Cut motors if no command within 250ms

// ===========================
// Sensor Readings (cm)
// ===========================
float frontDist = 999.0;
float leftDist = 999.0;
float rightDist = 999.0;
float rearDist = 999.0;

unsigned long lastSensorReadTime = 0;
const unsigned long SENSOR_INTERVAL =
    20; // Read sensors every 20ms for reliable, safe echo dissipation

unsigned long lastWsBroadcastTime = 0;
const unsigned long WS_BROADCAST_INTERVAL =
    100; // Broadcast telemetry 10 times/sec

// ===========================
// Auto Avoidance State Machine
// ===========================
enum AutoState { AUTO_FORWARD, AUTO_DODGE, AUTO_BACKUP };
AutoState autoState = AUTO_FORWARD;
unsigned long autoStateStartTime = 0;
bool autoTurnLeft = false;

// ===========================
// Servers: HTTP (Port 80) & WebSocket (Port 81)
// ===========================
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// ===========================
// Motor Control Functions
// ===========================
void stopMotors() {
  ledcWrite(MOTOR_IN1, 0);
  ledcWrite(MOTOR_IN2, 0);
  ledcWrite(MOTOR_IN3, 0);
  ledcWrite(MOTOR_IN4, 0);
}

void setMotors(int leftSpeed, int rightSpeed) {
  leftSpeed = constrain(leftSpeed, -255, 255);
  rightSpeed = constrain(rightSpeed, -255, 255);

  if (leftSpeed >= 0) {
    ledcWrite(MOTOR_IN1, leftSpeed);
    ledcWrite(MOTOR_IN2, 0);
  } else {
    ledcWrite(MOTOR_IN1, 0);
    ledcWrite(MOTOR_IN2, -leftSpeed);
  }

  if (rightSpeed >= 0) {
    ledcWrite(MOTOR_IN3, rightSpeed);
    ledcWrite(MOTOR_IN4, 0);
  } else {
    ledcWrite(MOTOR_IN3, 0);
    ledcWrite(MOTOR_IN4, -rightSpeed);
  }
}



// ===========================
// 2-Wheel Differential Steering Ratio
// ===========================
// angle: -100 (Full Left) to +100 (Full Right)
// throttle: -255 (Reverse) to +255 (Forward), 0 = stopped / spin
void setSteerDifferential(int angle, int throttle) {
  float steerFactor = constrain(angle, -100, 100) / 100.0f;

  if (throttle == 0) {
    // Steer in place if wheel is turned without gas
    if (abs(angle) > 5) {
      int spinSpeed = (int)(abs(steerFactor) * BASE_SPEED);
      if (steerFactor > 0) {
        // Turn right in place: Left forward, Right reverse
        setMotors(spinSpeed, -spinSpeed);
      } else {
        // Turn left in place: Left reverse, Right forward
        setMotors(-spinSpeed, spinSpeed);
      }
    } else {
      stopMotors();
    }
    return;
  }

  // Driving forward or backward with differential ratio
  int leftSpeed = 0;
  int rightSpeed = 0;

  if (throttle > 0) {
    // Forward drive
    if (steerFactor > 0) {
      // Turn Right:
      // - Left wheel (outer) speeds up from BASE_SPEED towards MAX_SPEED
      // - Right wheel (inner) slows down to 0 at full lock (steerFactor == 1.0)
      leftSpeed = BASE_SPEED + (int)((MAX_SPEED - BASE_SPEED) * steerFactor);
      rightSpeed = (int)(BASE_SPEED * (1.0f - steerFactor));
    } else if (steerFactor < 0) {
      // Turn Left:
      // - Right wheel (outer) speeds up from BASE_SPEED towards MAX_SPEED
      // - Left wheel (inner) slows down to 0 at full lock (steerFactor == -1.0)
      rightSpeed =
          BASE_SPEED + (int)((MAX_SPEED - BASE_SPEED) * (-steerFactor));
      leftSpeed = (int)(BASE_SPEED * (1.0f + steerFactor));
    } else {
      // Straight forward
      leftSpeed = BASE_SPEED;
      rightSpeed = BASE_SPEED;
    }
  } else {
    // Reverse drive
    int revBase = -BASE_SPEED;
    int revMax = -MAX_SPEED;
    if (steerFactor > 0) {
      leftSpeed = revBase + (int)((revMax - revBase) * steerFactor);
      rightSpeed = (int)(revBase * (1.0f - steerFactor));
    } else if (steerFactor < 0) {
      rightSpeed = revBase + (int)((revMax - revBase) * (-steerFactor));
      leftSpeed = (int)(revBase * (1.0f + steerFactor));
    } else {
      leftSpeed = revBase;
      rightSpeed = revBase;
    }
  }

  setMotors(leftSpeed, rightSpeed);
}

// ===========================
// Fast Ultrasonic Reading (Non-Blocking 10ms Timeout)
// ===========================
float readUltrasonic(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // 10ms timeout (~170cm range): ultra-fast and never starves WebSockets
  long duration = pulseIn(echoPin, HIGH, 10000);
  if (duration <= 0) {
    return 999.0;
  }
  float dist = (duration * 0.0343) / 2.0;
  if (dist <= 0.5)
    return 999.0; // Filter noise
  return dist;
}

uint8_t currentSensorIdx = 0;

void updateSensors() {
  // In Auto mode: prioritize Front sensor on every cycle for instant reaction,
  // rotate side sensors
  if (currentMode == MODE_AUTO) {
    frontDist = readUltrasonic(FRONT_TRIG, FRONT_ECHO);
    if (currentSensorIdx == 0) {
      leftDist = readUltrasonic(LEFT_TRIG, LEFT_ECHO);
      currentSensorIdx = 1;
    } else {
      rightDist = readUltrasonic(RIGHT_TRIG, RIGHT_ECHO);
      currentSensorIdx = 0;
    }
  } else {
    // In Manual mode: read one sensor per interval (Round-Robin) for 0ms lag on
    // drive packets
    switch (currentSensorIdx) {
    case 0:
      frontDist = readUltrasonic(FRONT_TRIG, FRONT_ECHO);
      break;
    case 1:
      leftDist = readUltrasonic(LEFT_TRIG, LEFT_ECHO);
      break;
    case 2:
      rightDist = readUltrasonic(RIGHT_TRIG, RIGHT_ECHO);
      break;
    case 3:
      rearDist = readUltrasonic(REAR_TRIG, REAR_ECHO);
      break;
    }
    currentSensorIdx = (currentSensorIdx + 1) % 4;
  }
}

// ===========================
// WebSocket Protocol (Port 81)
// ===========================
// Commands from browser:
// "D:angle,throttle" -> e.g. "D:45,200" or "D:-100,200" or "D:0,0"
// "M:manual" or "M:auto" -> Set navigation mode
// "X" -> Stop immediately
void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload,
                      size_t length) {
  switch (type) {
  case WStype_DISCONNECTED:
    Serial.printf("[WS] Client #%u Disconnected\n", num);
    if (currentMode == MODE_MANUAL)
      stopMotors();
    break;

  case WStype_CONNECTED:
    Serial.printf("[WS] Client #%u Connected\n", num);
    break;

  case WStype_TEXT: {
    String msg = String((char *)payload);
    lastPacketTime = millis();

    if (msg.startsWith("D:")) {
      // Drive Packet: D:angle,throttle
      if (currentMode == MODE_MANUAL) {
        int commaIdx = msg.indexOf(',');
        if (commaIdx > 2) {
          int angle = msg.substring(2, commaIdx).toInt();
          int throttle = msg.substring(commaIdx + 1).toInt();
          setSteerDifferential(angle, throttle);
        }
      }
    } else if (msg == "X") {
      if (currentMode == MODE_MANUAL)
        stopMotors();
    } else if (msg == "M:auto") {
      currentMode = MODE_AUTO;
      autoState = AUTO_FORWARD;
      Serial.println("[WS MODE] -> AUTO");
    } else if (msg == "M:manual") {
      currentMode = MODE_MANUAL;
      stopMotors();
      Serial.println("[WS MODE] -> MANUAL");
    }
    break;
  }
  default:
    break;
  }
}

// ===========================
// HTTP Endpoints (Fallback + Compatibility)
// ===========================
void handleSensors() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  String json = "{";
  json += "\"front\":" + String(frontDist, 1) + ",";
  json += "\"left\":" + String(leftDist, 1) + ",";
  json += "\"right\":" + String(rightDist, 1) + ",";
  json += "\"rear\":" + String(rearDist, 1) + ",";
  json += "\"mode\":\"" + String(currentMode == MODE_AUTO ? "auto" : "manual") +
          "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleMode() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (server.hasArg("set")) {
    String m = server.arg("set");
    if (m == "auto") {
      currentMode = MODE_AUTO;
      autoState = AUTO_FORWARD;
    } else if (m == "manual") {
      currentMode = MODE_MANUAL;
      stopMotors();
    }
  }
  String json = "{\"mode\":\"" +
                String(currentMode == MODE_AUTO ? "auto" : "manual") + "\"}";
  server.send(200, "application/json", json);
}

void handleCmd() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  lastPacketTime = millis();
  String action = server.arg("action");
  if (action == "stop") {
    if (currentMode == MODE_MANUAL) stopMotors();
  } else if (action == "steer") {
    if (currentMode == MODE_MANUAL) {
      int angle = server.hasArg("angle") ? server.arg("angle").toInt() : 0;
      int throttle =
          server.hasArg("throttle") ? server.arg("throttle").toInt() : 0;
      setSteerDifferential(angle, throttle);
    }
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

// ===========================
// Robust Autonomous Obstacle Avoidance (30cm Range, 1.5s Turn)
// ===========================
const float OBSTACLE_THRESHOLD =
    30.0f; // Trigger dodge when front obstacle <= 30cm
const float SIDE_CLEAR_THRESHOLD = 18.0f; // Self-centering side wall threshold
const unsigned long DODGE_TURN_TIME = 1500; // Turn duration: 1.5 seconds

void runAutoNavigation() {
  unsigned long now = millis();

  switch (autoState) {
  case AUTO_FORWARD:
    // Check if front obstacle is detected within 25cm
    if (frontDist <= OBSTACLE_THRESHOLD) {
      // Obstacle ahead! Choose side with greater distance / no obstacle
      autoTurnLeft = (leftDist > rightDist);
      autoState = AUTO_DODGE;
      autoStateStartTime = now;
      if (autoTurnLeft) {
        setMotors(-BASE_SPEED, BASE_SPEED); // Spin Left
      } else {
        setMotors(BASE_SPEED, -BASE_SPEED); // Spin Right
      }
    }
    // Front is clear: check side clearance for smart self-centering
    else if (leftDist <= SIDE_CLEAR_THRESHOLD) {
      setMotors(BASE_SPEED, BASE_SPEED - 60); // Steer slightly right
    } else if (rightDist <= SIDE_CLEAR_THRESHOLD) {
      setMotors(BASE_SPEED - 60, BASE_SPEED); // Steer slightly left
    } else {
      setMotors(BASE_SPEED, BASE_SPEED); // Full forward drive
    }
    break;

  case AUTO_DODGE:
    // Actively spin in the chosen dodge direction
    if (autoTurnLeft) {
      setMotors(-BASE_SPEED, BASE_SPEED);
    } else {
      setMotors(BASE_SPEED, -BASE_SPEED);
    }

    // Keep turning for the full 1.5 seconds (1500ms)
    if (now - autoStateStartTime >= DODGE_TURN_TIME) {
      // If front is now clear, return to forward drive
      if (frontDist > OBSTACLE_THRESHOLD) {
        autoState = AUTO_FORWARD;
      }
      // If still blocked after 1.5s (corner dead-end), back up
      else {
        autoState = AUTO_BACKUP;
        autoStateStartTime = now;
        setMotors(-BASE_SPEED, -BASE_SPEED);
      }
    }
    break;

  case AUTO_BACKUP:
    // Reverse straight back for 500ms to open up clearance
    setMotors(-BASE_SPEED, -BASE_SPEED);
    if (now - autoStateStartTime >= 500) {
      autoState = AUTO_DODGE;
      autoStateStartTime = now;
      autoTurnLeft = !autoTurnLeft; // Try the other direction
    }
    break;
  }
}

// ===========================
// Setup
// ===========================
// ===========================
// Web OTA Update HTML Page
// ===========================
static const char PROGMEM OTA_INDEX_HTML[] = R"rawhtml(
<!DOCTYPE html>
<html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Motor Board Firmware Update</title>
<style>
body{font-family:-apple-system,sans-serif;background:#0f172a;color:#fff;display:flex;align-items:center;justify-content:center;height:100vh;margin:0;}
.card{background:#1e293b;padding:30px;border-radius:16px;border:1px solid #334155;max-width:400px;width:90%;text-align:center;}
h2{margin-bottom:20px;font-size:20px;}
input[type=file]{margin:20px 0;width:100%;color:#94a3b8;}
button{background:#3b82f6;color:#fff;border:none;padding:12px 24px;border-radius:8px;font-weight:bold;cursor:pointer;width:100%;}
button:hover{background:#2563eb;}
#prg{margin-top:15px;font-weight:bold;color:#38bdf8;}
</style></head><body>
<div class="card">
  <h2>âš¡ Motor Board OTA Update</h2>
  <form method='POST' action='/update' enctype='multipart/form-data' id='upload_form'>
    <input type='file' name='update' id='file' accept='.bin' required>
    <button type='submit' id='btn'>Upload Firmware (.bin)</button>
  </form>
  <div id='prg'></div>
</div>
<script>
document.getElementById('upload_form').onsubmit = function(e) {
  e.preventDefault();
  var form = document.getElementById('upload_form');
  var data = new FormData(form);
  var req = new XMLHttpRequest();
  var prg = document.getElementById('prg');
  var btn = document.getElementById('btn');
  btn.disabled = true;
  prg.innerText = 'Uploading 0%...';
  req.upload.addEventListener('progress', function(p) {
    if (p.lengthComputable) {
      var pct = Math.round((p.loaded / p.total) * 100);
      prg.innerText = 'Uploading ' + pct + '%...';
    }
  });
  req.onload = function() {
    if (req.status === 200) {
      prg.innerText = 'Update Success! Rebooting...';
      setTimeout(function(){ window.location.href = '/'; }, 5000);
    } else {
      prg.innerText = 'Error: ' + req.responseText;
      btn.disabled = false;
    }
  };
  req.open('POST', '/update');
  req.send(data);
};
</script></body></html>
)rawhtml";

// ===========================
// Setup
// ===========================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n>>> Tile Robot Motor Controller (ESP32-S3) Starting <<<");

  // Initialize Ultrasonic Pins
  for (int i = 0; i < 4; i++) {
    pinMode(trigPins[i], OUTPUT);
    pinMode(echoPins[i], INPUT);
    digitalWrite(trigPins[i], LOW);
  }

  // Setup LEDC PWM (ESP32 Arduino 3.x API)
  ledcAttach(MOTOR_IN1, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(MOTOR_IN2, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(MOTOR_IN3, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(MOTOR_IN4, PWM_FREQ, PWM_RESOLUTION);
  // Connect to WiFi with Static IP 192.168.4.2 (with DHCP fallback)
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  IPAddress local_IP(192, 168, 4, 2);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  IPAddress primaryDNS(192, 168, 4, 1);
  WiFi.config(local_IP, gateway, subnet, primaryDNS);
  WiFi.begin(ssid);

  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 25) {
    delay(400);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected with Static IP!");
    Serial.print("Motor Board IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nStatic IP timed out. Retrying with DHCP...");
    WiFi.disconnect();
    delay(500);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.begin(ssid);
    attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 25) {
      delay(400);
      Serial.print(".");
      attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi connected via DHCP!");
      Serial.print("Motor Board IP: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("\nWiFi connection failed! Proceeding in offline mode...");
    }
  }

  // Setup HTTP server (Port 80)
  server.on("/sensors", HTTP_GET, handleSensors);
  server.on("/mode", HTTP_GET, handleMode);
  server.on("/cmd", HTTP_GET, handleCmd);

  // Web OTA Endpoints
  server.on("/update", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", OTA_INDEX_HTML);
  });
  server.on(
      "/update", HTTP_POST,
      []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        ESP.restart();
      },
      []() {
        HTTPUpload &upload = server.upload();
        if (upload.status == UPLOAD_FILE_START) {
          stopMotors();
          Serial.printf("OTA Update: %s\n", upload.filename.c_str());
          if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
          }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
          if (Update.write(upload.buf, upload.currentSize) !=
              upload.currentSize) {
            Update.printError(Serial);
          }
        } else if (upload.status == UPLOAD_FILE_END) {
          if (Update.end(true)) {
            Serial.printf("OTA Update Success: %uB\nRebooting...\n",
                          upload.totalSize);
          } else {
            Update.printError(Serial);
          }
        }
      });

  server.begin();

  // Setup ArduinoOTA
  ArduinoOTA.setHostname("tile-robot-motor");
  ArduinoOTA.onStart([]() {
    stopMotors();
    Serial.println("Start ArduinoOTA update");
  });
  ArduinoOTA.begin();

  // Setup WebSocket server (Port 81)
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);

  // Setup UDP Drive Bridge (Port 8888)
  udp.begin(UDP_DRIVE_PORT);

  Serial.println("Real-Time WebSockets Server running on port 81");
  Serial.println("UDP Drive Bridge listening on port 8888");
  Serial.println("OTA Update ready at http://192.168.4.2/update or via ArduinoOTA (tile-robot-motor)");
  Serial.println("Initial Mode: MANUAL (awaiting web commands)");
  lastPacketTime = millis();
}

// ===========================
// Main Loop
// ===========================
void loop() {
  // Always handle OTA, WebSocket & HTTP clients
  ArduinoOTA.handle();
  webSocket.loop();
  server.handleClient();
  unsigned long now = millis();

  // UDP Drive Packet Receiver from ESP #3 (<1ms)
  int packetSize = udp.parsePacket();
  if (packetSize) {
    char buf[64];
    int len = udp.read(buf, sizeof(buf) - 1);
    if (len > 0) {
      buf[len] = 0;
      lastPacketTime = now;
      String msg = String(buf);
      if (msg.startsWith("D:")) {
        if (currentMode == MODE_MANUAL) {
          int commaIdx = msg.indexOf(',');
          if (commaIdx > 1) {
            int angle = msg.substring(2, commaIdx).toInt();
            int throttle = msg.substring(commaIdx + 1).toInt();
            setSteerDifferential(angle, throttle);
          }
        }
      } else if (msg == "X") {
        if (currentMode == MODE_MANUAL) stopMotors();
      } else if (msg == "M:auto") {
        currentMode = MODE_AUTO;
        autoState = AUTO_FORWARD;
      } else if (msg == "M:manual") {
        currentMode = MODE_MANUAL;
        stopMotors();
      }
    }
  }

  // Sensor reading interval
  if (now - lastSensorReadTime >= SENSOR_INTERVAL) {
    lastSensorReadTime = now;
    updateSensors();
  }

  // Telemetry broadcast (10 times per second) over WebSocket and UDP to ESP #3
  if (now - lastWsBroadcastTime >= WS_BROADCAST_INTERVAL) {
    lastWsBroadcastTime = now;
    String telem = "{\"f\":" + String(frontDist, 1) +
                   ",\"l\":" + String(leftDist, 1) +
                   ",\"r\":" + String(rightDist, 1) +
                   ",\"b\":" + String(rearDist, 1) + ",\"m\":\"" +
                   String(currentMode == MODE_AUTO ? "auto" : "manual") + "\"}";
    webSocket.broadcastTXT(telem);
    udp.beginPacket(webHostIP, UDP_TELEM_PORT);
    udp.write((const uint8_t *)telem.c_str(), telem.length());
    udp.endPacket();
  }

  // Navigation Logic
  if (currentMode == MODE_AUTO) {
    runAutoNavigation();
  } else {
    // Safety Watchdog: If in manual mode and no drive packets received in 250ms, stop motors!
    if (now - lastPacketTime > WATCHDOG_TIMEOUT) {
      stopMotors();
    }
  }
}
