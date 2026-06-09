"""
Flask Server for Vibration Monitoring System
ESP32 + AI + Real-Time Dashboard
"""

from flask import Flask, render_template, jsonify, request
from flask_socketio import SocketIO, emit
from flask_cors import CORS
import numpy as np
import pandas as pd
from scipy.fft import rfft, rfftfreq
import joblib
from datetime import datetime
import os
from pathlib import Path
import traceback

BASE_DIR = Path(__file__).resolve().parent
MODEL_PATH = BASE_DIR / "motor_fault_model.pkl"

# -----------------------------
# Flask Setup
# -----------------------------
app = Flask(__name__)
app.config["SECRET_KEY"] = os.environ.get("SECRET_KEY", "vibration_monitor_secret")

CORS(app)
socketio = SocketIO(
    app,
    cors_allowed_origins="*",
    async_mode=os.environ.get("SOCKETIO_ASYNC_MODE", "threading"),
)

# -----------------------------
# Load Model
# -----------------------------
print("="*50)
print("VIBRATION MONITORING SERVER")
print("="*50)

if not MODEL_PATH.is_file():
    raise FileNotFoundError(f"motor_fault_model.pkl not found at {MODEL_PATH}")

model = joblib.load(MODEL_PATH)
print("Model loaded:", type(model).__name__)

prediction_history = []
MAX_HISTORY = 50


# -----------------------------
# Feature Extraction
# -----------------------------
def extract_features(accel_x, accel_y, accel_z, sample_rate=200):

    features = {}

    for axis_name, axis_data in [
        ('accel_x', accel_x),
        ('accel_y', accel_y),
        ('accel_z', accel_z)
    ]:

        axis_data = np.array(axis_data)

        if len(axis_data) < 2:
            raise ValueError("Not enough samples for FFT")

        # Time domain
        features[f'rms_{axis_name}'] = float(np.sqrt(np.mean(axis_data**2)))
        features[f'peak_{axis_name}'] = float(np.max(np.abs(axis_data)))
        features[f'kurtosis_{axis_name}'] = float(pd.Series(axis_data).kurtosis())

        # FFT
        fft_vals = rfft(axis_data)
        freqs = rfftfreq(len(axis_data), 1/sample_rate)

        magnitude = np.abs(fft_vals)

        dominant_idx = np.argmax(magnitude)

        features[f'dominant_freq_{axis_name}'] = float(freqs[dominant_idx])
        features[f'dominant_mag_{axis_name}'] = float(magnitude[dominant_idx])

        features[f'energy_0_50hz_{axis_name}'] = float(
            np.sum(magnitude[(freqs >= 0) & (freqs < 50)])
        )

        features[f'energy_50_100hz_{axis_name}'] = float(
            np.sum(magnitude[(freqs >= 50) & (freqs < 100)])
        )

    return features


# -----------------------------
# Routes
# -----------------------------
@app.route("/")
def index():
    return render_template("dashboard.html")


@app.route("/health")
def health():
    return jsonify({"status": "ok"}), 200


@app.route('/predict', methods=['POST'])
def predict():

    try:
        data = request.get_json()

        if not data:
            return jsonify({'error': 'No data received'}), 400

        accel_x = np.array(data.get('accel_x', []), dtype=float)
        accel_y = np.array(data.get('accel_y', []), dtype=float)
        accel_z = np.array(data.get('accel_z', []), dtype=float)

        if not (len(accel_x) == len(accel_y) == len(accel_z) == 512):
            return jsonify({'error': 'Expected 512 samples per axis'}), 400

        # Reject invalid constant data that may indicate a disconnected or dead sensor
        if np.allclose(accel_x, accel_x[0]) or np.allclose(accel_y, accel_y[0]) or np.allclose(accel_z, accel_z[0]):
            return jsonify({'error': 'Invalid sensor data: no variation detected'}), 400

        print("\nData Received:", datetime.now().strftime('%H:%M:%S'))

        # Extract Features
        features = extract_features(accel_x, accel_y, accel_z)

        # Maintain training feature order
        feature_df = pd.DataFrame([features])
        feature_df = feature_df.reindex(columns=model.feature_names_in_)

        # Predict
        prediction = model.predict(feature_df)[0]
        probabilities = model.predict_proba(feature_df)[0]

        confidence = float(np.max(probabilities))

        class_probabilities = {
            cls: float(probabilities[i])
            for i, cls in enumerate(model.classes_)
        }

        result = {
            'classification': prediction,
            'confidence': confidence,
            'probabilities': class_probabilities,
            'timestamp': datetime.now().isoformat(),
            'sample_count': 512
        }

        prediction_history.insert(0, result)
        if len(prediction_history) > MAX_HISTORY:
            prediction_history.pop()

        socketio.emit('prediction', result)

        print(f"Prediction: {prediction} ({confidence*100:.1f}%)")

        return jsonify(result), 200

    except Exception as e:
        print("\nERROR:", str(e))
        traceback.print_exc()
        return jsonify({'error': str(e)}), 500


@app.route('/history')
def get_history():
    return jsonify(prediction_history)


@socketio.on('connect')
def handle_connect():
    print("Dashboard Connected:", request.sid)
    emit('history', prediction_history)


@socketio.on('disconnect')
def handle_disconnect():
    print("Dashboard Disconnected:", request.sid)


# -----------------------------
# Run Server
# -----------------------------
if __name__ == "__main__":
    # Hugging Face Spaces use port 7860; local default is 5000
    default_port = 7860 if os.environ.get("SPACE_ID") else 5000
    port = int(os.environ.get("PORT", default_port))
    print("\nStarting Server...")
    print(f"Dashboard: http://localhost:{port}\n")

    socketio.run(app, host="0.0.0.0", port=port, debug=False)
