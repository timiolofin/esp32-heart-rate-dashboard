import pytest
from fastapi.testclient import TestClient
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "backend"))

from main import app, DB_FILE, init_db
import sqlite3

import main

@pytest.fixture(autouse=True)
def setup_db(tmp_path, monkeypatch):
    test_db = str(tmp_path / "test_readings.db")
    monkeypatch.setattr(main, "DB_FILE", test_db)
    main.init_db()
    yield

client = TestClient(app)

TEST_PAYLOAD = {
    "device_id": "esp32_test",
    "session_id": "session_test",
    "heart_rate": 75.0,
    "hr_valid": True,
    "spo2": 98.2,
    "spo2_valid": True,
    "ir": 100000,
    "red": 95000,
    "ratio": 0.98,
    "correlation": 0.96
}

VALID_HEADERS   = {"Authorization": "Bearer test123"}
INVALID_HEADERS = {"Authorization": "Bearer wrongtoken"}


# ── REQ: backend validates token ─────────────────────────────────────────────

def test_post_valid_token():
    """Sensor data accepted when correct token is provided."""
    response = client.post("/data", json=TEST_PAYLOAD, headers=VALID_HEADERS)
    assert response.status_code == 200
    data = response.json()
    assert data["device_id"] == "esp32_test"
    assert data["session_id"] == "session_test"

def test_post_invalid_token():
    """Request rejected with 401 when wrong token is provided."""
    response = client.post("/data", json=TEST_PAYLOAD, headers=INVALID_HEADERS)
    assert response.status_code == 401
    assert response.json()["detail"] == "Unauthorized"

def test_post_missing_token():
    """Request rejected with 401 when no token is provided."""
    response = client.post("/data", json=TEST_PAYLOAD)
    assert response.status_code == 401

def test_get_latest_requires_token():
    """GET /latest is protected — rejected without valid token."""
    response = client.get("/latest", headers=INVALID_HEADERS)
    assert response.status_code == 401

def test_get_history_requires_token():
    """GET /history is protected — rejected without valid token."""
    response = client.get("/history", headers=INVALID_HEADERS)
    assert response.status_code == 401


# ── REQ: backend stores readings with timestamps ──────────────────────────────

def test_post_stores_reading_in_db():
    """Verify sensor payload is actually written to the database."""
    client.post("/data", json=TEST_PAYLOAD, headers=VALID_HEADERS)
    conn = sqlite3.connect(main.DB_FILE)
    conn.row_factory = sqlite3.Row
    row = conn.execute(
        "SELECT * FROM readings WHERE device_id = 'esp32_test' ORDER BY id DESC LIMIT 1"
    ).fetchone()
    conn.close()
    assert row is not None
    assert row["heart_rate"] == 75.0
    assert row["session_id"] == "session_test"
    assert row["timestamp"] is not None

def test_post_missing_required_field():
    """Payload missing heart_rate is rejected with 422."""
    bad_payload = TEST_PAYLOAD.copy()
    del bad_payload["heart_rate"]
    response = client.post("/data", json=bad_payload, headers=VALID_HEADERS)
    assert response.status_code == 422


# ── REQ: dashboard displays heart rate trends ─────────────────────────────────

def test_latest_returns_most_recent_reading():
    """GET /latest returns the most recently stored reading."""
    client.post("/data", json=TEST_PAYLOAD, headers=VALID_HEADERS)
    response = client.get("/latest", headers=VALID_HEADERS)
    assert response.status_code == 200
    data = response.json()
    assert data["heart_rate"] == 75.0
    assert data["spo2"] == 98.2

def test_latest_has_session_id():
    """GET /latest includes session_id in response."""
    client.post("/data", json=TEST_PAYLOAD, headers=VALID_HEADERS)
    response = client.get("/latest", headers=VALID_HEADERS)
    assert response.json()["session_id"] == "session_test"

def test_latest_empty_db():
    """GET /latest returns no-data message when DB is empty."""
    conn = sqlite3.connect(DB_FILE)
    conn.execute("DELETE FROM readings")
    conn.commit()
    conn.close()
    response = client.get("/latest", headers=VALID_HEADERS)
    assert response.status_code == 200
    assert response.json().get("message") == "no data"

def test_history_returns_list():
    """GET /history returns a list of readings."""
    response = client.get("/history", headers=VALID_HEADERS)
    assert response.status_code == 200
    data = response.json()
    assert "readings" in data
    assert isinstance(data["readings"], list)

def test_history_limit():
    """GET /history respects the limit query parameter."""
    for _ in range(5):
        client.post("/data", json=TEST_PAYLOAD, headers=VALID_HEADERS)
    response = client.get("/history?limit=3", headers=VALID_HEADERS)
    assert response.status_code == 200
    assert len(response.json()["readings"]) <= 3

def test_history_limit_capped():
    """GET /history limit cannot exceed 200."""
    response = client.get("/history?limit=9999", headers=VALID_HEADERS)
    assert response.status_code == 200
    assert len(response.json()["readings"]) <= 200


# ── REQ: multiple devices using device_id ────────────────────────────────────

def test_multiple_device_ids_stored():
    """Two devices with different device_ids can both store data."""
    payload2 = {**TEST_PAYLOAD, "device_id": "esp32_002", "session_id": "session_002"}
    client.post("/data", json=TEST_PAYLOAD, headers=VALID_HEADERS)
    client.post("/data", json=payload2,     headers=VALID_HEADERS)

    conn = sqlite3.connect(main.DB_FILE)
    ids = [r[0] for r in conn.execute(
        "SELECT device_id FROM readings WHERE device_id IN ('esp32_test','esp32_002')"
    ).fetchall()]
    conn.close()
    assert "esp32_test" in ids
    assert "esp32_002"  in ids


# ── REQ: logging ──────────────────────────────────────────────────────────────

def test_root_endpoint():
    """Backend health check returns ok."""
    response = client.get("/")
    assert response.status_code == 200
    assert response.json() == {"status": "ok"}