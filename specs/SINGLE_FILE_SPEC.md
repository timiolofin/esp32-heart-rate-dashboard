# VitalSense — Complete System Specification

*ECE-3824 — Spring 2026 | Temple University*

---

## Overview

VitalSense is a cloud-connected physiological monitoring system. It collects heart rate and oxygen saturation data from a MAX30102 optical sensor via an ESP32-S3 microcontroller, transmits readings over Wi-Fi to a FastAPI backend deployed on Render, stores them in a SQLite database, and displays live trends on a browser-based dashboard.

---

## System Architecture

```
MAX30102 Sensor
    ↓ I2C (SDA=GPIO5, SCL=GPIO6, INT=GPIO7)
ESP32-S3 Firmware (Arduino/C++)
    ↓ HTTPS POST — Bearer token — every 5s
FastAPI Backend (Python) — Render cloud
    ↓ writes
SQLite Database — readings.db
    ↓ queries (GET /latest, GET /history)
Dashboard (HTML/JS) — GitHub Pages / local browser
```

---

## Hardware

| Component | Role |
|---|---|
| ESP32-S3 | Wi-Fi microcontroller — runs firmware, sends data |
| MAX30102 | Optical pulse sensor — red/IR PPG signal |

**Wiring:**

| ESP32-S3 | MAX30102 |
|---|---|
| 3.3V | VIN |
| GND | GND |
| GPIO5 | SDA |
| GPIO6 | SCL |
| GPIO7 | INT |

---

## Firmware

**Language:** C++ (Arduino framework)  
**Key files:** `main.ino`, `max30102.cpp/h`, `algorithm_by_RF.cpp/h`, `algorithm.cpp/h`, `median_filter.h`, `http_helper.h`, `wifi_helper.h`

**Signal Processing Pipeline:**

Raw PPG samples are acquired at 100 Hz via interrupt on GPIO7 and written into a 100-sample ring buffer. Once the buffer is full, two algorithms run in parallel on every update cycle (every 10 new samples):

- **RF algorithm** — autocorrelation-based periodicity detection for HR, red/IR ratio method for SpO2
- **MAXIM algorithm** — peak detection reference implementation

Results from both are passed through physiological validity gates. The RF result is preferred when valid; MAXIM is used as fallback. Valid readings are pushed into a 4-sample median filter. The median output is then smoothed by a two-layer debounce that limits value changes to 3 units per output cycle, preventing sudden jumps.

**Output and Transmission Behavior:**
- Waits 5 seconds after buffer fills before producing first output
- Outputs one reading every 5 seconds
- Holds last known good value between output cycles
- Sends HTTP POST to backend only when a valid displayed value exists
- Skips POST if no valid reading has been established yet

**Session Management:**
- Session ID is auto-generated from `millis()` at boot
- Session ID regenerates if finger has been absent for more than 60 seconds
- Brief removals under 60 seconds continue the same session

**Finger Detection:**
- Finger on: IR > 20000
- Finger off: IR < 10000
- On removal: all buffers, filters, and state are fully reset

---

## Backend

**Language:** Python  
**Framework:** FastAPI  
**Deployment:** Render (cloud)  
**Database:** SQLite (`readings.db`)  
**Auth:** Bearer token via `Authorization` header  
**Token source:** `os.getenv("API_TOKEN", "test123")`

**Endpoints:**

| Method | Path | Auth | Description |
|---|---|---|---|
| GET | `/` | No | Health check |
| POST | `/data` | Yes | Receive sensor reading |
| GET | `/latest` | Yes | Most recent reading |
| GET | `/history` | Yes | Last N readings (max 200) |

---

## Database

**Type:** SQLite  
**Table:** `readings`  
**Key columns:** `id`, `timestamp`, `device_id`, `session_id`, `heart_rate`, `hr_valid`, `spo2`, `spo2_valid`, `ir`, `red`, `ratio`, `correlation`  
**Timestamps:** Eastern Time (America/New_York) via `zoneinfo`

---

## Frontend

**Type:** Static HTML file (`dashboard.html`)  
**Hosting:** GitHub Pages  
**Live URL:** `https://timiolofin.github.io/esp32-heart-rate-dashboard/backend/dashboard.html`  
**Polling:** Every 3 seconds via `fetch()` with Bearer token header  
**Features:** Live BPM display, SpO2 arc gauge, animated ECG, dual-axis trend chart, bar chart with range color-coding, session/timestamp info, finger detection status, last seen counter, light/dark mode toggle

---

## Security

| Layer | Mechanism |
|---|---|
| ESP32 → Backend | Bearer token on every POST |
| Frontend → Backend | Bearer token on every GET |
| Invalid token | 401 Unauthorized returned |
| Database | Not directly exposed — only accessible through API |
| Wi-Fi credentials | Stored in `secrets.h`, excluded from version control via `.gitignore` |

---

## Testing

**Framework:** pytest + FastAPI TestClient  
**Test file:** `backend/test_main.py`  
**Test count:** 15 unit tests  
**Test DB:** Temporary SQLite via `monkeypatch` — production data never touched

**Coverage:** Authentication, data validation, DB writes, data retrieval, edge cases, multi-device support

Run with:
```bash
cd backend && pytest test_main.py -v
```

---

## CI/CD

**Platform:** GitHub Actions  
**Workflow file:** `.github/workflows/test.yml`  
**Trigger:** Every push and pull request to `main`

**Pipeline steps:**
1. Checkout repository
2. Set up Python 3.12
3. Install `backend/requirements.txt`
4. Run `pytest test_main.py -v` from the `backend/` directory

Pass/fail status is reported as a commit check on every push. All 15 tests are expected to pass.

---

## Requirements Coverage

| Requirement | Status |
|---|---|
| Read HR and SpO2 from MAX30102 | Done |
| Dual-algorithm signal processing (RF + MAXIM) | Done |
| Physiological validity gates | Done |
| Median filter and output debounce | Done |
| Finger detection and removal reset | Done |
| Session ID auto-generation and regeneration after 60s | Done |
| ESP32 sends data over Wi-Fi via HTTP POST | Done |
| Backend validates token | Done |
| Backend stores readings with timestamps | Done |
| Dashboard displays HR and SpO2 trends | Done |
| Multiple devices via device_id | Done |
| Session-based collection via session_id | Done |
| Docker-ready | Dockerfile present |
| Logging | Python `logging` module — all POSTs logged |
| CI/CD pipeline | GitHub Actions — pytest runs on every push to main |

---

## Improvements With More Time

- Replace SQLite with PostgreSQL for production scalability
- WebSockets for true real-time push instead of polling
- Hardware fix for MAX30102 clone board LED resistor issue for more consistent PPG signal
- User authentication on the dashboard
- Anomaly detection — alert when BPM goes outside normal range
- Full Docker Compose deployment with automated build pipeline
- Input range validation on sensor fields
