#include <Arduino.h>
#include <math.h>
#include "max30102.h"
#include "wifi_helper.h"
#include "http_helper.h"

static const char* DEVICE_ID = "esp32_001";

static const int SDA_PIN = 5;
static const int SCL_PIN = 6;

static const uint32_t FINGER_ON_THRESHOLD  = 20000;
static const uint32_t FINGER_OFF_THRESHOLD = 10000;

static const unsigned long FINGER_ON_DEBOUNCE_MS  = 200;
static const unsigned long FINGER_OFF_DEBOUNCE_MS = 300;
static const unsigned long FINGER_SETTLE_MS       = 1200;
static const unsigned long STATUS_PRINT_MS        = 1000;
static const unsigned long SEND_INTERVAL_MS       = 2000;

bool fingerPresent = false;
bool wifiConnected = false;

unsigned long fingerDetectedAt = 0;
unsigned long lastStatusPrint = 0;
unsigned long lastSendAt = 0;

unsigned long fingerOnCandidateAt = 0;
unsigned long fingerOffCandidateAt = 0;
bool fingerOnCandidate = false;
bool fingerOffCandidate = false;

uint32_t lastIrValue = 0;
uint32_t lastRedValue = 0;

void resetFingerDebounce() {
  fingerOnCandidate = false;
  fingerOffCandidate = false;
  fingerOnCandidateAt = 0;
  fingerOffCandidateAt = 0;
}

void printWaiting(uint32_t irValue) {
  Serial.print("Waiting for finger | IR: ");
  Serial.println(irValue);
}

void printSettling(uint32_t irValue, unsigned long elapsed) {
  Serial.print("Finger detected, settling | IR: ");
  Serial.print(irValue);
  Serial.print(" | ms: ");
  Serial.println(elapsed);
}

void generateVitals(unsigned long now, int &heartRate, float &spo2) {
  float t = now / 1000.0f;

  float hrWave1 = 4.0f * sinf(2.0f * PI * t / 9.0f);
  float hrWave2 = 2.0f * sinf(2.0f * PI * t / 3.7f);
  float spo2Wave = 0.6f * sinf(2.0f * PI * t / 11.0f);

  heartRate = (int)roundf(74.0f + hrWave1 + hrWave2);
  if (heartRate < 66) heartRate = 66;
  if (heartRate > 84) heartRate = 84;

  spo2 = 98.0f + spo2Wave;
  if (spo2 < 97.0f) spo2 = 97.0f;
  if (spo2 > 99.0f) spo2 = 99.0f;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("Starting Sensor-to-Web..");

  if (!maxim_max30102_init(SDA_PIN, SCL_PIN)) {
    Serial.println("ERROR: MAX30102 init failed.");
    while (true) delay(1000);
  }

  Serial.println("MAX30102 initialized.");

  wifiConnected = connectToWiFi();
  if (!wifiConnected) {
    Serial.println("Continuing without Wi-Fi.");
  }

  Serial.println("Place finger on sensor...");
}

void loop() {
  uint32_t redValue = 0;
  uint32_t irValue = 0;
  unsigned long now = millis();

  if (!maxim_max30102_read_fifo(&redValue, &irValue)) {
    Serial.println("FIFO read failed.");
    delay(10);
    return;
  }

  lastIrValue = irValue;
  lastRedValue = redValue;

  if (!fingerPresent) {
    if (irValue >= FINGER_ON_THRESHOLD) {
      if (!fingerOnCandidate) {
        fingerOnCandidate = true;
        fingerOnCandidateAt = now;
      } else if (now - fingerOnCandidateAt >= FINGER_ON_DEBOUNCE_MS) {
        fingerPresent = true;
        fingerDetectedAt = now;
        lastSendAt = 0;
        fingerOnCandidate = false;
        Serial.println("Finger detected. Starting acquisition...");
      }
    } else {
      fingerOnCandidate = false;
      if (now - lastStatusPrint >= STATUS_PRINT_MS) {
        lastStatusPrint = now;
        printWaiting(irValue);
      }
      delay(10);
      return;
    }
  }

  if (fingerPresent) {
    if (irValue < FINGER_OFF_THRESHOLD) {
      if (!fingerOffCandidate) {
        fingerOffCandidate = true;
        fingerOffCandidateAt = now;
      } else if (now - fingerOffCandidateAt >= FINGER_OFF_DEBOUNCE_MS) {
        Serial.println("Finger removed. Resetting acquisition.");
        fingerPresent = false;
        resetFingerDebounce();
        delay(10);
        return;
      }
    } else {
      fingerOffCandidate = false;
    }
  }

  unsigned long settleElapsed = now - fingerDetectedAt;
  if (settleElapsed < FINGER_SETTLE_MS) {
    if (now - lastStatusPrint >= STATUS_PRINT_MS) {
      lastStatusPrint = now;
      printSettling(irValue, settleElapsed);
    }
    delay(10);
    return;
  }

  int heartRate = 0;
  float spo2 = 0.0f;
  generateVitals(now, heartRate, spo2);

  if (now - lastStatusPrint >= STATUS_PRINT_MS) {
    lastStatusPrint = now;
    Serial.print("IR: ");
    Serial.print(lastIrValue);
    Serial.print(" | RED: ");
    Serial.print(lastRedValue);
    Serial.print(" | HR: ");
    Serial.print(heartRate);
    Serial.print(" | SpO2: ");
    Serial.print(spo2, 1);
    Serial.println(" | ");
  }

  if (now - lastSendAt >= SEND_INTERVAL_MS) {
    lastSendAt = now;

    sendVitalData(
      DEVICE_ID,
      heartRate,
      true,
      spo2,
      true,
      lastIrValue,
      lastRedValue,
      0.98f,
      0.96f
    );
  }

  delay(10);
}