# VitalSense — Heart Rate Monitoring System

*ECE-3824 — Spring 2026 | Temple University*

---

## Overview

VitalSense is a cloud-connected physiological monitoring system that collects heart rate and oxygen saturation data from a MAX30102 optical sensor using an ESP32-S3 microcontroller, transmits readings over Wi-Fi to a FastAPI backend deployed on Render, stores them in a SQLite database, and displays live trends on a browser-based dashboard.

The system is structured as a complete end-to-end IoT pipeline: embedded firmware handles signal acquisition and processing, the backend handles ingestion and storage, and the frontend handles visualization.

---

## Use Case

A user places their finger on the MAX30102 sensor. The ESP32-S3 detects finger presence, waits 5 seconds for the signal to stabilize, then processes the optical signal and sends BPM and SpO2 readings to the backend every 5 seconds. The dashboard updates automatically in real time. When the finger is removed, the device resets and waits for the next placement.

The system supports multiple devices via `device_id` and multiple sessions via `session_id`, making it extensible to a multi-patient monitoring environment.

---

## System Data Flow

```
MAX30102 Sensor
    ↓ I2C (SDA=GPIO5, SCL=GPIO6, INT=GPIO7)
ESP32-S3 Firmware (Arduino/C++)
    ↓ HTTPS POST — Bearer token — every 5s
FastAPI Backend (Python) — deployed on Render
    ↓ writes
SQLite Database — readings.db
    ↓ queries (GET /latest, GET /history)
Dashboard (HTML/JS) — static file, hosted on GitHub Pages
```

---

## Hardware

| Component | Purpose |
|---|---|
| ESP32-S3 | Wi-Fi microcontroller — runs firmware, transmits data |
| MAX30102 | Optical pulse sensor — red/IR PPG channels |
| Jumper wires | I2C, interrupt, and power connections |
| USB cable | Power and programming |

### Wiring

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
**Location:** `firmware/main/`

**Key files:**

| File | Purpose |
|---|---|
| `main.ino` | Main loop — finger detection, signal processing, transmission |
| `max30102.cpp/h` | Custom MAX30102 I2C driver |
| `algorithm_by_RF.cpp/h` | RF-style buffered HR/SpO2 estimation algorithm |
| `algorithm.cpp/h` | MAXIM reference HR/SpO2 algorithm (fallback) |
| `median_filter.h` | Fixed-size median filter template for output smoothing |
| `http_helper.h` | HTTP POST to backend with Bearer token |
| `wifi_helper.h` | Multi-network Wi-Fi connection logic |
| `secrets.h` | Wi-Fi credentials — excluded from version control |

**Signal Processing Pipeline:**

The firmware runs two algorithms on every update cycle — the RF autocorrelation-based algorithm and the MAXIM peak-detection algorithm. Readings are filtered through a 4-sample median filter, validated against physiological ranges, and smoothed with a two-layer debounce. The RF algorithm is preferred when valid; MAXIM is used as fallback.

**Behavior:**
- Initializes MAX30102 over I2C with interrupt-driven sampling on GPIO7
- Detects finger presence using IR threshold (>20000 = finger on, <10000 = finger off)
- Resets all state cleanly on finger removal
- Fills a 100-sample ring buffer before running algorithms
- Waits 5 seconds after buffer fills for signal stabilization
- Outputs one reading every 5 seconds, held at last known good value between updates
- Session ID auto-generated from boot timestamp
- Sends JSON payload to backend on each valid output cycle
- Continues operating locally if Wi-Fi is unavailable

---

## Backend

**Language:** Python  
**Framework:** FastAPI  
**Database:** SQLite (`readings.db`)  
**Deployment:** Render (cloud)  
**Location:** `backend/main.py`

**Endpoints:**

| Method | Endpoint | Auth | Description |
|---|---|---|---|
| GET | `/` | No | Health check |
| POST | `/data` | Yes | Receive sensor reading from ESP32 |
| GET | `/latest` | Yes | Most recent reading |
| GET | `/history?limit=50` | Yes | Last N readings (max 200) |

**Auth:** All endpoints except `/` require `Authorization: Bearer <token>` header. Invalid or missing token returns `401 Unauthorized`.

**Token:** Read from environment variable `API_TOKEN`, defaults to `test123`.

**Timestamps:** Stored in Eastern Time (America/New_York) using `zoneinfo`.

---

## Web Dashboard

**File:** `backend/dashboard.html`  
**Type:** Static HTML — hosted on GitHub Pages  
**Live URL:** `https://timiolofin.github.io/esp32-heart-rate-dashboard/backend/dashboard.html`  
**Polling:** Every 3 seconds via `fetch()` with Bearer token

**Features:**
- Live BPM display with animated ECG
- BPM color changes to amber if outside normal range (< 60 or > 100 BPM)
- SpO2 arc gauge with color coding (blue = normal, amber = low, red = critical)
- Finger detection status indicator — updates in real time
- Last seen counter — shows seconds since last reading
- Dual-axis trend chart (HR + SpO2)
- Bar chart color-coded by BPM range
- Session ID, device ID, and timestamp display
- Session avg, min, and max HR + avg SpO2
- Total readings captured
- Light / dark mode toggle

---

## Security

| Layer | Mechanism |
|---|---|
| ESP32 → Backend | Bearer token on every POST |
| Frontend → Backend | Bearer token on every GET |
| Invalid token | 401 Unauthorized |
| Database | Not directly exposed — only accessible via API |
| Wi-Fi credentials | Stored in `secrets.h`, excluded via `.gitignore` |

---

## Testing

**Framework:** pytest + FastAPI TestClient  
**File:** `backend/test_main.py`  
**Count:** 15 unit tests  
**Test DB:** Temporary SQLite via `monkeypatch` — production data never touched

**Coverage:** Authentication, data validation, DB writes, data retrieval, edge cases, multi-device support, limit capping

```bash
cd backend
source venv/bin/activate
pytest test_main.py -v
```

Expected: **15 passed**

---

## Project Stack

| Layer | Technology |
|---|---|
| Firmware | Arduino (C++) |
| Sensor Driver | Custom MAX30102 I2C driver |
| Signal Processing | RF autocorrelation + MAXIM peak-detection, dual-algorithm with fallback |
| Output Smoothing | 4-sample median filter + two-layer debounce |
| Backend | FastAPI (Python) |
| Database | SQLite |
| Frontend | HTML + JavaScript |
| Deployment | Render (cloud) |
| Security | Bearer token |

---

## Repository Structure

```
backend/
    main.py
    dashboard.html
    test_main.py
    requirements.txt
    Dockerfile
    readings.db
firmware/
    main/
        main.ino
        max30102.cpp
        max30102.h
        algorithm_by_RF.cpp
        algorithm_by_RF.h
        algorithm.cpp
        algorithm.h
        median_filter.h
        http_helper.h
        wifi_helper.h
        secrets.h
specs/
    1_REQUIREMENTS.md
    2_DATA_MODELS.md
    3_API_SPEC.md
    4_TESTS.md
    SINGLE_FILE_SPEC.md
README.md
```

---

## How to Run

### Backend (local)
```bash
cd backend
uvicorn main:app --host 0.0.0.0 --port 8000
```

### Backend (deployed)
Live at: `https://esp32-heart-rate-dashboard.onrender.com`

### Frontend
Open `backend/dashboard.html` directly in a browser, or visit the hosted version:
```
https://timiolofin.github.io/esp32-heart-rate-dashboard/backend/dashboard.html
```

### ESP32 Firmware
- Open `firmware/main/` in Arduino IDE
- Set board to **ESP32S3 Dev Module** with **USB CDC On Boot: Enabled**
- Set Wi-Fi credentials in `secrets.h`
- Flash to ESP32-S3
- Open Serial Monitor at 115200 baud
- Place finger on sensor — first reading appears after buffering + 5s stabilization

---

## Current Status

- [x] ESP32-S3 communicates with MAX30102 over I2C with interrupt-driven sampling
- [x] Dual-algorithm HR/SpO2 pipeline (RF + MAXIM) with median filter and debounce
- [x] Physiological validity gates (75–105 BPM, 95–100% SpO2)
- [x] Finger detection and removal with clean state reset
- [x] 5-second stabilization window + 5-second output interval
- [x] Wi-Fi multi-network connection
- [x] HTTP POST with Bearer token auth
- [x] FastAPI backend deployed on Render
- [x] SQLite storage with EST timestamps
- [x] Session ID tracking — auto-generated at boot
- [x] Live dashboard with trend charts
- [x] Auth on all GET and POST endpoints
- [x] 15 passing backend unit tests
- [x] Full spec documentation

---

## Timeline

| Week | Milestone |
|---|---|
| 9 | Project proposal, repo setup |
| 10 | Hardware connected, local sensor readout |
| 11 | Sensor-to-backend pipeline working |
| 12 | Dashboard integrated with live data |
| 13 | Firmware restructure, dual RF/MAXIM algorithm, session ID, auth hardening |
| 14 | Signal processing refinement, output smoothing, testing, documentation |

---

## Improvements With More Time

- Replace SQLite with PostgreSQL for production scalability
- WebSockets for true real-time push instead of polling
- Hardware fix for MAX30102 clone board LED resistor issue for more consistent signal
- User authentication on the dashboard
- Anomaly alerts when BPM goes outside normal range
- Full Docker + CI/CD deployment pipeline
- Input range validation on sensor fields
