# Test Specification

## Overview

Testing is focused on the backend API, which is where authentication, data validation, and database logic live. The hardware layer was validated through live sensor testing and serial monitor output. The frontend was validated through integration testing with real API responses.

---

## Test Stack

| Tool | Purpose |
|---|---|
| `pytest` | Test runner |
| `FastAPI TestClient` | Simulates HTTP requests to the API without a live server |
| `monkeypatch` + `tmp_path` | Injects a temporary SQLite DB for each test — production data is never touched |

---

## Test File

`backend/test_main.py` — 15 unit tests

---

## Test Categories

### Authentication (5 tests)

| Test | What it verifies |
|---|---|
| `test_post_valid_token` | Valid token accepted, data stored |
| `test_post_invalid_token` | Wrong token returns 401 |
| `test_post_missing_token` | No token returns 401 |
| `test_get_latest_requires_token` | GET /latest rejects invalid token |
| `test_get_history_requires_token` | GET /history rejects invalid token |

### Data Validation (2 tests)

| Test | What it verifies |
|---|---|
| `test_post_missing_required_field` | Missing `heart_rate` returns 422 |
| `test_post_valid_token` | Valid complete payload accepted with 200 |

### Database Writes (2 tests)

| Test | What it verifies |
|---|---|
| `test_post_stores_reading_in_db` | Row is actually written to DB with correct values and timestamp |
| `test_multiple_device_ids_stored` | Two different device_ids both stored correctly |

### Data Retrieval (5 tests)

| Test | What it verifies |
|---|---|
| `test_latest_returns_most_recent_reading` | /latest returns correct HR and SpO2 |
| `test_latest_has_session_id` | session_id is returned in /latest response |
| `test_latest_empty_db` | Empty DB returns `{"message": "no data"}` |
| `test_history_returns_list` | /history returns a list of readings |
| `test_history_limit` | limit param respected |

### Edge Cases (2 tests)

| Test | What it verifies |
|---|---|
| `test_history_limit_capped` | limit=9999 is capped at 200 |
| `test_root_endpoint` | Health check returns `{"status": "ok"}` |

---

## Running the Tests

```bash
cd backend
source venv/bin/activate
pytest test_main.py -v
```

Expected output: **15 passed**

---

## Hardware Validation

The ESP32 firmware was validated through:
- Serial monitor output confirming finger detection, 5s stabilization countdown, buffering progress, and BPM/SpO2 output
- Confirmed finger removal triggers clean state reset and "Finger removed. Waiting..." message
- Confirmed new session ID is generated after 60s of no finger placement
- Live HTTP POST confirmations printed to serial (`HTTP code: 200`)
- Visual confirmation on the dashboard that readings update in real time with correct session ID

## Frontend Validation

The dashboard was validated through:
- Integration testing with live backend responses
- Verified BPM, SpO2, session ID, and timestamp all populate correctly
- Verified finger detection status indicator updates within 10 seconds of removal
- Light and dark mode tested manually in browser
