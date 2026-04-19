// =============================================================================
// main.ino — VitalSense Firmware
// ESP32-S3 firmware for the VitalSense heart rate and SpO2 monitoring system.
// Reads PPG signals from a MAX30102 sensor, processes them through a dual
// RF + MAXIM algorithm pipeline, and transmits validated readings to the
// cloud backend over Wi-Fi every 5 seconds.
//
// Hardware: ESP32-S3 | MAX30102 (SDA=GPIO5, SCL=GPIO6, INT=GPIO7)
// ECE-3824 — Spring 2026 | Temple University
// =============================================================================

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
const byte oxiInt             = 7;   // MAX30102 interrupt pin — goes LOW when data is ready
const int  SENSOR_I2C_SDA_PIN = 5;
const int  SENSOR_I2C_SCL_PIN = 6;

// ── Finger detection thresholds ──────────────────────────────────────────────
// IR values above FINGER_ON indicate a finger is present.
// Below FINGER_OFF, the finger is considered removed. Hysteresis prevents
// false triggers from minor pressure changes.
static const uint32_t FINGER_ON_THRESHOLD  = 20000;
static const uint32_t FINGER_OFF_THRESHOLD = 10000;

// ── Sample ring buffers ───────────────────────────────────────────────────────
// Raw samples are written into circular ring buffers. Before each algorithm
// run, the ring is unrolled into a flat ordered window (oldest → newest).
uint32_t aun_ir_buffer[BUFFER_SIZE];
uint32_t aun_red_buffer[BUFFER_SIZE];
uint32_t ordered_ir_buffer[BUFFER_SIZE];
uint32_t ordered_red_buffer[BUFFER_SIZE];

// ── Update cadence ────────────────────────────────────────────────────────────
// The algorithm runs every UPDATE_STEP new samples (~0.4s at 25 Hz effective).
// MEDIAN_HISTORY controls how many readings the smoothing filter holds.
const int32_t UPDATE_STEP    = 10;
const uint8_t MEDIAN_HISTORY = 4;

// Sentinel values used to mark filter slots as empty / not yet valid
const int32_t INVALID_HR   = -999;
const float   INVALID_SPO2 = -999.0f;

uint16_t sample_write_index = 0;
uint16_t sample_count       = 0;

// Separate median filters for HR and SpO2 — invalid sentinels are excluded
// from the median calculation so early invalid readings don't bias the output
MedianFilter<int32_t, MEDIAN_HISTORY> hr_filter(INVALID_HR);
MedianFilter<float,   MEDIAN_HISTORY> spo2_filter(INVALID_SPO2);

// ── Output timing ─────────────────────────────────────────────────────────────
// First output is held for 5s after buffer fills (stabilization window).
// Subsequent outputs fire every 5s regardless of algorithm cycle rate.
uint32_t timeStart                      = 0;
const unsigned long READING_INTERVAL_MS = 5000UL;
unsigned long fingerPlacedAt            = 0;
unsigned long lastOutputAt              = 0;
bool firstReadingDone                   = false;

// ── Finger state ──────────────────────────────────────────────────────────────
bool fingerPresent = false;

// ── Last known good values ────────────────────────────────────────────────────
// Held between output cycles so the display never shows dashes after the
// first valid reading, even if the algorithm temporarily loses lock.
int32_t last_good_hr   = INVALID_HR;
float   last_good_spo2 = INVALID_SPO2;

// ── Session management ────────────────────────────────────────────────────────
// Session ID is generated at boot. If the finger is absent for more than
// SESSION_RESET_MS, a new session ID is generated on the next placement,
// keeping data from separate use sessions grouped correctly in the database.
String sessionId = "";
unsigned long fingerRemovedAt        = 0;
const unsigned long SESSION_RESET_MS = 60000UL;

// ── WiFi state ────────────────────────────────────────────────────────────────
bool wifiConnected = false;

// =============================================================================
// waitForDataReady()
// Blocks until the MAX30102 INT pin goes LOW, indicating a new sample is
// in the FIFO. Times out after timeout_us microseconds to avoid hanging
// if the interrupt line is unresponsive.
// =============================================================================
static void waitForDataReady(uint32_t timeout_us) {
  uint32_t t = micros();
  while (digitalRead(oxiInt) == HIGH) {
    if ((micros() - t) > timeout_us) break;
    delayMicroseconds(50);
  }
}

// =============================================================================
// readNextSampleIntoRing()
// Waits for data ready, reads one red+IR sample from the sensor FIFO,
// and writes it into the circular ring buffer at the current write index.
// =============================================================================
static void readNextSampleIntoRing(uint32_t timeout_us) {
  waitForDataReady(timeout_us);
  maxim_max30102_read_fifo(
    &aun_red_buffer[sample_write_index],
    &aun_ir_buffer[sample_write_index]
  );
  sample_write_index = (sample_write_index + 1) % BUFFER_SIZE;
  if (sample_count < BUFFER_SIZE) ++sample_count;
}

// =============================================================================
// buildOrderedWindow()
// Unrolls the circular ring buffer into a flat chronological array.
// The ring's oldest sample starts at sample_write_index (next write slot),
// so we iterate from there, wrapping around.
// =============================================================================
static void buildOrderedWindow() {
  uint16_t start = sample_write_index;
  for (uint16_t i = 0; i < BUFFER_SIZE; ++i) {
    uint16_t src = (start + i) % BUFFER_SIZE;
    ordered_red_buffer[i] = aun_red_buffer[src];
    ordered_ir_buffer[i]  = aun_ir_buffer[src];
  }
}

// =============================================================================
// resetAcquisition()
// Called on finger removal. Clears all buffers, filters, and timing state
// so the next placement starts completely fresh. Also stamps fingerRemovedAt
// to start the 60s session reset countdown.
// =============================================================================
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
  fingerRemovedAt    = millis();
  Serial.println("Finger removed. Waiting...");
}

// =============================================================================
// setup()
// =============================================================================
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

  sessionId       = "session_" + String(millis());
  fingerRemovedAt = millis();  // prevent immediate session reset on first placement

  wifiConnected = connectToWiFi();
  if (!wifiConnected) {
    Serial.println("No WiFi — readings will print locally only.");
  }

  timeStart = millis();
  Serial.println("Place finger on sensor...\n");
  Serial.println("Time(s)\tHR\tSpO2");
}

// =============================================================================
// loop()
// Main acquisition and processing cycle. Each iteration:
//   1. Reads one sample and checks for finger presence/removal
//   2. Collects UPDATE_STEP samples into the ring buffer
//   3. Once the buffer is full, runs both algorithms on an ordered window
//   4. Applies validity gates, median filter, and debounce
//   5. Every 5s, prints and POSTs the displayed values
// =============================================================================
void loop() {
  const uint32_t timeout_us = (2000000UL / FS);  // 2 sample periods at 25 Hz

  // Read one sample up front to inspect IR level for finger detection
  uint32_t peek_red = 0, peek_ir = 0;
  waitForDataReady(timeout_us);
  maxim_max30102_read_fifo(&peek_red, &peek_ir);

  // ── Finger removal ────────────────────────────────────────────────────────
  if (fingerPresent && peek_ir < FINGER_OFF_THRESHOLD) {
    fingerPresent = false;
    resetAcquisition();
    return;
  }

  // ── Finger detection ──────────────────────────────────────────────────────
  if (!fingerPresent) {
    if (peek_ir >= FINGER_ON_THRESHOLD) {
      fingerPresent = true;
      // Start a new session if finger was absent for more than 60s
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

  // Write the peeked sample into the ring, then collect the rest of the batch
  aun_red_buffer[sample_write_index] = peek_red;
  aun_ir_buffer[sample_write_index]  = peek_ir;
  sample_write_index = (sample_write_index + 1) % BUFFER_SIZE;
  if (sample_count < BUFFER_SIZE) ++sample_count;

  for (int32_t n = 1; n < UPDATE_STEP; ++n) {
    readNextSampleIntoRing(timeout_us);
  }

  if (sample_count < BUFFER_SIZE) {
    Serial.print("Buffering... ");
    Serial.print(sample_count);
    Serial.print("/");
    Serial.println(BUFFER_SIZE);
    return;
  }

  if (fingerPlacedAt == 0) fingerPlacedAt = millis();

  buildOrderedWindow();

  // ── RF algorithm ─────────────────────────────────────────────────────────
  // Autocorrelation-based periodicity detection for HR.
  // Red/IR AC ratio method for SpO2. Preferred over MAXIM when valid.
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
  // Peak detection reference implementation. Used as fallback when RF
  // does not produce a valid result.
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
  // Rejects readings outside the expected resting ranges before they can
  // influence the median filter. Prevents outliers (e.g. 300 BPM, 12% SpO2)
  // from corrupting the smoothed output.
  bool rf_hr_ok   = ch_hr_valid_rf   && n_heart_rate_rf >= 75   && n_heart_rate_rf <= 105;
  bool rf_spo2_ok = ch_spo2_valid_rf && n_spo2_rf       >= 95.0f && n_spo2_rf      <= 100.0f;
  bool mx_hr_ok   = ch_hr_valid_mx   && n_heart_rate_mx >= 75   && n_heart_rate_mx <= 105;
  bool mx_spo2_ok = ch_spo2_valid_mx && n_spo2_mx       >= 95.0f && n_spo2_mx      <= 100.0f;

  // ── Median filter input — RF preferred, MAXIM as fallback ────────────────
  int32_t hr_to_push = rf_hr_ok ? n_heart_rate_rf : mx_hr_ok ? n_heart_rate_mx : INVALID_HR;
  float spo2_to_push = rf_spo2_ok ? n_spo2_rf : mx_spo2_ok ? n_spo2_mx : INVALID_SPO2;

  hr_filter.push(hr_to_push);
  spo2_filter.push(spo2_to_push);

  // ── Layer 1 debounce — last known good value ──────────────────────────────
  // Updates the internal best estimate each algorithm cycle. If the new
  // median jumps by more than 3 units, only steps by 3 toward it.
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

  // ── Output gate — 5s stabilize then every 5s ─────────────────────────────
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

  // ── Layer 2 debounce — displayed value ───────────────────────────────────
  // A second debounce at the output level. What actually gets printed and
  // posted also steps by max 3 per 5s cycle, so even if last_good jumps,
  // the number on screen and in the database changes gradually.
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

  // ── Serial output ─────────────────────────────────────────────────────────
  uint32_t elapsed_s = (millis() - timeStart) / 1000;
  Serial.print(elapsed_s); Serial.print("\t");
  if (displayed_hr   != INVALID_HR)   Serial.print(displayed_hr);
  else                                Serial.print("--");
  Serial.print("\t");
  if (displayed_spo2 != INVALID_SPO2) Serial.print(displayed_spo2, 1);
  else                                Serial.print("--");
  Serial.println();

  // ── HTTP POST ─────────────────────────────────────────────────────────────
  // Only transmits when both HR and SpO2 have valid displayed values.
  // Skips silently if WiFi is unavailable — readings still print to serial.
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