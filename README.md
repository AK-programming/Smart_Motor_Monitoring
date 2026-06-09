# Smart Motor Monitoring System

This project monitors motor vibration using an ESP32-S3 and an MPU6050 sensor, sends data to a Python Flask server for AI-based fault classification, and displays results on a real-time web dashboard.

## What This Project Does

1. The ESP32-S3 collects 512 samples for each acceleration axis at 200 Hz.
2. The board sends the samples to the Flask server via WiFi (`/predict`).
3. The server extracts vibration features and uses a trained model to classify the motor state.
4. The dashboard updates in real time via Socket.IO and stores the latest 50 predictions in memory.

## Project Structure (Key Files)

- `vibration_project/python_server/app.py` - Flask + Socket.IO server.
- `vibration_project/python_server/requirements.txt` - Python dependencies.
- `vibration_project/python_server/motor_fault_model.pkl` - Trained ML model used by the server.
- `vibration_project/python_server/templates/dashboard.html` - Web dashboard UI.
- `vibration_project/Arduino_code/esp32_vibration_wifi.ino` - ESP32 firmware.
- `extract_features.py` - Builds `features.csv` from raw CSV vibration data.
- `train_model.py` - Trains the model and saves `motor_fault_model.pkl`.
- `normal.csv`, `Imbalance.csv`, `loose.csv` - Example datasets.

## Quick Start (Run the Server)

1. Open a terminal in the project root.
2. Create and activate a virtual environment (optional but recommended).

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
```

3. Install dependencies.

```powershell
cd vibration_project\python_server
pip install -r requirements.txt
```

4. Start the server.

```powershell
python app.py
```

5. Open the dashboard in your browser.

```
http://localhost:5000
```

The server listens on `0.0.0.0:5000`, so other devices on the same network can reach it too.

## API Endpoints

- `GET /` serves the dashboard UI.
- `POST /predict` expects JSON with exactly 512 samples per axis.
- `GET /history` returns the last 50 predictions held in memory.
- `GET /health` returns server status (for cloud health checks).

## Deploy to Cloud (Koyeb / Render)

Hugging Face Spaces often sleep when idle, which causes timeouts and inconsistent predictions from the ESP32. For a more stable setup, deploy the server to **Koyeb** (recommended) or **Render**.

See step-by-step instructions in `vibration_project/python_server/README.md`.

After deployment, set the ESP32 `SERVER_URL` to your cloud HTTPS URL:

```cpp
const char* SERVER_URL = "https://your-app.koyeb.app/predict";
```

Example payload shape for `POST /predict`:

```json
{
  "accel_x": [0.01, 0.02, 0.03, ...],
  "accel_y": [0.01, 0.02, 0.03, ...],
  "accel_z": [0.01, 0.02, 0.03, ...]
}
```

## Run the ESP32 (Send Live Data)

1. Open `vibration_project/Arduino_code/esp32_vibration_wifi.ino` in Arduino IDE.
2. Install required libraries: `MPU6050`, `Wire`, `WiFi`, `HTTPClient`.
3. Update `WIFI_SSID`, `WIFI_PASSWORD`, and `SERVER_URL` in the sketch.
4. Find your PC IP with `ipconfig` and set `SERVER_URL` like `http://<YOUR_PC_IP>:5000/predict`.
5. Select the correct board and port (ESP32-S3 Dev Module).
6. Upload the sketch.
7. Open Serial Monitor at `115200` baud to confirm WiFi connection.

Once connected, the board will send data windows to the server and the dashboard will update live.

## Train a New Model (Optional)

If you want to retrain the model using the CSV datasets:

1. Generate features from raw data.

```powershell
python extract_features.py
```

2. Train the model.

```powershell
python train_model.py
```

3. Copy the newly created `motor_fault_model.pkl` to `vibration_project/python_server/motor_fault_model.pkl`.

Then restart the server.

## Troubleshooting

- `motor_fault_model.pkl not found`:
  Ensure the file exists in `vibration_project/python_server` and you start the server from that folder.

- `Expected 512 samples per axis`:
  The ESP32 must send exactly 512 samples per axis. Keep `WINDOW_SIZE = 512` in the Arduino sketch.

- `ModuleNotFoundError`:
  Make sure you installed dependencies from `requirements.txt`.

- Dashboard does not update:
  Check that the ESP32 can reach the server IP and that the server is running on port 5000.

## Notes

- Prediction history is stored in memory and resets when the server restarts.
- The dashboard uses Socket.IO for real-time updates.
