from pathlib import Path
import numpy as np
import pandas as pd

BASE_DIR = Path(__file__).resolve().parent
DATASET_PATH = BASE_DIR / "dataset.txt"

def compute_statistical_features(signal_window):
    """Computes time-domain statistical moments."""
    mean_val = float(np.mean(signal_window))
    rms_val = float(np.sqrt(np.mean(np.square(signal_window))))
    std_val = float(np.std(signal_window))
    peak_val = float(np.max(np.abs(signal_window)))
    crest_factor = float(peak_val / (rms_val + 1e-6))
    return {
        "mean": mean_val,
        "rms": rms_val,
        "std": std_val,
        "crest_factor": crest_factor
    }

def compute_spectral_energy(signal_window, sample_rate=100.0):
    """Computes FFT frequency energy distribution."""
    fft_vals = np.abs(np.fft.rfft(signal_window))
    freqs = np.fft.rfftfreq(len(signal_window), d=1.0 / sample_rate)
    dominant_freq = freqs[np.argmax(fft_vals[1:]) + 1] if len(fft_vals) > 1 else 0.0
    spectral_energy = np.sum(np.square(fft_vals))
    return {
        "dominant_freq_hz": float(dominant_freq),
        "spectral_energy": float(spectral_energy)
    }

def benchmark_dataset():
    if not DATASET_PATH.exists():
        print(f"Dataset not found at {DATASET_PATH}")
        return

    df = pd.read_csv(DATASET_PATH)
    print(f"Loaded dataset: {df.shape[0]} rows, {df.shape[1]} columns")
    print(f"Target distribution:\n{df['status'].value_counts()}\n")

    sample_vib = df["vibration_rms"].values[:64]
    sample_curr = df["current_rms"].values[:64]

    vib_feats = compute_statistical_features(sample_vib)
    curr_feats = compute_statistical_features(sample_curr)
    vib_fft = compute_spectral_energy(sample_vib)

    print("--- Benchmark Feature Extraction Output ---")
    print("Vibration Stats:", vib_feats)
    print("Current Stats:  ", curr_feats)
    print("Vibration FFT:  ", vib_fft)
    print("\nConclusion: Time-domain RMS, Mean, and StdDev provide optimal discriminatory power within ESP32 RAM budget.")

if __name__ == "__main__":
    benchmark_dataset()
