#include <Arduino.h>

// ===== KNOBS YOU WILL TUNE =====
float Kp = 4.5;       // start small
float Ki = 0.0;       // stays 0 until tuning steady-state offset
float Kd = 1.2;       // dampens oscillation on straights
int baseSpeed = 110;  // cruise speed on straights (0-255)
int minSpeed  = 65;   // cornering speed on sharp turns
const int MAX_PWM = 180; // 6V motor limit on 7.4-8.4V battery

// ===== PWM SETTINGS =====
const int PWM_FREQ = 20000; // 20 kHz (silent, high efficiency)
const int PWM_BITS = 8;     // 8-bit resolution (0-255)

// ===== PINS =====
const int NUM_SENSORS = 8;
const int sensorPins[NUM_SENSORS] = {32, 33, 25, 26, 27, 14, 13, 23}; // left to right

// Non-linear weights: gentle in center, aggressive on outer edges
const int weights[NUM_SENSORS] = {-10, -5, -2, -1, 1, 2, 5, 10};

const int LEFT_PWM = 18, LEFT_IN1 = 16, LEFT_IN2 = 17;   // TB6612: PWMA, AIN1, AIN2
const int RIGHT_PWM = 19, RIGHT_IN1 = 21, RIGHT_IN2 = 22; // TB6612: PWMB, BIN1, BIN2
const int BUTTON_PIN = 5;                                // pressed = LOW
const int LED_YELLOW = 4, LED_GREEN = 2, LED_RED = 15;
const int HALL_PIN = 34;                                 // A3144 Hall sensor (finish line)

// ===== TIMING AND MEMORY =====
const unsigned long LOOP_MS = 5; // PID runs every 5 ms = 200 Hz
unsigned long lastTime = 0;
float lastError = 0;
float integral = 0;

const unsigned int MAX_TIME = 2500;  // timeout in microseconds
unsigned int threshold[NUM_SENSORS];
const unsigned int MIN_RANGE = 200;

// Measure all 8 sensors
void readSensors(unsigned int values[]) {
  for (int i = 0; i < NUM_SENSORS; i++) {
    pinMode(sensorPins[i], OUTPUT);
    digitalWrite(sensorPins[i], HIGH);
    values[i] = MAX_TIME;
  }
  delayMicroseconds(10);

  for (int i = 0; i < NUM_SENSORS; i++)
    pinMode(sensorPins[i], INPUT);

  unsigned long start = micros();
  while (micros() - start < MAX_TIME) {
    unsigned long t = micros() - start;
    for (int i = 0; i < NUM_SENSORS; i++) {
      if (values[i] == MAX_TIME && digitalRead(sensorPins[i]) == LOW) {
        values[i] = t;
      }
    }
  }
}

// 3-second calibration routine
void calibrate() {
  unsigned int minV[NUM_SENSORS], maxV[NUM_SENSORS], v[NUM_SENSORS];
  for (int i = 0; i < NUM_SENSORS; i++) {
    minV[i] = MAX_TIME;
    maxV[i] = 0;
  }

  unsigned long start = millis();
  while (millis() - start < 3000) {
    readSensors(v);
    for (int i = 0; i < NUM_SENSORS; i++) {
      if (v[i] < minV[i]) minV[i] = v[i];
      if (v[i] > maxV[i]) maxV[i] = v[i];
    }
  }

  for (int i = 0; i < NUM_SENSORS; i++) {
    if (maxV[i] - minV[i] < MIN_RANGE) {
      threshold[i] = MAX_TIME;
      digitalWrite(LED_YELLOW, HIGH);
    } else {
      threshold[i] = (minV[i] + maxV[i]) / 2;
    }
  }
}

// Line position calculation
float readError() {
  unsigned int values[NUM_SENSORS];
  readSensors(values);

  int sum = 0;
  int count = 0;
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (values[i] > threshold[i]) {
      sum += weights[i];
      count++;
    }
  }

  if (count == 0) {
    digitalWrite(LED_RED, HIGH);
    return (lastError >= 0) ? 10 : -10; // line lost: turn hard toward last side
  }

  digitalWrite(LED_RED, LOW);
  return (float)sum / count;
}

// Bi-directional motor drive: supports forward, active braking, and reverse
void setMotors(int left, int right) {
  // Left Motor Direction
  if (left >= 0) {
    digitalWrite(LEFT_IN1, HIGH);
    digitalWrite(LEFT_IN2, LOW);
    ledcWrite(LEFT_PWM, constrain(left, 0, MAX_PWM));
  } else {
    digitalWrite(LEFT_IN1, LOW);
    digitalWrite(LEFT_IN2, HIGH); // Reverse
    ledcWrite(LEFT_PWM, constrain(-left, 0, MAX_PWM));
  }

  // Right Motor Direction
  if (right >= 0) {
    digitalWrite(RIGHT_IN1, HIGH);
    digitalWrite(RIGHT_IN2, LOW);
    ledcWrite(RIGHT_PWM, constrain(right, 0, MAX_PWM));
  } else {
    digitalWrite(RIGHT_IN1, LOW);
    digitalWrite(RIGHT_IN2, HIGH); // Reverse
    ledcWrite(RIGHT_PWM, constrain(-right, 0, MAX_PWM));
  }
}

void waitForButton() {
  while (digitalRead(BUTTON_PIN) == HIGH) delay(10);
  delay(300);
}

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(HALL_PIN, INPUT);
  pinMode(LED_YELLOW, OUTPUT); 
  pinMode(LED_GREEN, OUTPUT); 
  pinMode(LED_RED, OUTPUT);

  pinMode(LEFT_IN1, OUTPUT);  pinMode(LEFT_IN2, OUTPUT);
  pinMode(RIGHT_IN1, OUTPUT); pinMode(RIGHT_IN2, OUTPUT);

  ledcAttach(LEFT_PWM,  PWM_FREQ, PWM_BITS);
  ledcAttach(RIGHT_PWM, PWM_FREQ, PWM_BITS);
  setMotors(0, 0);

  // Calibration phase
  digitalWrite(LED_YELLOW, HIGH);
  waitForButton();
  delay(1000);
  calibrate();
  digitalWrite(LED_YELLOW, LOW);

  // Ready to race
  digitalWrite(LED_GREEN, HIGH);
  waitForButton();
  delay(500);
}

void loop() {
  // Check for Finish Line Magnet (Hall Sensor)
  if (digitalRead(HALL_PIN) == LOW) {
    setMotors(0, 0);               // Emergency stop
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_RED, HIGH);
    while (true) delay(100);       // Halt execution
  }

  if (millis() - lastTime < LOOP_MS) return;
  lastTime = millis();

  float error = readError();

  // PID calculations
  integral += error;
  integral = constrain(integral, -40, 40);
  if (error == 0) integral = 0;
  float derivative = error - lastError;

  float correction = (Kp * error) + (Ki * integral) + (Kd * derivative);
  lastError = error;

  // Adaptive speed: slow down when navigating curves
  float absError = abs(error);
  int currentBase = baseSpeed;
  if (absError > 2.0) {
    // Progressively scale speed down from baseSpeed to minSpeed
    currentBase = map((long)(absError * 10), 20, 100, baseSpeed, minSpeed);
    currentBase = constrain(currentBase, minSpeed, baseSpeed);
  }

  int leftSpeed  = (int)(currentBase + correction);
  int rightSpeed = (int)(currentBase - correction);

  setMotors(leftSpeed, rightSpeed);
}
