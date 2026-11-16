"""Generate labeled simulation scenarios, not independent motor measurements.

The ranges below are assumptions for exercising the existing feature contract.
They do not estimate this motor's thermal/electrical/mechanical dynamics.
"""
from pathlib import Path
import argparse

import numpy as np
import pandas as pd

SCENARIO_CLASSES = ["NORMAL", "OVERCURRENT", "OVERHEAT", "STOPPED", "UNBALANCE"]
RECIPE = "motor_rule_scenarios_v1"
SAMPLES_PER_CLASS = 128
SEED = 42
MAX_SYNTHETIC_WEIGHT_RATIO = 0.10


def generate_scenarios(real_training, samples_per_class=SAMPLES_PER_CLASS, seed=SEED):
    """Use only the supplied training partition to choose baseline magnitudes."""
    if samples_per_class < 1:
        raise ValueError("samples_per_class must be positive")
    healthy = real_training.loc[real_training.status == "NORMAL"]
    if healthy.empty:
        raise ValueError("Synthetic scenarios require real NORMAL training examples")
    rng = np.random.default_rng(seed)
    gravity_mean = float(healthy.vib_mean.median())
    running = healthy.loc[healthy.rpm >= 250, "rpm"]
    rpm_mean = float(running.median()) if not running.empty else 1300.0
    rows = []
    for label in SCENARIO_CLASSES:
        for index in range(samples_per_class):
            # Only single-fault cases: do not invent labels for simultaneous faults.
            vib_mean = float(np.clip(gravity_mean * rng.uniform(0.98, 1.02), 8.5, 10.5))
            vib_std = rng.uniform(0.04, 0.20)
            current_mean = rng.uniform(0.05, 0.14)
            current_std = rng.uniform(0.01, 0.05)
            rpm = float(np.clip(rpm_mean * rng.uniform(0.85, 1.15), 250, 2500))
            temp = rng.uniform(25, 45)
            if label == "OVERHEAT":
                temp = rng.uniform(60, 90)
            elif label == "OVERCURRENT":
                current_mean = rng.uniform(0.27, 0.65)
                current_std = rng.uniform(0.02, 0.10)
            elif label == "UNBALANCE":
                vib_std = rng.uniform(0.50, 3.0)
            elif label == "STOPPED":
                rpm = 0.0
                current_mean = rng.uniform(0, 0.025)
                current_std = min(rng.uniform(0, 0.025), current_mean)
                vib_std = rng.uniform(0.02, 0.08)
                temp = rng.uniform(25, 40)
            # Moment consistency: RMS^2 = mean^2 + population variance. These
            # are magnitude/absolute-current statistics, matching firmware units.
            rows.append({
                "time_ms": index * 1500,
                "vib_rms": float(np.hypot(vib_mean, vib_std)),
                "vib_mean": vib_mean, "vib_std": vib_std,
                "current_rms": float(np.hypot(current_mean, current_std)),
                "current_mean": current_mean, "current_std": current_std,
                "rpm": rpm, "temp_C": temp, "status": label,
                "source": "SYNTHETIC_RULE_SIMULATION",
                "scenario_id": f"SIM_{label}_{index // 16:02d}",
            })
    return pd.DataFrame(rows)


def augment_training(real_training, real_weights):
    """Add thermal priors only; other generated states are simulation exercises.

    Invented mechanical/electrical combinations can conflict with measured normal
    transients. Limit training augmentation to the missing thermal operating range.
    """
    synthetic = generate_scenarios(real_training)
    synthetic = synthetic.loc[synthetic.status == "OVERHEAT"].reset_index(drop=True)
    synthetic_weights = np.full(len(synthetic),
                                MAX_SYNTHETIC_WEIGHT_RATIO * float(np.sum(real_weights)) / len(synthetic))
    combined = pd.concat([real_training, synthetic], ignore_index=True)
    return combined, np.concatenate([real_weights, synthetic_weights])


def main():
    from training import BASE_DIR, load_dataset
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=BASE_DIR / "synthetic_dataset.csv")
    args = parser.parse_args()
    data = load_dataset(BASE_DIR / "dataset.txt")
    synthetic = generate_scenarios(data)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    synthetic.to_csv(args.output, index=False, float_format="%.9g", lineterminator="\n")
    print(f"Saved {len(synthetic)} artificial rows to {args.output}")
    print("Every row is marked SYNTHETIC_RULE_SIMULATION; scenario IDs are not real recordings.")
    print("Do not append this file to dataset.txt or use it as independent validation data.")


if __name__ == "__main__":
    main()
