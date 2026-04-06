#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"

/*
  ============================================================
  ESP32-S3 + MAX30102 Single-File Heart Rate Monitor
  ============================================================

  What this sketch does:
  1. Initializes the MAX30102 over I2C
  2. Detects when a finger is placed on the sensor
  3. Waits 3 seconds for a short calibration period
  4. Detects heart beats from the IR signal
  5. Prints BPM to the Serial Monitor
  6. Resets cleanly when the finger is removed

  Required Arduino libraries:
  - SparkFun MAX3010x Pulse and Proximity Sensor Library
  - heartRate.h (included with the SparkFun MAX3010x library)

  Your current wiring:
  - MAX30102 VIN  -> ESP32-S3 3.3V
  - MAX30102 GND  -> ESP32-S3 GND
  - MAX30102 SDA  -> ESP32-S3 GPIO 5
  - MAX30102 SCL  -> ESP32-S3 GPIO 6

  Serial Monitor:
  - Baud rate: 115200
*/


// ============================================================
// Hardware configuration
// ============================================================

// I2C pins for your ESP32-S3 setup
static const int SDA_PIN = 5;
static const int SCL_PIN = 6;

// Create MAX30102 sensor object
MAX30105 sensor;


// ============================================================
// User-tunable settings
// ============================================================

// IR threshold used to decide whether a finger is present.
// If finger detection is too sensitive, increase this value.
// If it misses your finger, decrease this value.
static const long FINGER_THRESHOLD = 50000;

// Calibration time after finger is detected.
// User asked for 3 seconds.
static const unsigned long CALIBRATION_TIME_MS = 3000;

// How often to print BPM after calibration is complete.
static const unsigned long PRINT_INTERVAL_MS = 500;


// ============================================================
// Beat averaging settings
// ============================================================

// Number of most recent beat values used for rolling average BPM.
// Higher = smoother but slower response.
const byte RATE_SIZE = 8;
byte rates[RATE_SIZE];
byte rateSpot = 0;


// ============================================================
// Runtime state variables
// ============================================================

// Finger/session state
bool fingerPresent = false;
unsigned long fingerDetectedTime = 0;
unsigned long lastPrintTime = 0;

// Beat detection state
long lastBeatTime = 0;
float currentBPM = 0.0;
int averageBPM = 0;


// ============================================================
// Helper: Reset all session-specific state
// ============================================================

void resetSession() {
  fingerPresent = false;
  fingerDetectedTime = 0;
  lastPrintTime = 0;
  lastBeatTime = 0;
  currentBPM = 0.0;
  averageBPM = 0;
  rateSpot = 0;

  for (byte i = 0; i < RATE_SIZE; i++) {
    rates[i] = 0;
  }
}


// ============================================================
// Helper: Compute average BPM from valid entries in rates[]
// ============================================================

int computeAverageBPM() {
  int total = 0;
  int count = 0;

  for (byte i = 0; i < RATE_SIZE; i++) {
    if (rates[i] > 0) {
      total += rates[i];
      count++;
    }
  }

  if (count == 0) {
    return 0;
  }

  return total / count;
}


// ============================================================
// Helper: Handle beat detection from current IR value
// ============================================================

void updateBeatDetection(long irValue) {
  unsigned long now = millis();

  // checkForBeat() returns true when a heartbeat is detected
  if (checkForBeat(irValue)) {
    long delta = now - lastBeatTime;
    lastBeatTime = now;

    // Protect against divide-by-zero or nonsense timing
    if (delta <= 0) {
      return;
    }

    // Convert time between beats into BPM
    currentBPM = 60.0 / (delta / 1000.0);

    // Accept only realistic human BPM values
    if (currentBPM > 35 && currentBPM < 220) {
      rates[rateSpot] = (byte)currentBPM;
      rateSpot = (rateSpot + 1) % RATE_SIZE;

      averageBPM = computeAverageBPM();
    }
  }
}


// ============================================================
// Helper: Print no-finger status
// ============================================================

void printNoFinger(long irValue) {
  Serial.print("No finger | IR: ");
  Serial.println(irValue);
}


// ============================================================
// Helper: Print calibration countdown
// ============================================================

void printCalibrationCountdown(unsigned long elapsedMs) {
  static int lastSecondShown = -1;

  int secondsLeft = (CALIBRATION_TIME_MS - elapsedMs + 999) / 1000;

  if (secondsLeft != lastSecondShown) {
    lastSecondShown = secondsLeft;
    Serial.print("Calibrating... ");
    Serial.print(secondsLeft);
    Serial.println("s");
  }

  // Reset countdown display once calibration is complete
  if (elapsedMs >= CALIBRATION_TIME_MS) {
    lastSecondShown = -1;
  }
}


// ============================================================
// Helper: Print finger/BPM status after calibration
// ============================================================

void printBPMStatus(long irValue) {
  Serial.print("Finger detected | IR: ");
  Serial.print(irValue);

  if (averageBPM > 0) {
    Serial.print(" | BPM: ");
    Serial.println(averageBPM);
  } else {
    Serial.println(" | BPM: Calculating...");
  }
}


// ============================================================
// Arduino setup()
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Start I2C on your chosen ESP32-S3 pins
  Wire.begin(SDA_PIN, SCL_PIN);

  // Initialize MAX30102
  if (!sensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("ERROR: MAX30102 not found. Check wiring.");
    while (1) {
      delay(1000);
    }
  }

  /*
    Sensor configuration notes:
    - ledBrightness: LED current / brightness
    - sampleAverage: internal averaging
    - ledMode: 2 = Red + IR
    - sampleRate: sampling frequency in Hz
    - pulseWidth: pulse width
    - adcRange: ADC full-scale range
  */
  byte ledBrightness = 60;
  byte sampleAverage = 4;
  byte ledMode = 2;
  int sampleRate = 100;
  int pulseWidth = 411;
  int adcRange = 4096;

  sensor.setup(
    ledBrightness,
    sampleAverage,
    ledMode,
    sampleRate,
    pulseWidth,
    adcRange
  );

  // Turn on Red and IR LEDs, keep Green off
  sensor.setPulseAmplitudeRed(0x1F);
  sensor.setPulseAmplitudeIR(0x1F);
  sensor.setPulseAmplitudeGreen(0);

  resetSession();

  Serial.println("Place finger on sensor...");
}


// ============================================================
// Arduino loop()
// ============================================================

void loop() {
  long irValue = sensor.getIR();
  unsigned long now = millis();

  // ----------------------------------------------------------
  // 1. Detect whether a finger is present
  // ----------------------------------------------------------
  if (irValue < FINGER_THRESHOLD) {
    if (fingerPresent) {
      Serial.println("Finger removed. Resetting...");
    }

    resetSession();
    printNoFinger(irValue);
    delay(150);
    return;
  }

  // ----------------------------------------------------------
  // 2. Start a new session when finger first appears
  // ----------------------------------------------------------
  if (!fingerPresent) {
    fingerPresent = true;
    fingerDetectedTime = now;

    Serial.println("Finger detected. Hold still for 3 seconds...");
  }

  // ----------------------------------------------------------
  // 3. Always keep updating beat detection while finger is on
  // ----------------------------------------------------------
  updateBeatDetection(irValue);

  // ----------------------------------------------------------
  // 4. Wait through the short calibration window
  // ----------------------------------------------------------
  unsigned long elapsedSinceFingerDetected = now - fingerDetectedTime;

  if (elapsedSinceFingerDetected < CALIBRATION_TIME_MS) {
    printCalibrationCountdown(elapsedSinceFingerDetected);
    delay(20);
    return;
  }

  // ----------------------------------------------------------
  // 5. After calibration, print BPM at fixed intervals
  // ----------------------------------------------------------
  if (now - lastPrintTime >= PRINT_INTERVAL_MS) {
    lastPrintTime = now;
    printBPMStatus(irValue);
  }

  delay(20);
}