# =============================================================================
# VitalSense — Backend API
# FastAPI server that receives sensor readings from the ESP32, stores them
# in a SQLite database, and exposes endpoints for the frontend dashboard.
# Deployed on Render: https://esp32-heart-rate-dashboard.onrender.com
# =============================================================================

from fastapi import FastAPI, Header, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from typing import Optional
from datetime import datetime
from zoneinfo import ZoneInfo
import sqlite3
import logging
import os

# Configure structured logging — all incoming requests are logged with timestamp
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(levelname)s - %(message)s"
)

app = FastAPI()

# Allow cross-origin requests so the frontend dashboard can call this API
# from any domain (including GitHub Pages and local file system)
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# API token loaded from environment variable — falls back to "test123" for development
# On Render, set API_TOKEN as an environment variable for production use
API_TOKEN = os.getenv("API_TOKEN", "test123")

# SQLite database file — stored alongside main.py on the Render server
DB_FILE = "readings.db"

# All timestamps stored in Eastern Time
EST = ZoneInfo("America/New_York")


# =============================================================================
# Data Model
# Pydantic validates every incoming POST request against this schema.
# Required: device_id, session_id, heart_rate, hr_valid
# Optional: spo2, spo2_valid, ir, red, ratio, correlation
# =============================================================================
class SensorData(BaseModel):
    device_id: str          # Unique identifier for the ESP32 device
    session_id: str         # Identifier for the current monitoring session
    heart_rate: float       # Computed BPM value from the RF algorithm
    hr_valid: bool          # True if the heart rate signal passed quality check
    spo2: Optional[float] = None          # Oxygen saturation percentage
    spo2_valid: Optional[bool] = None     # True if SpO2 signal is valid
    ir: Optional[int] = None              # Raw IR channel value from MAX30102
    red: Optional[int] = None             # Raw red channel value from MAX30102
    ratio: Optional[float] = None         # Red/IR ratio used in SpO2 calculation
    correlation: Optional[float] = None   # Pearson correlation of red and IR signals


# =============================================================================
# Database Initialization
# Creates the readings table if it doesn't exist.
# The ALTER TABLE migration adds session_id to existing databases without
# dropping any data — safe to run on every startup.
# =============================================================================
def init_db():
    conn = sqlite3.connect(DB_FILE)
    cur = conn.cursor()
    cur.execute("""
        CREATE TABLE IF NOT EXISTS readings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT NOT NULL,
            device_id TEXT NOT NULL,
            session_id TEXT,
            heart_rate REAL,
            hr_valid INTEGER,
            spo2 REAL,
            spo2_valid INTEGER,
            ir INTEGER,
            red INTEGER,
            ratio REAL,
            correlation REAL
        )
    """)
    # Migration: add session_id column if upgrading from an older schema
    try:
        cur.execute("ALTER TABLE readings ADD COLUMN session_id TEXT")
        logging.info("Migrated: added session_id column")
    except Exception:
        pass  # Column already exists — safe to ignore
    conn.commit()
    conn.close()


@app.on_event("startup")
def startup():
    """Initialize the database when the server starts."""
    init_db()


# =============================================================================
# Auth Helper
# All endpoints except GET / require a valid Bearer token.
# Raises 401 Unauthorized if the token is missing or incorrect.
# =============================================================================
def check_auth(authorization: str):
    if authorization != f"Bearer {API_TOKEN}":
        raise HTTPException(status_code=401, detail="Unauthorized")


# =============================================================================
# Endpoints
# =============================================================================

@app.get("/")
def root():
    """Health check — confirms the backend is running."""
    return {"status": "ok"}


@app.get("/latest")
def get_latest(authorization: str = Header(default="")):
    """
    Returns the most recent sensor reading from the database.
    Used by the frontend dashboard to display live BPM and SpO2.
    Returns {"message": "no data"} if the database is empty.
    """
    check_auth(authorization)
    conn = sqlite3.connect(DB_FILE)
    conn.row_factory = sqlite3.Row  # Return rows as dicts
    cur = conn.cursor()
    cur.execute("SELECT * FROM readings ORDER BY id DESC LIMIT 1")
    row = cur.fetchone()
    conn.close()

    if row is None:
        return {"message": "no data"}

    return dict(row)


@app.get("/history")
def get_history(limit: int = 50, authorization: str = Header(default="")):
    """
    Returns the last N readings in descending order (newest first).
    Used by the frontend to render trend charts.
    limit is capped between 1 and 200 to prevent abuse.
    """
    check_auth(authorization)
    limit = min(max(limit, 1), 200)  # Clamp limit to safe range
    conn = sqlite3.connect(DB_FILE)
    conn.row_factory = sqlite3.Row
    cur = conn.cursor()
    cur.execute("SELECT * FROM readings ORDER BY id DESC LIMIT ?", (limit,))
    rows = [dict(r) for r in cur.fetchall()]
    conn.close()
    return {"count": len(rows), "readings": rows}


@app.post("/data")
def receive_data(payload: SensorData, authorization: str = Header(default="")):
    """
    Receives a sensor reading from the ESP32 and stores it in the database.
    Called by the firmware every ~2 seconds when a finger is detected.
    Validates the Bearer token before accepting any data.
    """
    if authorization != f"Bearer {API_TOKEN}":
        raise HTTPException(status_code=401, detail="Unauthorized")

    # Stamp the reading with the current Eastern Time
    timestamp = datetime.now(EST).strftime("%Y-%m-%dT%H:%M:%S")
    logging.info(f"Received payload: {payload}")

    conn = sqlite3.connect(DB_FILE)
    cur = conn.cursor()
    cur.execute("""
        INSERT INTO readings (
            timestamp, device_id, session_id, heart_rate, hr_valid,
            spo2, spo2_valid, ir, red, ratio, correlation
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    """, (
        timestamp,
        payload.device_id,
        payload.session_id,
        payload.heart_rate,
        int(payload.hr_valid),
        payload.spo2,
        int(payload.spo2_valid) if payload.spo2_valid is not None else None,
        payload.ir,
        payload.red,
        payload.ratio,
        payload.correlation
    ))
    conn.commit()
    conn.close()

    return {"message": "Data received", "device_id": payload.device_id, "session_id": payload.session_id}