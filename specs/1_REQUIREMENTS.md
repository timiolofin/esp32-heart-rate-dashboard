# System Requirements

## Overview

The system is a cloud-connected physiological monitoring platform that collects heart rate and oxygen saturation data from a MAX30102 sensor using an ESP32-S3 device and displays it on a live web dashboard.

---

## Functional Requirements

- The system must read heart rate and SpO2 data from the MAX30102 sensor
- The ESP32 must process raw PPG signals to compute BPM and oxygen saturation
- The ESP32 must send data to a backend server over Wi-Fi using HTTP POST
- The backend must validate incoming requests using a Bearer token
- The backend must store readings in a database with timestamps
- The web dashboard must display heart rate and SpO2 trends over time
- The system must support multiple devices using a device_id field
- The system must support session-based data collection using a session_id field

---

## Non-Functional Requirements

- Data transmission must be secure (token-based authentication on all endpoints)
- The system must handle data uploads every ~2 seconds
- The system must support multiple concurrent devices
- The backend must log all incoming requests
- The system must be deployable using Docker

---

## Use Case

A user places their finger on the MAX30102 sensor for a short session (30–120 seconds). The system detects finger presence, records heart rate and SpO2 data, sends it to a cloud backend, and displays live trends on a web dashboard that updates automatically every 3 seconds.

---

## Sampling Requirements

- Sensor sampling rate: ~100 Hz (raw PPG signal)
- Data upload rate: every ~2 seconds (aggregated data)
- Dashboard polling rate: every 3 seconds
