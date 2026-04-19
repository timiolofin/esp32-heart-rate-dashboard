# Data Models

## Database

**Type:** SQLite  
**File:** `readings.db`  
**Managed by:** FastAPI backend on Render

---

## Table: `readings`

| Column | Type | Nullable | Description |
|---|---|---|---|
| `id` | INTEGER | NO | Auto-incremented primary key |
| `timestamp` | TEXT | NO | EST datetime string — format `YYYY-MM-DDTHH:MM:SS` |
| `device_id` | TEXT | NO | Identifier for the ESP32 device e.g. `esp32_001` |
| `session_id` | TEXT | YES | Identifier for the monitoring session e.g. `session_001` |
| `heart_rate` | REAL | YES | Computed BPM value |
| `hr_valid` | INTEGER | YES | 1 if heart rate signal is valid, 0 if not |
| `spo2` | REAL | YES | Oxygen saturation percentage |
| `spo2_valid` | INTEGER | YES | 1 if SpO2 signal is valid, 0 if not |
| `ir` | INTEGER | YES | Raw IR channel value from MAX30102 |
| `red` | INTEGER | YES | Raw red channel value from MAX30102 |
| `ratio` | REAL | YES | Red/IR ratio used in SpO2 calculation |
| `correlation` | REAL | YES | Pearson correlation of red and IR signals |

---

## Pydantic Model (API input validation)

```python
class SensorData(BaseModel):
    device_id: str
    session_id: str
    heart_rate: float
    hr_valid: bool
    spo2: Optional[float]
    spo2_valid: Optional[bool]
    ir: Optional[int]
    red: Optional[int]
    ratio: Optional[float]
    correlation: Optional[float]
```

`device_id`, `session_id`, `heart_rate`, and `hr_valid` are required.  
All other fields are optional and default to `None`.

---

## Sample Record

```json
{
  "id": 42,
  "timestamp": "2026-04-14T08:35:10",
  "device_id": "esp32_001",
  "session_id": "session_001",
  "heart_rate": 76.0,
  "hr_valid": 1,
  "spo2": 98.2,
  "spo2_valid": 1,
  "ir": 102400,
  "red": 97800,
  "ratio": 0.9800,
  "correlation": 0.9600
}
```
