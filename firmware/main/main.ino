#include <Wire.h>
#include <WiFi.h>
#include "MAX30105.h"
#include "heartRate.h"
#include "secrets.h"
#include "wifi_helper.h"
#include "sensor_helper.h"

// ============================
// Config
// ============================

const char* POST_URL = "http://192.168.0.17:8000/data";
const char* DEVICE_ID = "esp32_001";
const char* SESSION_ID = "session_001";

bool wifiConnected = false;

// ============================
// Setup
// ============================

void setup() {
  Serial.begin(115200);
  delay(1000);

  sensorInit();          // from sensor_helper.h
  resetSession();        // from sensor_helper.h

  wifiConnected = connectToWiFi();  // from wifi_helper.h

  if (!wifiConnected) {
    Serial.println("Continuing without Wi-Fi for now.");
  }

  Serial.println("Place finger on sensor...");
}

// ============================
// Loop
// ============================

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

  unsigned long elapsed = now - fingerDetectedTime;

  if (elapsed < CALIBRATION_TIME_MS) {
    printCalibrationCountdown(elapsed);
    delay(20);
    return;
  }

  if (now - lastPrintTime >= PRINT_INTERVAL_MS) {
    lastPrintTime = now;
    printBPMStatus(irValue);
  }

  delay(20);
}