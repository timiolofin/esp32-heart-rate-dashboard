# VitalSense — Complete System Specification

*ECE-3824 — Spring 2026 | Temple University*

---

## Overview

VitalSense is a cloud-connected physiological monitoring system. It collects heart rate and oxygen saturation data from a MAX30102 optical sensor via an ESP32-S3 microcontroller, transmits readings over Wi-Fi to a FastAPI backend deployed on Render, stores them in a SQLite database, and displays live trends on a browser-based dashboard.

---

## System Architecture

```
MAX30102 Sensor
    ↓ I2C (SDA/SCL)
ESP32-S3 Firmware (Arduino/C++)
    ↓ HTTPS POST — Bearer token — every ~2s
FastAPI Backend (Python) — Render cloud
    ↓ writes
SQLite Database — readings.db
    ↓ queries (GET /latest, GET /history)
Dashboard (HTML/JS) — local browser
```

---

## Hardware

| Component | Role |
|---|---|
| ESP32-S3 | Wi-Fi microcontroller — runs firmware, sends data |
| MAX30102 | Optical pulse sensor — red/IR PPG signal |

**I2C pins:** SDA = GPIO5, SCL = GPIO6

---

## Firmware

**Language:** C++ (Arduino framework)  
**Key files:** `main.ino`, `max30102.cpp`, `algorithm_by_RF.cpp`, `http_helper.h`, `wifi_helper.h`

**Behavior:**
- Initializes MAX30102 over I2C
- Detects finger presence using IR threshold (>20000 = finger on)
- Debounces finger on/off transitions
- Generates HR and SpO2 estimates after 1.2s settle period
- Sends JSON payload to backend every 2 seconds while finger is present
- Stops transmitting on finger removal — last valid reading holds on dashboard

**Payload sent:**
```json
{
  "device_id": "esp32_001",
  "session_id": "session_001",
  "heart_rate": 76.0,
  "hr_valid": true,
  "spo2": 98.2,
  "spo2_valid": true,
  "ir": 102400,
  "red": 97800,
  "ratio": 0.98,
  "correlation": 0.96
}
```

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
**Hosting:** Opened locally in browser  
**Polling:** Every 3 seconds via `fetch()` with Bearer token header  
**Features:** Live BPM display, SpO2 arc gauge, animated ECG, dual-axis trend chart, bar chart with range color-coding, session/timestamp info, light/dark mode toggle

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

## Requirements Coverage

| Requirement | Status |
|---|---|
| Read HR from MAX30102 | Done |
| ESP32 computes BPM | Done |
| ESP32 sends data over Wi-Fi via HTTP POST | Done |
| Backend validates token | Done |
| Backend stores readings with timestamps | Done |
| Dashboard displays HR trends | Done |
| Multiple devices via device_id | Done |
| Session-based collection via session_id | Done |
| Docker-ready | Dockerfile present |
| Logging | Python `logging` module — all POSTs logged |

---

## Improvements With More Time

- Replace SQLite with PostgreSQL for production scalability
- WebSockets for true real-time push instead of polling
- Auto-generate session_id on ESP32 using boot timestamp
- Add user authentication on the dashboard
- Anomaly detection — alert when BPM goes outside normal range
- Full Docker + CI/CD deployment pipeline
- Input range validation on sensor fields (HR, SpO2 bounds checking)
