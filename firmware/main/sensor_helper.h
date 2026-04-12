#pragma once
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"

// ============================
// Pins
// ============================

static const int SDA_PIN = 5;
static const int SCL_PIN = 6;

// ============================
// Sensor + state
// ============================

MAX30105 sensor;

static const long FINGER_THRESHOLD = 30000;
static const unsigned long CALIBRATION_TIME_MS = 2000;
static const unsigned long PRINT_INTERVAL_MS = 500;

const byte RATE_SIZE = 8;
byte rates[RATE_SIZE];
byte rateSpot = 0;

bool fingerPresent = false;
unsigned long fingerDetectedTime = 0;
unsigned long lastPrintTime = 0;

long lastBeatTime = 0;
float currentBPM = 0.0;
int averageBPM = 0;

// ============================
// Init
// ============================

void sensorInit() {
  Wire.begin(SDA_PIN, SCL_PIN);

  if (!sensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("ERROR: MAX30102 not found.");
    while (1) delay(1000);
  }

  sensor.setup(60, 4, 2, 100, 411, 4096);

  sensor.setPulseAmplitudeRed(0x1F);
  sensor.setPulseAmplitudeIR(0x1F);
  sensor.setPulseAmplitudeGreen(0);
}

// ============================
// Session reset
// ============================

void resetSession() {
  fingerPresent = false;
  fingerDetectedTime = 0;
  lastPrintTime = 0;
  lastBeatTime = 0;
  currentBPM = 0;
  averageBPM = 0;
  rateSpot = 0;

  for (byte i = 0; i < RATE_SIZE; i++) {
    rates[i] = 0;
  }
}

// ============================
// BPM
// ============================

int computeAverageBPM() {
  int total = 0, count = 0;

  for (byte i = 0; i < RATE_SIZE; i++) {
    if (rates[i] > 0) {
      total += rates[i];
      count++;
    }
  }

  return (count == 0) ? 0 : total / count;
}

void updateBeatDetection(long irValue) {
  unsigned long now = millis();

  if (checkForBeat(irValue)) {
    long delta = now - lastBeatTime;
    lastBeatTime = now;

    if (delta <= 0) return;

    currentBPM = 60.0 / (delta / 1000.0);

    if (currentBPM > 35 && currentBPM < 220) {
      rates[rateSpot] = (byte)currentBPM;
      rateSpot = (rateSpot + 1) % RATE_SIZE;
      averageBPM = computeAverageBPM();
    }
  }
}

// ============================
// Prints
// ============================

void printNoFinger(long irValue) {
  Serial.print("No finger | IR: ");
  Serial.println(irValue);
}

void printCalibrationCountdown(unsigned long elapsedMs) {
  static int last = -1;

  int sec = (CALIBRATION_TIME_MS - elapsedMs + 999) / 1000;

  if (sec != last) {
    last = sec;
    Serial.print("Calibrating... ");
    Serial.print(sec);
    Serial.println("s");
  }

  if (elapsedMs >= CALIBRATION_TIME_MS) {
    last = -1;
  }
}

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