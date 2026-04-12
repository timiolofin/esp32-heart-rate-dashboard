#include <Wire.h>
#include <WiFi.h>
#include "MAX30105.h"
#include "heartRate.h"
#include "secrets.h"
#include "wifi_helper.h"
#include "sensor_helper.h"
#include "http_helper.h"

// ============================
// Config
// ============================

const char* DEVICE_ID = "esp32_001";

bool wifiConnected = false;
static unsigned long lastPost = 0;

// ============================
// Setup
// ============================

void setup() {
  Serial.begin(115200);
  delay(1000);

  sensorInit();
  resetSession();

  wifiConnected = connectToWiFi();

  if (!wifiConnected) {
    Serial.println("Continuing without Wi-Fi for now.");
  }

  Serial.println("Place finger on sensor...");
}

// ============================
// Loop
// ============================

void loop() {
  unsigned long now = millis();
  long irValue = sensor.getIR();

  if (now - lastPost >= 5000) {
    lastPost = now;

    int bpmToSend = averageBPM;
    bool fingerDetected = (irValue >= FINGER_THRESHOLD);

    Serial.println("Sending reading...");
    sendHeartRateData(DEVICE_ID, bpmToSend, fingerDetected, irValue);
  }

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