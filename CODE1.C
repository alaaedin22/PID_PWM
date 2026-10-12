#include <Arduino.h> // pinMode, digitalWrite, micros... (automatic in .ino, needed elsewhere)

// ===== KNOBS YOU WILL TUNE =====
float Kp = 5;        // start small, see section D (tuning)
float Ki = 0;        // stays 0 until the very end
float Kd = 0;        // add after Kp works
int baseSpeed = 100; // normal speed (0-255). Start LOW!
const int MAX_PWM =
    180; // 6 V motors on a 7.4-8.4 V battery: never go above this

// ===== PWM SETTINGS =====
const int PWM_FREQ = 20000; // 20,000 ON/OFF cycles per second (silent)
const int PWM_BITS = 8;     // 8 bits = numbers 0-255

// ===== PINS =====
const int NUM_SENSORS = 8;
const int sensorPins[NUM_SENSORS] = {32, 33, 25, 26,
                                     27, 14, 13, 23};          // left to right
const int weights[NUM_SENSORS] = {-7, -5, -3, -1, 1, 3, 5, 7}; // the ruler

const int LEFT_PWM = 18, LEFT_IN1 = 16,
          LEFT_IN2 = 17; // TB6612: PWMA, AIN1, AIN2
const int RIGHT_PWM = 19, RIGHT_IN1 = 21,
          RIGHT_IN2 = 22; // TB6612: PWMB, BIN1, BIN2
const int BUTTON_PIN = 5; // pressed = LOW
const int LED_YELLOW = 4, LED_GREEN = 2, LED_RED = 15;
const int HALL_PIN = 34;  // A3144 output (input-only pin, 10k pull-up to 3.3V)

// ===== TIMING AND MEMORY =====
const unsigned long LOOP_MS = 5; // PID runs every 5 ms = 200 times per second
unsigned long lastTime = 0;      // when the PID last ran
float lastError = 0;             // error from the previous loop (for D)
float integral = 0;              // sum of errors over time (for I)

const unsigned int MAX_TIME = 2500;  // longest sensor wait, in microseconds
unsigned int threshold[NUM_SENSORS]; // "line or floor?" limit for each sensor
const unsigned int MIN_RANGE = 200;  // line must be >= 200 us slower than floor

// Measure all 8 sensors. Result = time in microseconds for each one.
// Dark line = LONG time. Light floor = SHORT time.
void readSensors(unsigned int values[]) {
  for (int i = 0; i < NUM_SENSORS; i++) {
    pinMode(sensorPins[i], OUTPUT);    // 1. fill the "bucket": pin as output...
    digitalWrite(sensorPins[i], HIGH); //    ...set to HIGH
    values[i] = MAX_TIME;              // "this sensor has not finished yet"
  }
  delayMicroseconds(10); // wait 10 µs for it to fill

  for (int i = 0; i < NUM_SENSORS; i++)
    pinMode(sensorPins[i], INPUT); // 2. let it empty

  unsigned long start = micros();       // start the stopwatch
  while (micros() - start < MAX_TIME) { // keep measuring up to MAX_TIME
    unsigned long t = micros() - start; // time elapsed so far
    for (int i = 0; i < NUM_SENSORS; i++) {
      if (values[i] == MAX_TIME && digitalRead(sensorPins[i]) == LOW) {
        values[i] = t; // pin just fell to LOW: save the time
      }
    }
  }
}

// For 3 seconds, slide the robot by hand across the line AND the floor.
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
      if (v[i] < minV[i])
        minV[i] = v[i]; // shortest time seen = floor
      if (v[i] > maxV[i])
        maxV[i] = v[i]; // longest time seen = line
    }
  }
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (maxV[i] - minV[i] < MIN_RANGE) {
      // FIX: this sensor never really saw the line during calibration.
      // Its "middle" would sit right on top of the floor value, so random
      // noise would make it flicker between "line" and "floor".
      threshold[i] = MAX_TIME;        // ignore it (a reading can't be > MAX_TIME)
      digitalWrite(LED_YELLOW, HIGH); // warning: calibrate again!
    } else {
      threshold[i] = (minV[i] + maxV[i]) / 2; // the middle
    }
  }
}

// Where is the line? Negative = line is on the LEFT, positive = on the RIGHT,
// 0 = centered. Returns -8 or +8 when the line is lost.
float readError() {
  unsigned int values[NUM_SENSORS];
  readSensors(values);

  int sum = 0;   // adds up the weights of sensors that see the line
  int count = 0; // counts how many sensors see the line
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (values[i] > threshold[i]) { // long time = dark = line
      sum += weights[i];
      count++;
    }
  }

  if (count == 0) { // no sensor sees the line: lost!
    digitalWrite(LED_RED, HIGH);
    return (lastError >= 0) ? 8 : -8; // guess: it went the same way as last time
  }
  digitalWrite(LED_RED, LOW);
  return (float)sum / count; // the average weight = the error
}