#include <Wire.h>
#include <WiFi.h>
#include "MAX30105.h"
#include "heartRate.h"
#include "secrets.h"

/*
  ============================================================
  ESP32-S3 + MAX30102 Single-File Heart Rate Monitor + Wi-Fi
  ============================================================

  What this sketch does:
  1. Initializes the MAX30102 over I2C
  2. Connects to one of multiple Wi-Fi networks
  3. Detects when a finger is placed on the sensor
  4. Waits 3 seconds for a short calibration period
  5. Detects heart beats from the IR signal
  6. Prints BPM to the Serial Monitor
  7. Resets cleanly when the finger is removed

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

static const int SDA_PIN = 5;
static const int SCL_PIN = 6;

MAX30105 sensor;


// ============================================================
// Wi-Fi configuration
// ============================================================

struct WiFiNetwork {
  const char* ssid;
  const char* password;
};

WiFiNetwork networks[] = {
  {WIFI_SSID_1, WIFI_PASS_1}, // phone hotspot
  {WIFI_SSID_2, WIFI_PASS_2}, // home Wi-Fi
  {WIFI_SSID_3, WIFI_PASS_3}  // school Wi-Fi
};

const int NUM_NETWORKS = sizeof(networks) / sizeof(networks[0]);
bool wifiConnected = false;


// ============================================================
// User-tunable settings
// ============================================================

static const long FINGER_THRESHOLD = 50000;
static const unsigned long CALIBRATION_TIME_MS = 3000;
static const unsigned long PRINT_INTERVAL_MS = 500;


// ============================================================
// Beat averaging settings
// ============================================================

const byte RATE_SIZE = 8;
byte rates[RATE_SIZE];
byte rateSpot = 0;


// ============================================================
// Runtime state variables
// ============================================================

bool fingerPresent = false;
unsigned long fingerDetectedTime = 0;
unsigned long lastPrintTime = 0;

long lastBeatTime = 0;
float currentBPM = 0.0;
int averageBPM = 0;


// ============================================================
// Helper: Wi-Fi connect
// ============================================================

bool connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(500);

  Serial.print("ESP32 STA MAC: ");
  Serial.println(WiFi.macAddress());

  for (int i = 0; i < NUM_NETWORKS; i++) {
    if (networks[i].ssid == nullptr || strlen(networks[i].ssid) == 0) {
      continue;
    }

    Serial.print("Trying Wi-Fi: ");
    Serial.println(networks[i].ssid);

    WiFi.begin(networks[i].ssid, networks[i].password);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWi-Fi connected.");
      Serial.print("SSID: ");
      Serial.println(WiFi.SSID());
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      return true;
    }

    Serial.println("\nFailed. Moving to next network...");
    WiFi.disconnect(true, true);
    delay(500);
  }

  Serial.println("No Wi-Fi network connected.");
  return false;
}


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

  if (checkForBeat(irValue)) {
    long delta = now - lastBeatTime;
    lastBeatTime = now;

    if (delta <= 0) {
      return;
    }

    currentBPM = 60.0 / (delta / 1000.0);

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

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!sensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("ERROR: MAX30102 not found. Check wiring.");
    while (1) {
      delay(1000);
    }
  }

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

  sensor.setPulseAmplitudeRed(0x1F);
  sensor.setPulseAmplitudeIR(0x1F);
  sensor.setPulseAmplitudeGreen(0);

  resetSession();

  wifiConnected = connectToWiFi();

  if (!wifiConnected) {
    Serial.println("Continuing without Wi-Fi for now.");
  }

  Serial.println("Place finger on sensor...");
}


// ============================================================
// Arduino loop()
// ============================================================

void loop() {
  long irValue = sensor.getIR();
  unsigned long now = millis();

  if (irValue < FINGER_THRESHOLD) {
    if (fingerPresent) {
      Serial.println("Finger removed. Resetting...");
    }

    resetSession();
    printNoFinger(irValue);
    delay(150);
    return;
  }

  if (!fingerPresent) {
    fingerPresent = true;
    fingerDetectedTime = now;
    Serial.println("Finger detected. Hold still for 3 seconds...");
  }

  updateBeatDetection(irValue);

  unsigned long elapsedSinceFingerDetected = now - fingerDetectedTime;

  if (elapsedSinceFingerDetected < CALIBRATION_TIME_MS) {
    printCalibrationCountdown(elapsedSinceFingerDetected);
    delay(20);
    return;
  }

  if (now - lastPrintTime >= PRINT_INTERVAL_MS) {
    lastPrintTime = now;
    printBPMStatus(irValue);
  }

  delay(20);
}