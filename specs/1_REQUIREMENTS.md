# System Requirements

## Overview

The system is a cloud-connected heart rate monitoring platform that collects physiological data from a MAX30102 sensor using an ESP32 device and displays it on a web dashboard.

---

## Functional Requirements

- The system must read heart rate data from the MAX30102 sensor
- The ESP32 must process raw PPG signals to compute BPM
- The ESP32 must send data to a backend server over Wi-Fi using HTTP POST
- The backend must validate incoming requests using a token
- The backend must store readings in a database with timestamps
- The web dashboard must display heart rate trends over time
- The system must support multiple devices using a device_id field
- The system must support session-based data collection using a session_id field

---

## Non-Functional Requirements

- Data transmission must be secure (token-based authentication)
- The system must handle data uploads every ~5 seconds
- The system must support multiple concurrent devices
- The backend must log all incoming requests
- The system must be deployable using Docker

---

## Use Case

A user places their finger on the MAX30102 sensor for a short session (30–60 seconds). The system records heart rate data, sends it to a backend server, and displays trends on a web dashboard.

---

## Sampling Requirements

- Sensor sampling rate: ~100 Hz (raw PPG signal)
- Data upload rate: every ~5 seconds (aggregated data)