---
title: Smart Motor Health Monitor
emoji: ⚙️
colorFrom: blue
colorTo: purple
sdk: docker
app_port: 7860
pinned: false
license: mit
---

# Smart Motor Health Monitor — Server

Flask + Socket.IO server for ESP32 vibration data and real-time fault classification.

## Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Web dashboard |
| `/predict` | POST | ESP32 sends 512 samples per axis (JSON) |
| `/history` | GET | Last 50 predictions |
| `/health` | GET | Health check (use for Koyeb/Render) |

## Hugging Face Spaces (current ESP32 setup)

| Use | URL |
|-----|-----|
| Dashboard | https://afnan2155-motor-health-monitor.hf.space/ |
| ESP32 API | https://afnan2155-motor-health-monitor.hf.space/predict |
| Health | https://afnan2155-motor-health-monitor.hf.space/health |

The ESP32 sketch calls `/health` first to wake the Space, then posts to `/predict` with retries on 500/502/503.

**Keep Space awake:** use [cron-job.org](https://cron-job.org) to ping `/health` every 5 minutes (free, no card).

**After updating `app.py`:** push changes to your Hugging Face Space so server fixes take effect.

## Deploy on Koyeb (requires credit card)

Koyeb is more reliable than Hugging Face Spaces for this project: no cold-start sleep, consistent predictions, and HTTPS for the ESP32.

### 1. Push code to GitHub

Make sure these files are committed and pushed:

- `app.py`
- `Dockerfile`
- `requirements.txt`
- `motor_fault_model.pkl`
- `templates/` and `static/`

Repo: https://github.com/AK-programming/Smart_Motor_Monitoring

### 2. Create Koyeb service

1. Go to [koyeb.com](https://www.koyeb.com) and sign in with GitHub.
2. **Create Service** → **Web Service** → **GitHub**.
3. Select repository: `AK-programming/Smart_Motor_Monitoring`.
4. Branch: `main`.
5. **Work directory**: `vibration_project/python_server`
6. **Builder**: Docker
7. **Instance**: Free (Nano)
8. **Ports**: `8000` (HTTP)
9. **Health check path**: `/health`
10. **Environment variables**:
    - `PORT` = `8000`
    - `SOCKETIO_ASYNC_MODE` = `eventlet`
11. Click **Deploy**.

### 3. After deploy

Your app URL will look like:

```
https://motor-health-monitor-<your-name>.koyeb.app
```

| Use | URL |
|-----|-----|
| Dashboard | `https://YOUR-APP.koyeb.app/` |
| ESP32 API | `https://YOUR-APP.koyeb.app/predict` |
| Health | `https://YOUR-APP.koyeb.app/health` |

### 4. Update ESP32

In `esp32_vibration_wifi.ino`:

```cpp
const char* SERVER_URL = "https://YOUR-APP.koyeb.app/predict";
```

Use **HTTPS** (required for cloud). Only one POST to `/predict` is needed — the dashboard updates automatically via Socket.IO.

## Deploy on Render (alternative)

The repo includes `render.yaml` at the project root. Connect the repo in Render → **New Blueprint**. Free tier sleeps after ~15 minutes of inactivity (first ESP32 request may be slow).

## Local run

```powershell
cd vibration_project\python_server
pip install -r requirements.txt
python app.py
```

Open http://localhost:5000

## Predict payload

```json
{
  "accel_x": [0.01, 0.02, ...],
  "accel_y": [0.01, 0.02, ...],
  "accel_z": [0.01, 0.02, ...]
}
```

Each array must contain exactly **512** float values.
