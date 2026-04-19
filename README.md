# VitalSense — Heart Rate Monitoring System

*ECE-3824 — Spring 2026 | Temple University*

---

## Overview

VitalSense is a cloud-connected physiological monitoring system that collects heart rate and oxygen saturation data from a MAX30102 optical sensor using an ESP32-S3 microcontroller, transmits readings over Wi-Fi to a FastAPI backend deployed on Render, stores them in a SQLite database, and displays live trends on a browser-based dashboard.

The system is structured as a complete end-to-end IoT pipeline: embedded firmware handles signal acquisition and processing, the backend handles ingestion and storage, and the frontend handles visualization.

---

## Use Case

A user places their finger on the MAX30102 sensor for a short session. The ESP32-S3 detects finger presence, processes the optical signal, and sends BPM and SpO2 readings to the backend every ~2 seconds. The dashboard updates automatically in real time.

The system supports multiple devices via `device_id` and multiple sessions via `session_id`, making it extensible to a multi-patient monitoring environment.

---

## System Data Flow

```
MAX30102 Sensor
    ↓ I2C (SDA=GPIO5, SCL=GPIO6)
ESP32-S3 Firmware (Arduino/C++)
    ↓ HTTPS POST — Bearer token — every ~2s
FastAPI Backend (Python) — deployed on Render
    ↓ writes
SQLite Database — readings.db
    ↓ queries (GET /latest, GET /history)
Dashboard (HTML/JS) — static file, opened in browser
```

---

## Hardware

| Component | Purpose |
|---|---|
| ESP32-S3 | Wi-Fi microcontroller — runs firmware, transmits data |
| MAX30102 | Optical pulse sensor — red/IR PPG channels |
| Jumper wires | I2C and power connections |
| USB cable | Power and programming |

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
| `http_helper.h` | HTTP POST to backend with Bearer token |
| `wifi_helper.h` | Multi-network Wi-Fi connection logic |
| `secrets.h` | Wi-Fi credentials — excluded from version control |

**Behavior:**
- Initializes MAX30102 over I2C
- Detects finger presence using IR threshold (>20000 = finger on)
- Debounces finger on/off events
- Settles for 1.2 seconds after finger placement
- Generates HR and SpO2 estimates via RF algorithm
- Sends JSON payload to backend every 2 seconds
- Stops transmitting on finger removal — last valid reading holds on dashboard

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
**Type:** Static HTML — open directly in browser  
**Polling:** Every 3 seconds via `fetch()` with Bearer token

**Features:**
- Live BPM display with animated ECG
- SpO2 arc gauge
- Dual-axis trend chart (HR + SpO2)
- Bar chart color-coded by BPM range
- Session ID and timestamp display
- Session averages and reading count
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
| Signal Processing | RF-style buffered HR/SpO2 algorithm |
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
Open `backend/dashboard.html` directly in a browser. No server needed.

### ESP32 Firmware
- Open `firmware/main/` in Arduino IDE
- Set Wi-Fi credentials in `secrets.h`
- Flash to ESP32-S3
- Device connects to Wi-Fi and begins sending data automatically

---

## Current Status

- [x] ESP32-S3 communicates with MAX30102 over I2C
- [x] RF-style buffered HR/SpO2 algorithm integrated
- [x] Finger detection with debouncing
- [x] Wi-Fi multi-network connection
- [x] HTTP POST with Bearer token auth
- [x] FastAPI backend deployed on Render
- [x] SQLite storage with EST timestamps
- [x] Session ID tracking
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
| 13 | Firmware restructure, RF algorithm, session ID, auth hardening |
| 14 | Testing, documentation, final presentation |

---

## Improvements With More Time

- Replace SQLite with PostgreSQL for production scalability
- WebSockets for true real-time push instead of polling
- Auto-generated session ID on the ESP32 using boot timestamp
- User authentication on the dashboard
- Anomaly alerts when BPM goes outside normal range
- Full Docker + CI/CD deployment pipeline
- Input range validation on sensor fields
