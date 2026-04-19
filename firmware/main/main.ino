#include <Arduino.h>
#include "algorithm_by_RF.h"
#include "algorithm.h"
#include "max30102.h"
#include "median_filter.h"
#include "wifi_helper.h"
#include "http_helper.h"

// ── Device identity ───────────────────────────────────────────────────────────
static const char* DEVICE_ID = "esp32_001";

// ── Hardware ──────────────────────────────────────────────────────────────────
const byte oxiInt             = 7;
const int  SENSOR_I2C_SDA_PIN = 5;
const int  SENSOR_I2C_SCL_PIN = 6;

// ── Finger detection thresholds ──────────────────────────────────────────────
static const uint32_t FINGER_ON_THRESHOLD  = 20000;
static const uint32_t FINGER_OFF_THRESHOLD = 10000;

// ── Buffers ───────────────────────────────────────────────────────────────────
uint32_t aun_ir_buffer[BUFFER_SIZE];
uint32_t aun_red_buffer[BUFFER_SIZE];
uint32_t ordered_ir_buffer[BUFFER_SIZE];
uint32_t ordered_red_buffer[BUFFER_SIZE];

// ── Update cadence & smoothing ────────────────────────────────────────────────
const int32_t UPDATE_STEP    = 10;
const uint8_t MEDIAN_HISTORY = 4;

const int32_t INVALID_HR   = -999;
const float   INVALID_SPO2 = -999.0f;

uint16_t sample_write_index = 0;
uint16_t sample_count       = 0;

MedianFilter<int32_t, MEDIAN_HISTORY> hr_filter(INVALID_HR);
MedianFilter<float,   MEDIAN_HISTORY> spo2_filter(INVALID_SPO2);

// ── Timing & output state ─────────────────────────────────────────────────────
uint32_t timeStart                      = 0;
const unsigned long READING_INTERVAL_MS = 5000UL;
unsigned long fingerPlacedAt            = 0;
unsigned long lastOutputAt              = 0;
bool firstReadingDone                   = false;

// ── Finger state ──────────────────────────────────────────────────────────────
bool fingerPresent = false;

// ── Last known good & displayed values ───────────────────────────────────────
int32_t last_good_hr   = INVALID_HR;
float   last_good_spo2 = INVALID_SPO2;

// ── Session ID — regenerates after 60s of no finger ─────────────────────────
String sessionId = "";
unsigned long fingerRemovedAt = 0;  // timestamp of last finger removal
const unsigned long SESSION_RESET_MS = 60000UL;  // 60 seconds

// ── WiFi state ────────────────────────────────────────────────────────────────
bool wifiConnected = false;

// ── Helpers ───────────────────────────────────────────────────────────────────
static void waitForDataReady(uint32_t timeout_us) {
  uint32_t t = micros();
  while (digitalRead(oxiInt) == HIGH) {
    if ((micros() - t) > timeout_us) {
      break;
    }
    delayMicroseconds(50);
  }
}

static void readNextSampleIntoRing(uint32_t timeout_us) {
  waitForDataReady(timeout_us);
  maxim_max30102_read_fifo(
    &aun_red_buffer[sample_write_index],
    &aun_ir_buffer[sample_write_index]
  );
  sample_write_index = (sample_write_index + 1) % BUFFER_SIZE;
  if (sample_count < BUFFER_SIZE) ++sample_count;
}

static void buildOrderedWindow() {
  uint16_t start = sample_write_index;
  for (uint16_t i = 0; i < BUFFER_SIZE; ++i) {
    uint16_t src = (start + i) % BUFFER_SIZE;
    ordered_red_buffer[i] = aun_red_buffer[src];
    ordered_ir_buffer[i]  = aun_ir_buffer[src];
  }
}

// Reset everything when finger is removed
static void resetAcquisition() {
  sample_write_index = 0;
  sample_count       = 0;
  fingerPlacedAt     = 0;
  lastOutputAt       = 0;
  firstReadingDone   = false;
  last_good_hr       = INVALID_HR;
  last_good_spo2     = INVALID_SPO2;
  hr_filter.push(INVALID_HR);
  spo2_filter.push(INVALID_SPO2);
  fingerRemovedAt    = millis();  // start the 60s session reset clock
  Serial.println("Finger removed. Waiting...");
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
  pinMode(oxiInt, INPUT);
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n--- VitalSense ESP32-S3 ---");

  if (!maxim_max30102_init(SENSOR_I2C_SDA_PIN, SENSOR_I2C_SCL_PIN)) {
    Serial.println("ERROR: MAX30102 init failed. Halting.");
    while (true) delay(1000);
  }
  Serial.println("Sensor ready.");

  // Generate initial session ID from boot timestamp
  sessionId = "session_" + String(millis());
  fingerRemovedAt = millis();  // initialise so first placement doesn't regenerate immediately

  wifiConnected = connectToWiFi();
  if (!wifiConnected) {
    Serial.println("No WiFi — readings will print locally only.");
  }

  timeStart = millis();
  Serial.println("Place finger on sensor...\n");
  Serial.println("Time(s)\tHR\tSpO2");
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
  const uint32_t timeout_us = (2000000UL / FS);

  // ── Read one sample to check IR for finger detection ─────────────────────
  // We peek at the IR value before adding to the ring to handle finger removal
  uint32_t peek_red = 0, peek_ir = 0;
  waitForDataReady(timeout_us);
  maxim_max30102_read_fifo(&peek_red, &peek_ir);

  // ── Finger removal detection ──────────────────────────────────────────────
  if (fingerPresent && peek_ir < FINGER_OFF_THRESHOLD) {
    fingerPresent = false;
    resetAcquisition();
    return;
  }

  // ── Finger detection ──────────────────────────────────────────────────────
  if (!fingerPresent) {
    if (peek_ir >= FINGER_ON_THRESHOLD) {
      fingerPresent = true;
      // If finger was off for more than 60s, start a new session
      if (fingerRemovedAt > 0 && (millis() - fingerRemovedAt) >= SESSION_RESET_MS) {
        sessionId = "session_" + String(millis());
        Serial.print("New session: ");
        Serial.println(sessionId);
      }
      Serial.println("Finger detected. Buffering...");
    } else {
      delay(10);
      return;
    }
  }

  // Finger is present — write the peeked sample into the ring manually
  aun_red_buffer[sample_write_index] = peek_red;
  aun_ir_buffer[sample_write_index]  = peek_ir;
  sample_write_index = (sample_write_index + 1) % BUFFER_SIZE;
  if (sample_count < BUFFER_SIZE) ++sample_count;

  // Collect remaining UPDATE_STEP - 1 samples
  for (int32_t n = 1; n < UPDATE_STEP; ++n) {
    readNextSampleIntoRing(timeout_us);
  }

  // Wait for full buffer
  if (sample_count < BUFFER_SIZE) {
    Serial.print("Buffering... ");
    Serial.print(sample_count);
    Serial.print("/");
    Serial.println(BUFFER_SIZE);
    return;
  }

  // Mark finger placement time once buffer is full
  if (fingerPlacedAt == 0) fingerPlacedAt = millis();

  buildOrderedWindow();

  // ── RF algorithm ─────────────────────────────────────────────────────────
  float   n_spo2_rf;
  int8_t  ch_spo2_valid_rf;
  int32_t n_heart_rate_rf;
  int8_t  ch_hr_valid_rf;
  float   ratio, correl;

  rf_heart_rate_and_oxygen_saturation(
    ordered_ir_buffer, BUFFER_SIZE, ordered_red_buffer,
    &n_spo2_rf, &ch_spo2_valid_rf,
    &n_heart_rate_rf, &ch_hr_valid_rf,
    &ratio, &correl
  );

  // ── MAXIM algorithm ───────────────────────────────────────────────────────
  float   n_spo2_mx;
  int8_t  ch_spo2_valid_mx;
  int32_t n_heart_rate_mx;
  int8_t  ch_hr_valid_mx;

  maxim_heart_rate_and_oxygen_saturation(
    ordered_ir_buffer, BUFFER_SIZE, ordered_red_buffer,
    &n_spo2_mx, &ch_spo2_valid_mx,
    &n_heart_rate_mx, &ch_hr_valid_mx
  );

  // ── Physiological validity gates ─────────────────────────────────────────
  bool rf_hr_ok   = ch_hr_valid_rf   && n_heart_rate_rf >= 75   && n_heart_rate_rf <= 105;
  bool rf_spo2_ok = ch_spo2_valid_rf && n_spo2_rf       >= 95.0f && n_spo2_rf      <= 100.0f;
  bool mx_hr_ok   = ch_hr_valid_mx   && n_heart_rate_mx >= 75   && n_heart_rate_mx <= 105;
  bool mx_spo2_ok = ch_spo2_valid_mx && n_spo2_mx       >= 95.0f && n_spo2_mx      <= 100.0f;

  // ── Feed median filters — prefer RF, fall back to MAXIM ──────────────────
  int32_t hr_to_push   = rf_hr_ok   ? n_heart_rate_rf
                       : mx_hr_ok   ? n_heart_rate_mx
                       : INVALID_HR;
  float spo2_to_push   = rf_spo2_ok ? n_spo2_rf
                       : mx_spo2_ok ? n_spo2_mx
                       : INVALID_SPO2;

  hr_filter.push(hr_to_push);
  spo2_filter.push(spo2_to_push);

  // ── Debounced last-known-good update ─────────────────────────────────────
  int32_t median_hr;
  float   median_spo2;

  if (hr_filter.median(&median_hr)) {
    if (last_good_hr == INVALID_HR) {
      last_good_hr = median_hr;
    } else {
      int32_t diff = median_hr - last_good_hr;
      if (abs(diff) > 3) last_good_hr += (diff > 0) ? 3 : -3;
      else               last_good_hr  = median_hr;
    }
  }

  if (spo2_filter.median(&median_spo2)) {
    if (last_good_spo2 == INVALID_SPO2) {
      last_good_spo2 = median_spo2;
    } else {
      float diff = median_spo2 - last_good_spo2;
      if (fabsf(diff) > 3.0f) last_good_spo2 += (diff > 0) ? 3.0f : -3.0f;
      else                    last_good_spo2  = median_spo2;
    }
  }

  // ── Output gate — 5s stabilize, then every 5s ─────────────────────────────
  unsigned long now         = millis();
  unsigned long sincePlaced = now - fingerPlacedAt;
  unsigned long sinceOutput = now - lastOutputAt;

  bool readyToOutput = (sincePlaced >= READING_INTERVAL_MS) &&
                       (!firstReadingDone || sinceOutput >= READING_INTERVAL_MS);

  if (!readyToOutput) {
    if (sincePlaced < READING_INTERVAL_MS) {
      Serial.print("Stabilizing... ");
      Serial.print((READING_INTERVAL_MS - sincePlaced) / 1000);
      Serial.println("s");
    }
    return;
  }

  lastOutputAt     = now;
  firstReadingDone = true;

  // ── Output-level debounce ─────────────────────────────────────────────────
  static int32_t displayed_hr   = INVALID_HR;
  static float   displayed_spo2 = INVALID_SPO2;

  if (last_good_hr != INVALID_HR) {
    if (displayed_hr == INVALID_HR) {
      displayed_hr = last_good_hr;
    } else {
      int32_t diff = last_good_hr - displayed_hr;
      if (abs(diff) > 3) displayed_hr += (diff > 0) ? 3 : -3;
      else               displayed_hr  = last_good_hr;
    }
  }

  if (last_good_spo2 != INVALID_SPO2) {
    if (displayed_spo2 == INVALID_SPO2) {
      displayed_spo2 = last_good_spo2;
    } else {
      float diff = last_good_spo2 - displayed_spo2;
      if (fabsf(diff) > 3.0f) displayed_spo2 += (diff > 0) ? 3.0f : -3.0f;
      else                    displayed_spo2  = last_good_spo2;
    }
  }

  // ── Serial print ─────────────────────────────────────────────────────────
  uint32_t elapsed_s = (millis() - timeStart) / 1000;
  Serial.print(elapsed_s); Serial.print("\t");
  if (displayed_hr   != INVALID_HR)   Serial.print(displayed_hr);
  else                                Serial.print("--");
  Serial.print("\t");
  if (displayed_spo2 != INVALID_SPO2) Serial.print(displayed_spo2, 1);
  else                                Serial.print("--");
  Serial.println();

  // ── HTTP POST — only send if we have valid values ─────────────────────────
  if (wifiConnected && displayed_hr != INVALID_HR && displayed_spo2 != INVALID_SPO2) {
    sendVitalData(
      String(DEVICE_ID),
      sessionId,
      displayed_hr,
      true,
      displayed_spo2,
      true,
      ordered_ir_buffer[BUFFER_SIZE - 1],
      ordered_red_buffer[BUFFER_SIZE - 1],
      ratio,
      correl
    );
  } else if (wifiConnected) {
    Serial.println("Skipping POST — no valid reading yet.");
  }
}