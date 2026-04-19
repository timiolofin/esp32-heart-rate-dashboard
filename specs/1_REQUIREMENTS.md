# System Requirements

## Overview

The system is a cloud-connected physiological monitoring platform that collects heart rate and oxygen saturation data from a MAX30102 sensor using an ESP32-S3 device and displays it on a live web dashboard.

---

## Functional Requirements

- The system must read heart rate and SpO2 data from the MAX30102 sensor
- The ESP32 must process raw PPG signals to compute BPM and oxygen saturation using a dual-algorithm pipeline (RF autocorrelation + MAXIM peak detection)
- The ESP32 must apply physiological validity gates, median filtering, and output debouncing to ensure stable readings
- The ESP32 must detect finger presence and absence and reset acquisition state on removal
- The ESP32 must send data to a backend server over Wi-Fi using HTTP POST
- The backend must validate incoming requests using a Bearer token
- The backend must store readings in a database with timestamps
- The web dashboard must display heart rate and SpO2 trends over time
- The system must support multiple devices using a device_id field
- The system must support session-based data collection using a session_id field
- A new session must be automatically generated if the finger has been absent for more than 60 seconds

---

## Non-Functional Requirements

- Data transmission must be secure (token-based authentication on all endpoints)
- The system must handle data uploads every 5 seconds per active session
- The system must support multiple concurrent devices
- The backend must log all incoming requests
- The system must be deployable using Docker

---

## Use Case

A user places their finger on the MAX30102 sensor. The system detects finger presence, waits 5 seconds for the signal to stabilize, then computes heart rate and SpO2 and sends readings to the cloud backend every 5 seconds. The dashboard updates automatically every 3 seconds. When the finger is removed, the device resets and waits for the next placement. If the finger is absent for more than 60 seconds, a new session begins on the next placement.

---

## Sampling Requirements

- Sensor sampling rate: 100 Hz (raw PPG signal via interrupt-driven acquisition)
- Algorithm update rate: every 10 new samples (~0.4s internal cycle)
- Data upload rate: every 5 seconds (one reading per output gate cycle)
- Dashboard polling rate: every 3 seconds
