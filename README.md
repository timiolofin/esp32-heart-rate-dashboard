# Heart Rate Monitoring System

## Overview

This project is a cloud-connected physiological monitoring system that collects heart rate data from a MAX30102 optical sensor using an ESP32 microcontroller, transmits readings over Wi-Fi to a backend API, stores them in a database, and displays trends on a web dashboard. The system is designed as an end-to-end IoT pipeline with token-protected data transfer and a live dashboard served directly from the backend.



## Use Case

This system is designed for short, session-based heart rate monitoring using a fingertip optical sensor. A user places their finger on the MAX30102 sensor for a brief session (30–120 seconds), and the system records and visualizes heart rate trends.

The system is designed to scale to multiple devices, where each sensor uploads data with a unique device ID and session ID. This allows the backend to distinguish between multiple users or sensors, similar to a multi-patient monitoring environment.



## What Data Will Be Collected and Why

**Data collected:**
- Heart rate (BPM) — averaged over a short sampling window
- Raw PPG signal amplitude (red/IR channel)
- Signal quality indicator
- Timestamp of each reading

**Why:**  
Heart rate is one of the most accessible physiological signals measurable with low-cost optical hardware. Rather than building a generic environmental sensor, this project targets a more meaningful signal. The goal is to build a pipeline that can store and visualize pulse data in a way that is useful, interpretable, and technically interesting.



## Hardware

| Component | Purpose |
|---|---|
| ESP32-S3 (with OLED) | Wi-Fi microcontroller — reads sensor, sends data |
| MAX30102 sensor module | Optical heart rate sensor (red/IR PPG) |
| Jumper wires | GPIO wiring between ESP32 and sensor |
| USB cable | Programming and power |

**Communication:** MAX30102 → ESP32 over I2C (SDA/SCL)  
**Data transmission:** ESP32 → Backend API over HTTP POST (Wi-Fi)



## Backend and Database

- **API Framework:** FastAPI (Python)
- **Database:** SQLite (local persistent storage)
- **Auth:** Bearer token in request headers
- **Endpoints:**
  - `/data` — receive sensor data (POST)
  - `/readings` — fetch recent readings
  - `/latest` — fetch most recent reading
  - `/dashboard` — live web dashboard

Note: SQLite is used for lightweight local persistence. The system is designed to be easily upgraded to PostgreSQL for cloud deployment.



## Web Dashboard

A live dashboard is implemented and served directly by the backend.

**Features:**
- Live BPM display
- Finger detection status
- Signal strength (IR value)
- Timestamp of latest reading
- Auto-refresh every ~2 seconds

The dashboard fetches data from `/latest` and updates in real time.



## System Data Flow

```
MAX30102 Sensor
    ↓ I2C
ESP32-S3 (computes BPM)
    ↓ HTTP POST (token-protected, every ~5 seconds)
FastAPI Backend (validates token, parses payload)
    ↓ writes
SQLite Database (stores timestamped readings)
    ↓ queries
Web Dashboard (displays live BPM, signal status, latest reading)
```



## Sampling Strategy

- Sensor sampling rate: ~100 Hz (raw PPG signal)
- Data transmission rate: every ~5 seconds (aggregated data)
- Each transmission contains averaged BPM and metadata rather than raw high-frequency data

This approach reduces network load while preserving meaningful physiological information.



## Basic Goals

- [x] ESP32 reads valid heart rate data from MAX30102
- [x] ESP32 transmits readings over Wi-Fi to backend API
- [x] Backend validates token and stores readings in database
- [x] Web dashboard displays live heart rate
- [ ] Historical chart visualization
- [ ] Docker deployment (optional)
- [ ] CI/CD pipeline (optional)



## Project Stack Summary

| Layer | Technology |
|---|---|
| Firmware | Arduino framework (C++) |
| API | FastAPI (Python) |
| Database | SQLite |
| Frontend | HTML + JavaScript (served by FastAPI) |
| Security | Bearer token (API key) |



## How to Run

### Backend
```bash
cd backend
uvicorn main:app --host 0.0.0.0 --port 8000
```

Then open:
```
http://<your-ip>:8000/dashboard
```

### ESP32
- Flash firmware from `/firmware/main/`
- Ensure Wi-Fi credentials are set in `secrets.h`
- ESP32 will automatically connect and send data every ~5 seconds

### Requirements
- ESP32 and computer must be on the same local network



## Current Status

- [x] Sensor connected and working locally
- [x] Stable BPM detection on ESP32-S3
- [x] Wi-Fi data transmission working
- [x] Backend API implemented (FastAPI)
- [x] SQLite database integration complete
- [x] Live dashboard implemented
- [ ] BPM stabilization improvements
- [ ] Historical chart visualization



## Firmware

The firmware files are located in `/firmware/main/`.

Current capabilities:
- Detects finger presence using IR signal
- Performs short calibration (~3 seconds)
- Computes and prints heart rate (BPM) via Serial
- Transmits BPM data over Wi-Fi to backend API

Security note:
- Wi-Fi credentials are stored locally in `firmware/main/secrets.h`
- `firmware/main/secrets.h` is excluded from version control using `.gitignore`



## Timeline

| Week | Milestone |
|---|---|
| 9  | Project proposal, GitHub repo created |
| 10 | Hardware connected, sensor reading data locally |
| 11 | Sensor → database pipeline working, API defined, tests written |
| 12 | Dashboard working with live data |
| 13 | Final polish — code, docs, spec files, README |
| 14 | In-class presentation |

---

*ECE-3824 — Spring 2026 | Temple University*