from fastapi import FastAPI, Header, HTTPException, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel
from typing import Optional
from datetime import datetime
from zoneinfo import ZoneInfo
import sqlite3
import logging
import os

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(levelname)s - %(message)s"
)

app = FastAPI()

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

API_TOKEN = os.getenv("API_TOKEN", "test123")
DB_FILE = "readings.db"
EST = ZoneInfo("America/New_York")


class SensorData(BaseModel):
    device_id: str
    session_id: str
    heart_rate: float
    hr_valid: bool
    spo2: Optional[float] = None
    spo2_valid: Optional[bool] = None
    ir: Optional[int] = None
    red: Optional[int] = None
    ratio: Optional[float] = None
    correlation: Optional[float] = None


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
    try:
        cur.execute("ALTER TABLE readings ADD COLUMN session_id TEXT")
        logging.info("Migrated: added session_id column")
    except Exception:
        pass
    conn.commit()
    conn.close()


@app.on_event("startup")
def startup():
    init_db()


def check_auth(authorization: str):
    if authorization != f"Bearer {API_TOKEN}":
        raise HTTPException(status_code=401, detail="Unauthorized")


@app.get("/")
def root():
    return {"status": "ok"}


@app.get("/dashboard")
def serve_dashboard():
    return FileResponse("dashboard.html")


@app.get("/api/latest")
def get_latest(authorization: str = Header(default="")):
    check_auth(authorization)
    conn = sqlite3.connect(DB_FILE)
    conn.row_factory = sqlite3.Row
    cur = conn.cursor()
    cur.execute("SELECT * FROM readings ORDER BY id DESC LIMIT 1")
    row = cur.fetchone()
    conn.close()
    if row is None:
        return {"message": "no data"}
    return dict(row)


@app.get("/api/history")
def get_history_api(limit: int = 50, authorization: str = Header(default="")):
    check_auth(authorization)
    limit = min(max(limit, 1), 200)
    conn = sqlite3.connect(DB_FILE)
    conn.row_factory = sqlite3.Row
    cur = conn.cursor()
    cur.execute("SELECT * FROM readings ORDER BY id DESC LIMIT ?", (limit,))
    rows = [dict(r) for r in cur.fetchall()]
    conn.close()
    return {"count": len(rows), "readings": rows}


@app.post("/data")
def receive_data(payload: SensorData, authorization: str = Header(default="")):
    if authorization != f"Bearer {API_TOKEN}":
        raise HTTPException(status_code=401, detail="Unauthorized")

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