#include <Arduino.h>
#include "max30102.h"
#include "algorithm_by_RF.h"
#include "wifi_helper.h"
#include "http_helper.h"

static const char* DEVICE_ID = "esp32_001";

static const int SDA_PIN = 5;
static const int SCL_PIN = 6;

static const int RAW_SENSOR_HZ = 100;
static const int ALGO_HZ = FS;
static const int DOWNSAMPLE_FACTOR = RAW_SENSOR_HZ / ALGO_HZ; // 4

static const uint32_t FINGER_ON_THRESHOLD  = 20000;
static const uint32_t FINGER_OFF_THRESHOLD = 10000;

static const unsigned long FINGER_ON_DEBOUNCE_MS  = 200;
static const unsigned long FINGER_OFF_DEBOUNCE_MS = 300;
static const unsigned long FINGER_SETTLE_MS       = 1500;
static const unsigned long STATUS_PRINT_MS        = 1000;

uint32_t irBuffer[BUFFER_SIZE];
uint32_t redBuffer[BUFFER_SIZE];
int bufferIndex = 0;

bool fingerPresent = false;
bool wifiConnected = false;

unsigned long fingerDetectedAt = 0;
unsigned long lastStatusPrint = 0;

unsigned long fingerOnCandidateAt = 0;
unsigned long fingerOffCandidateAt = 0;
bool fingerOnCandidate = false;
bool fingerOffCandidate = false;

uint64_t irAccum = 0;
uint64_t redAccum = 0;
int rawAccumCount = 0;

int32_t heartRate = -999;
int8_t hrValid = 0;
float spo2 = -999.0f;
int8_t spo2Valid = 0;
float ratio = 0.0f;
float correl = 0.0f;

void resetRfState() {
  bufferIndex = 0;
  irAccum = 0;
  redAccum = 0;
  rawAccumCount = 0;

  heartRate = -999;
  hrValid = 0;
  spo2 = -999.0f;
  spo2Valid = 0;
  ratio = 0.0f;
  correl = 0.0f;
}

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

void printCollecting(uint32_t irValue, uint32_t redValue) {
  Serial.print("Collecting samples | IR: ");
  Serial.print(irValue);
  Serial.print(" | RED: ");
  Serial.print(redValue);
  Serial.print(" | Batch: ");
  Serial.print(bufferIndex);
  Serial.print("/");
  Serial.println(BUFFER_SIZE);
}

void printResults() {
  Serial.println("----- Batch complete -----");

  Serial.print("HR valid: ");
  Serial.println(hrValid);
  if (hrValid) {
    Serial.print("Heart Rate (BPM): ");
    Serial.println(heartRate);
  } else {
    Serial.println("Heart Rate: invalid");
  }

  Serial.print("SpO2 valid: ");
  Serial.println(spo2Valid);
  if (spo2Valid) {
    Serial.print("SpO2 (%): ");
    Serial.println(spo2, 1);
  } else {
    Serial.println("SpO2: invalid");
  }

  Serial.print("Signal ratio: ");
  Serial.println(ratio, 3);

  Serial.print("Correlation: ");
  Serial.println(correl, 3);

  Serial.println("--------------------------");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("Starting Sensor-to-Web RF rebuild...");

  if (!maxim_max30102_init(SDA_PIN, SCL_PIN)) {
    Serial.println("ERROR: MAX30102 init failed.");
    while (true) delay(1000);
  }

  Serial.println("MAX30102 initialized.");

  wifiConnected = connectToWiFi();
  if (!wifiConnected) {
    Serial.println("Continuing without Wi-Fi.");
  }

  resetRfState();
  resetFingerDebounce();
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

  if (!fingerPresent) {
    if (irValue >= FINGER_ON_THRESHOLD) {
      if (!fingerOnCandidate) {
        fingerOnCandidate = true;
        fingerOnCandidateAt = now;
      } else if (now - fingerOnCandidateAt >= FINGER_ON_DEBOUNCE_MS) {
        fingerPresent = true;
        fingerDetectedAt = now;
        resetRfState();
        fingerOnCandidate = false;
        Serial.println("Finger detected. Starting buffered acquisition...");
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
        resetRfState();
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

  irAccum += irValue;
  redAccum += redValue;
  rawAccumCount++;

  if (rawAccumCount < DOWNSAMPLE_FACTOR) {
    delay(10);
    return;
  }

  uint32_t irDown = (uint32_t)(irAccum / DOWNSAMPLE_FACTOR);
  uint32_t redDown = (uint32_t)(redAccum / DOWNSAMPLE_FACTOR);

  static uint32_t lastIrDown = 0;
static uint32_t lastRedDown = 0;

int32_t irDelta = (int32_t)irDown - (int32_t)lastIrDown;
int32_t redDelta = (int32_t)redDown - (int32_t)lastRedDown;

lastIrDown = irDown;
lastRedDown = redDown;

if (now - lastStatusPrint >= 250) {
  lastStatusPrint = now;
  Serial.print("IR: ");
  Serial.print(irDown);
  Serial.print(" | dIR: ");
  Serial.print(irDelta);
  Serial.print(" | RED: ");
  Serial.print(redDown);
  Serial.print(" | dRED: ");
  Serial.println(redDelta);
}

  irAccum = 0;
  redAccum = 0;
  rawAccumCount = 0;

  irBuffer[bufferIndex] = irDown;
  redBuffer[bufferIndex] = redDown;
  bufferIndex++;

  if (now - lastStatusPrint >= STATUS_PRINT_MS) {
    lastStatusPrint = now;
    printCollecting(irDown, redDown);
  }

  if (bufferIndex < BUFFER_SIZE) {
    delay(10);
    return;
  }

  rf_heart_rate_and_oxygen_saturation(
    irBuffer,
    BUFFER_SIZE,
    redBuffer,
    &spo2,
    &spo2Valid,
    &heartRate,
    &hrValid,
    &ratio,
    &correl
  );

  printResults();

  if (hrValid || spo2Valid) {
    sendVitalData(
      DEVICE_ID,
      heartRate,
      hrValid != 0,
      spo2,
      spo2Valid != 0,
      irBuffer[BUFFER_SIZE - 1],
      redBuffer[BUFFER_SIZE - 1],
      ratio,
      correl
    );
  } else {
    Serial.println("Skipping HTTP: batch invalid.");
  }

  bufferIndex = 0;
}