from fastapi import FastAPI, Header, HTTPException
from pydantic import BaseModel
from typing import Optional
from datetime import datetime

app = FastAPI()

API_TOKEN = "test123"
latest_reading = None


class SensorData(BaseModel):
    device_id: str
    heart_rate: float
    hr_valid: bool
    spo2: Optional[float] = None
    spo2_valid: Optional[bool] = None
    ir: Optional[int] = None
    red: Optional[int] = None
    ratio: Optional[float] = None
    correlation: Optional[float] = None


@app.get("/")
def root():
    return {"status": "ok"}


@app.get("/latest")
def get_latest():
    if latest_reading is None:
        return {"message": "no data"}
    return latest_reading


@app.post("/data")
def receive_data(payload: SensorData, authorization: str = Header(default="")):
    global latest_reading

    if authorization != f"Bearer {API_TOKEN}":
        raise HTTPException(status_code=401, detail="Unauthorized")

    latest_reading = payload.model_dump()
    latest_reading["timestamp"] = datetime.now().isoformat()

    print("Received payload:", latest_reading)
    return {"message": "Data received", "device_id": payload.device_id}