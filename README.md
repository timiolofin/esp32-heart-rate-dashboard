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
```# Heart Rate Monitoring System

## Overview

This project is a cloud-connected physiological monitoring system that collects optical pulse data from a MAX30102 sensor using an ESP32-S3 microcontroller, transmits readings over Wi-Fi to a backend API, stores them in a database, and displays the latest data on a live web dashboard. The system is structured as an end-to-end IoT pipeline with embedded firmware, a FastAPI backend, persistent storage, and a browser-based monitoring interface.

The firmware now uses a buffered red/IR acquisition pipeline based on the MAX30102 and an RF-style processing algorithm for heart rate and oxygen saturation estimation, along with Wi-Fi transmission support and backend integration.

## Use Case

This system is designed for short, session-based fingertip monitoring. A user places a finger on the MAX30102 sensor for a brief session, and the device captures optical pulse data, processes the signal on the ESP32, and forwards measurements and signal metadata to the backend for storage and visualization.

The system is also structured to support multiple devices by tagging outbound payloads with a device identifier. This allows the backend to distinguish readings from different embedded nodes in a larger monitoring setup.

## What Data Will Be Collected and Why

**Data collected:**
- Heart rate estimate (BPM)
- Oxygen saturation estimate (SpO2)
- Raw IR signal value
- Raw red signal value
- Signal correlation / quality metadata
- Timestamp of each reading
- Device identifier

**Why:**  
This project focuses on physiological sensing rather than generic telemetry. Optical pulse sensing is a meaningful embedded systems problem because it combines hardware integration, signal acquisition, local processing, networking, storage, and visualization in one pipeline.

## Hardware

| Component | Purpose |
|---|---|
| ESP32-S3 | Main microcontroller with Wi-Fi connectivity |
| MAX30102 sensor module | Optical pulse sensor using red/IR LEDs |
| Jumper wires | I2C and power connections |
| USB cable | Power and programming |

**Communication:** MAX30102 → ESP32-S3 over I2C  
**Data transmission:** ESP32-S3 → Backend API over HTTP POST over Wi-Fi

## Backend and Database

- **API Framework:** FastAPI
- **Database:** SQLite
- **Auth:** Bearer token in request headers
- **Endpoints:**
  - `/data` — receive device readings
  - `/readings` — fetch stored readings
  - `/latest` — fetch most recent reading
  - `/dashboard` — live monitoring view

SQLite is currently used for lightweight persistent local storage. The backend structure can be upgraded later to PostgreSQL or another production database if cloud deployment is desired.

## Web Dashboard

A live dashboard is served directly by the backend.

**Features:**
- Latest reading display
- Finger/session status information
- Signal values and metadata
- Timestamped updates
- Auto-refreshing live view

The dashboard queries backend endpoints and displays the latest stored reading in the browser.

## System Data Flow

```text
MAX30102 Sensor
    ↓ I2C
ESP32-S3 Firmware
    ↓ HTTP POST
FastAPI Backend
    ↓ writes
SQLite Database
    ↓ queries
Web Dashboard
```

## Sampling and Processing Strategy

- Raw optical signal is sampled from the MAX30102 on the ESP32-S3
- Red and IR channels are collected in buffered batches
- Firmware processes the buffered signal for physiological estimation
- Aggregated values and signal metadata are transmitted to the backend at a lower rate than raw acquisition

This keeps the embedded side responsible for acquisition and signal processing while keeping the backend focused on ingestion, storage, and display.

## Basic Goals

- [x] ESP32-S3 communicates with MAX30102 over I2C
- [x] Buffered signal acquisition implemented on device
- [x] Firmware structured around red/IR batch processing
- [x] ESP32-S3 transmits readings over Wi-Fi to backend API
- [x] Backend validates requests and stores readings
- [x] Live dashboard displays latest data
- [ ] Historical chart visualization
- [ ] Optional containerized deployment
- [ ] Optional CI/CD polish

## Project Stack Summary

| Layer | Technology |
|---|---|
| Firmware | Arduino framework (C++) |
| Sensor Driver | MAX30102 custom driver |
| Signal Processing | RF-style buffered HR/SpO2 algorithm |
| API | FastAPI (Python) |
| Database | SQLite |
| Frontend | HTML + JavaScript served by FastAPI |
| Security | Bearer token |

## Repository Structure

```text
backend/
firmware/
  main/
    main.ino
    max30102.cpp
    max30102.h
    algorithm_by_RF.cpp
    algorithm_by_RF.h
    wifi_helper.h
    http_helper.h
    secrets.h
specs/
README.md
```

## How to Run

### Backend
```bash
cd backend
uvicorn main:app --host 0.0.0.0 --port 8000
```

Then open:
```text
http://<your-ip>:8000/dashboard
```

### ESP32 Firmware
- Open the sketch from `firmware/main/`
- Set Wi-Fi credentials in `secrets.h`
- Flash the ESP32-S3
- The device will initialize the MAX30102, connect to Wi-Fi, collect buffered sensor data, and send readings to the backend

### Requirements
- ESP32-S3 and backend machine must be on the same local network
- `secrets.h` must be configured locally
- Backend must be running before live device uploads are expected

## Current Status

- [x] Sensor-to-ESP32 integration complete
- [x] RF-style buffered acquisition pipeline integrated
- [x] MAX30102 custom driver integrated
- [x] Wi-Fi connection workflow implemented
- [x] HTTP POST transmission implemented
- [x] FastAPI backend implemented
- [x] SQLite storage implemented
- [x] Live dashboard implemented
- [ ] Historical chart visualization
- [ ] Deployment polish

## Firmware Notes

The firmware files are located in `firmware/main/`.

Current firmware capabilities:
- Initializes MAX30102 over I2C
- Reads red and IR optical data
- Buffers samples for signal processing
- Runs HR/SpO2 estimation pipeline
- Detects finger/session state
- Connects to configured Wi-Fi networks
- Sends structured JSON payloads to backend API

## Security Note

- Wi-Fi credentials are stored locally in `firmware/main/secrets.h`
- `secrets.h` should remain excluded from version control
- API tokens should be kept local and rotated as needed

## Timeline

| Week | Milestone |
|---|---|
| 9  | Project proposal, repo setup |
| 10 | Hardware connected, local sensor readout |
| 11 | Sensor-to-backend pipeline built |
| 12 | Dashboard integrated with backend |
| 13 | Firmware restructure, buffered algorithm integration, documentation updates |
| 14 | Final presentation |

---

*ECE-3824 — Spring 2026 | Temple University*


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