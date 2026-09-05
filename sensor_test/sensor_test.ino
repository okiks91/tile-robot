// ============================================================================
// Ultrasonic Sensor Calibration & Test (ESP32-S3 Motor Board)
// Ultra-fast compilation (No WiFi/Web overhead)
// Output: Formatted Serial Monitor @ 115200 Baud
// ============================================================================

// Pin Definitions (matching your Motor Board configuration)
const int FRONT_TRIG = 5;
const int FRONT_ECHO = 15;

const int LEFT_TRIG  = 9;
const int LEFT_ECHO  = 8;

const int RIGHT_TRIG = 4;
const int RIGHT_ECHO = 2;

const int REAR_TRIG  = 16;
const int REAR_ECHO  = 17;

const int trigPins[4] = {FRONT_TRIG, LEFT_TRIG, RIGHT_TRIG, REAR_TRIG};
const int echoPins[4] = {FRONT_ECHO, LEFT_ECHO, RIGHT_ECHO, REAR_ECHO};
const char* sensorNames[4] = {"FRONT", "LEFT ", "RIGHT", "REAR "};

// Read distance with non-blocking 15ms timeout (~250cm max range)
float getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 15000); // 15ms timeout
  if (duration <= 0) return 999.0f; // No echo received / out of range

  float dist = (duration * 0.0343f) / 2.0f;
  if (dist < 1.0f || dist > 400.0f) return 999.0f;
  return dist;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n==================================================");
  Serial.println("  ESP32-S3 Ultrasonic 4-Sensor Diagnostic Tool  ");
  Serial.println("==================================================");
  Serial.println("Reading sensors in continuous loop...\n");

  for (int i = 0; i < 4; i++) {
    pinMode(trigPins[i], OUTPUT);
    pinMode(echoPins[i], INPUT);
    digitalWrite(trigPins[i], LOW);
  }

  Serial.println("--------------------------------------------------");
  Serial.println("  FRONT (cm) |  LEFT (cm)  |  RIGHT (cm) |  REAR (cm)  ");
  Serial.println("--------------------------------------------------");
}

void loop() {
  float front = getDistance(FRONT_TRIG, FRONT_ECHO);
  delay(15); // Small delay to prevent ultrasonic echo crosstalk
  
  float left  = getDistance(LEFT_TRIG, LEFT_ECHO);
  delay(15);
  
  float right = getDistance(RIGHT_TRIG, RIGHT_ECHO);
  delay(15);
  
  float rear  = getDistance(REAR_TRIG, REAR_ECHO);
  delay(15);

  // Print formatted row
  Serial.printf("   %6.1f    |   %6.1f    |   %6.1f    |   %6.1f   \n", 
                front, left, right, rear);

  delay(100); // 100ms display refresh rate (10 Hz)
}
