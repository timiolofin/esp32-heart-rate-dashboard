# Heart Rate Monitoring System

## Overview

This project is a cloud-connected physiological monitoring system that collects heart rate data from a MAX30102 optical sensor using an ESP32 microcontroller, transmits readings over Wi-Fi to a backend API, stores them in a database, and displays trends on a web dashboard. The system is designed as an end-to-end IoT pipeline with token-protected data transfer, cloud deployment via Docker, and a CI/CD workflow through GitHub Actions.



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
| Breadboard | Prototyping connections |
| Jumper wires | GPIO wiring between ESP32 and sensor |
| USB cable | Programming and power |

**Communication:** MAX30102 → ESP32 over I2C (SDA/SCL)  
**Data transmission:** ESP32 → Backend API over HTTP POST (Wi-Fi)


## Backend and Database (Planned)

- **API Framework:** FastAPI (Python)
- **Database:** PostgreSQL
- **Auth:** Bearer token in request headers — all endpoints protected
- **Logging:** Structured JSON logs per request
- **CI/CD:** GitHub Actions 
- **Deployment:** Docker + cloud host (AWS / Render / Railway)



## Web Dashboard (What the User Will See)

- Current heart rate (live BPM value)
- Historical heart rate graph (line chart over time)
- Signal quality indicator (good / poor / no signal)
- Session log — list of recent reading sessions with timestamps
- Min / Max / Average BPM per session



## System Data Flow

```
MAX30102 Sensor
    ↓ I2C
ESP32-S3 (computes BPM)
    ↓ HTTP POST (token-protected, every ~5 seconds)
FastAPI Backend (validates token, parses payload)
    ↓ writes
PostgreSQL Database (stores timestamped readings)
    ↓ queries
Web Dashboard (displays charts, live value, session history)
```


## Sampling Strategy

- Sensor sampling rate: ~100 Hz (raw PPG signal)
- Data transmission rate: every ~5 seconds (aggregated data)
- Each transmission contains averaged BPM and metadata rather than raw high-frequency data

This approach reduces network load while preserving meaningful physiological information.



## Basic Goals

- [ ] ESP32 reads valid heart rate data from MAX30102
- [ ] ESP32 transmits readings over Wi-Fi to backend API
- [ ] Backend validates token and stores readings in database
- [ ] Web dashboard displays historical heart rate as a line chart
- [ ] System is secured with token-based authentication
- [ ] Backend and database run in Docker containers
- [ ] CI/CD pipeline runs tests and deploys on push



## Stretch Goals

- [ ] Real-time dashboard updates (WebSockets or polling)
- [ ] Signal quality analysis (classify readings as valid, noisy, or absent)
- [ ] SpO₂ estimation from red/IR ratio
- [ ] Multi-user session tracking (tag readings by session ID)



## Project Stack Summary

| Layer | Technology |
|---|---|
| Firmware | Arduino framework (C++) |
| API | FastAPI (Python) |
| Database | PostgreSQL |
| Frontend | Flask templates + Chart.js |
| Deployment | Docker + cloud host |
| CI/CD | GitHub Actions |
| Security | Bearer token (API key) |



## Timeline

| Week | Milestone |
|---|---|
| 9  | Project proposal, GitHub repo created |
| 10 | Hardware connected, sensor reading data locally |
| 11 | Sensor → database pipeline working, API defined, tests written |
| 12 | Dashboard working with charts |
| 13 | Final polish — code, docs, spec files, README |
| 14 | In-class presentation |

---

## Current Status

- [x] Sensor connected and working locally
- [x] Stable BPM detection on ESP32-S3
- [ ] Wi-Fi data transmission
- [ ] Backend API
- [ ] Database integration
- [ ] Web dashboard

## Firmware

The firmware is located in `/firmware/main/main.ino`.

Current capabilities:
- Detects finger presence using IR signal
- Performs short calibration (~3 seconds)
- Computes and prints heart rate (BPM) via Serial

Security note:
- Wi-Fi credentials are stored locally in firmware/main/secrets.h
- firmware/main/secrets.h is excluded from version control using .gitignore

Next step:
- Transmit BPM data over Wi-Fi to backend API

*ECE-3824 — Spring 2026 | Temple University*