import pytest
from fastapi.testclient import TestClient
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "backend"))

from main import app, DB_FILE, init_db
import sqlite3

@pytest.fixture(autouse=True)
def setup_db():
    init_db()
    yield
    # clean up test rows after each test
    conn = sqlite3.connect(DB_FILE)
    conn.execute("DELETE FROM readings WHERE device_id = 'esp32_test'")
    conn.commit()
    conn.close()

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

VALID_HEADERS = {"Authorization": "Bearer test123"}
INVALID_HEADERS = {"Authorization": "Bearer wrongtoken"}


def test_root():
    response = client.get("/")
    assert response.status_code == 200
    assert response.json() == {"status": "ok"}


def test_post_valid_token():
    response = client.post("/data", json=TEST_PAYLOAD, headers=VALID_HEADERS)
    assert response.status_code == 200
    data = response.json()
    assert data["device_id"] == "esp32_test"
    assert data["session_id"] == "session_test"


def test_post_invalid_token():
    response = client.post("/data", json=TEST_PAYLOAD, headers=INVALID_HEADERS)
    assert response.status_code == 401
    assert response.json()["detail"] == "Unauthorized"


def test_post_missing_token():
    response = client.post("/data", json=TEST_PAYLOAD)
    assert response.status_code == 401


def test_post_missing_required_field():
    bad_payload = TEST_PAYLOAD.copy()
    del bad_payload["heart_rate"]
    response = client.post("/data", json=bad_payload, headers=VALID_HEADERS)
    assert response.status_code == 422


def test_latest_returns_data():
    client.post("/data", json=TEST_PAYLOAD, headers=VALID_HEADERS)
    response = client.get("/latest")
    assert response.status_code == 200
    data = response.json()
    assert "heart_rate" in data
    assert data["heart_rate"] == 75.0


def test_latest_has_session_id():
    client.post("/data", json=TEST_PAYLOAD, headers=VALID_HEADERS)
    response = client.get("/latest")
    assert response.status_code == 200
    assert response.json()["session_id"] == "session_test"


def test_history_returns_list():
    response = client.get("/history")
    assert response.status_code == 200
    data = response.json()
    assert "readings" in data
    assert isinstance(data["readings"], list)


def test_history_limit():
    for _ in range(5):
        client.post("/data", json=TEST_PAYLOAD, headers=VALID_HEADERS)
    response = client.get("/history?limit=3")
    assert response.status_code == 200
    assert len(response.json()["readings"]) <= 3