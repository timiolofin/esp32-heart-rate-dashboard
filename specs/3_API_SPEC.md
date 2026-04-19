# API Specification

## Base URL

```
https://esp32-heart-rate-dashboard.onrender.com
```

---

## Authentication

All endpoints except `GET /` require a Bearer token in the `Authorization` header.

```
Authorization: Bearer test123
```

Requests with missing or invalid tokens return:

```json
{ "detail": "Unauthorized" }
```
with HTTP status `401`.

---

## Endpoints

### GET /

Health check. No auth required.

**Response**
```json
{ "status": "ok" }
```

---

### POST /data

Receive a sensor reading from the ESP32. Used exclusively by the firmware.

**Headers**
```
Authorization: Bearer test123
Content-Type: application/json
```

**Request body**
```json
{
  "device_id": "esp32_001",
  "session_id": "session_1713556200000",
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

`device_id`, `session_id`, `heart_rate`, `hr_valid` are required.  
All other fields are optional.

**Success response — 200**
```json
{
  "message": "Data received",
  "device_id": "esp32_001",
  "session_id": "session_1713556200000"
}
```

**Error responses**
| Status | Reason |
|---|---|
| 401 | Missing or invalid token |
| 422 | Missing required field or wrong data type |

---

### GET /latest

Returns the most recent reading stored in the database. Used by the frontend dashboard.

**Headers**
```
Authorization: Bearer test123
```

**Success response — 200**
```json
{
  "id": 42,
  "timestamp": "2026-04-14T08:35:10",
  "device_id": "esp32_001",
  "session_id": "session_1713556200000",
  "heart_rate": 76.0,
  "hr_valid": 1,
  "spo2": 98.2,
  "spo2_valid": 1,
  "ir": 102400,
  "red": 97800,
  "ratio": 0.98,
  "correlation": 0.96
}
```

**Empty DB response**
```json
{ "message": "no data" }
```

---

### GET /history?limit=50

Returns the most recent N readings in descending order. Used by the frontend for trend charts.

**Headers**
```
Authorization: Bearer test123
```

**Query parameters**
| Param | Type | Default | Max | Description |
|---|---|---|---|---|
| `limit` | integer | 50 | 200 | Number of readings to return |

**Success response — 200**
```json
{
  "count": 2,
  "readings": [
    { "id": 43, "timestamp": "...", "heart_rate": 77.0, "...": "..." },
    { "id": 42, "timestamp": "...", "heart_rate": 76.0, "...": "..." }
  ]
}
```

---

## ESP32 to Backend Communication

The firmware sends HTTP POST requests every 5 seconds when a finger is detected and a valid reading is available. Communication is over HTTPS to the Render-hosted backend. The Bearer token is hardcoded in `http_helper.h` and injected into every request header. If no valid reading has been produced yet (algorithms still warming up), the POST is skipped for that cycle.

## Frontend to Backend Communication

The dashboard (`dashboard.html`) polls `/latest` and `/history` every 3 seconds using the browser `fetch()` API with the Bearer token in the request header. No server is needed to run the frontend — it is a static HTML file that can be opened directly in a browser or hosted on GitHub Pages.
