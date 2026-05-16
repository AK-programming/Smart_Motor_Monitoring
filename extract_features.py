import numpy as np
import pandas as pd
from scipy.fft import fft, fftfreq


# ================================
# FEATURE EXTRACTION FUNCTION
# ================================
def extract_features(data, sample_rate=200):

    features = {}

    for axis in ['accel_x', 'accel_y', 'accel_z']:

        if axis not in data.columns:
            print(f"Warning: Column '{axis}' not found.")
            continue

        signal_data = data[axis].values

        # ---------- TIME DOMAIN ----------
        features[f'rms_{axis}'] = np.sqrt(np.mean(signal_data ** 2))
        features[f'peak_{axis}'] = np.max(np.abs(signal_data))
        features[f'kurtosis_{axis}'] = pd.Series(signal_data).kurtosis()

        # ---------- FREQUENCY DOMAIN ----------
        fft_vals = np.array(fft(signal_data))
        freqs = fftfreq(len(signal_data), 1 / sample_rate)

        positive_freqs = freqs[:len(freqs)//2]
        magnitude = np.abs(fft_vals[:len(fft_vals)//2])

        if len(magnitude) > 0:
            dominant_idx = np.argmax(magnitude)
            features[f'dominant_freq_{axis}'] = positive_freqs[dominant_idx]
            features[f'dominant_mag_{axis}'] = magnitude[dominant_idx]
        else:
            features[f'dominant_freq_{axis}'] = 0
            features[f'dominant_mag_{axis}'] = 0

        # ---------- BAND ENERGY ----------
        features[f'energy_0_50hz_{axis}'] = np.sum(
            magnitude[(positive_freqs >= 0) & (positive_freqs < 50)]
        )

        features[f'energy_50_100hz_{axis}'] = np.sum(
            magnitude[(positive_freqs >= 50) & (positive_freqs < 100)]
        )

    return features


# ================================
# MAIN DATA PROCESSING
# ================================
print("Processing vibration datasets...")

datasets = []

# IMPORTANT: Match your CSV filenames exactly
labels = ['normal', 'Imbalance', 'loose']

window_size = 512
hop_size = window_size // 2
sample_rate = 200

for label in labels:

    filename = f"{label}.csv"

    print(f"\nReading file: {filename}")

    try:
        data = pd.read_csv(filename)
    except FileNotFoundError:
        print(f"File not found: {filename}")
        continue

    if len(data) < window_size:
        print("Not enough data for windowing.")
        continue

    num_windows = (len(data) - window_size) // hop_size
    print(f"Creating approximately {num_windows} windows...")

    for i in range(0, len(data) - window_size, hop_size):

        window = data.iloc[i:i+window_size]

        features = extract_features(window, sample_rate)
        features['label'] = label

        datasets.append(features)


# ================================
# SAVE FEATURES
# ================================
print("\nCreating feature dataset...")

feature_df = pd.DataFrame(datasets)

if len(feature_df) == 0:
    print("No features extracted. Check your data.")
else:
    feature_df.to_csv('features.csv', index=False)

    print("\nDone!")
    print(f"Samples created: {len(feature_df)}")
    print(f"Features per sample: {len(feature_df.columns)-1}")
    print("Output file: features.csv")
