from fastapi import FastAPI, Header, HTTPException
from fastapi.responses import FileResponse
from pathlib import Path
from pydantic import BaseModel
from typing import Optional
from datetime import datetime
import sqlite3

app = FastAPI()

API_TOKEN = "test123"
DB_FILE = "readings.db"


class SensorPayload(BaseModel):
    device_id: str
    bpm: int
    finger_detected: bool
    signal: Optional[int] = None


def get_connection():
    return sqlite3.connect(DB_FILE)


def init_db():
    conn = get_connection()
    cursor = conn.cursor()

    cursor.execute("""
        CREATE TABLE IF NOT EXISTS readings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id TEXT NOT NULL,
            bpm INTEGER NOT NULL,
            finger_detected INTEGER NOT NULL,
            signal INTEGER,
            timestamp TEXT NOT NULL
        )
    """)

    conn.commit()
    conn.close()


init_db()


@app.get("/")
def root():
    return {"status": "backend running"}


@app.get("/readings")
def get_readings():
    conn = get_connection()
    cursor = conn.cursor()

    cursor.execute("""
        SELECT device_id, bpm, finger_detected, signal, timestamp
        FROM readings
        ORDER BY id DESC
        LIMIT 100
    """)

    rows = cursor.fetchall()
    conn.close()

    readings = []
    for row in rows:
        readings.append({
            "device_id": row[0],
            "bpm": row[1],
            "finger_detected": bool(row[2]),
            "signal": row[3],
            "timestamp": row[4],
        })

    return {"count": len(readings), "readings": readings}


@app.get("/latest")
def get_latest():
    conn = get_connection()
    cursor = conn.cursor()

    cursor.execute("""
        SELECT device_id, bpm, finger_detected, signal, timestamp
        FROM readings
        ORDER BY id DESC
        LIMIT 1
    """)

    row = cursor.fetchone()
    conn.close()

    if not row:
        return {"message": "no data"}

    return {
        "device_id": row[0],
        "bpm": row[1],
        "finger_detected": bool(row[2]),
        "signal": row[3],
        "timestamp": row[4],
    }


@app.post("/data")
def receive_data(payload: SensorPayload, authorization: str | None = Header(default=None)):
    if authorization != f"Bearer {API_TOKEN}":
        raise HTTPException(status_code=401, detail="Unauthorized")

    timestamp = datetime.utcnow().isoformat()

    conn = get_connection()
    cursor = conn.cursor()

    cursor.execute("""
        INSERT INTO readings (device_id, bpm, finger_detected, signal, timestamp)
        VALUES (?, ?, ?, ?, ?)
    """, (
        payload.device_id,
        payload.bpm,
        1 if payload.finger_detected else 0,
        payload.signal,
        timestamp
    ))

    conn.commit()
    conn.close()

    return {
        "status": "ok",
        "stored": {
            "device_id": payload.device_id,
            "bpm": payload.bpm,
            "finger_detected": payload.finger_detected,
            "signal": payload.signal,
            "timestamp": timestamp,
        }
    }

@app.get("/dashboard")
def dashboard():
    return FileResponse(Path("dashboard.html"))