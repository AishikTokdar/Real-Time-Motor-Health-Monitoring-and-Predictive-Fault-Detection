"""Shared dataset contract and reproducible evaluation for the two classifiers."""
from pathlib import Path
import csv
import hashlib
import os

import joblib
import numpy as np
import pandas as pd
from sklearn.base import clone
from sklearn.metrics import accuracy_score, balanced_accuracy_score, classification_report, confusion_matrix, f1_score
from sklearn.model_selection import StratifiedGroupKFold, StratifiedKFold

BASE_DIR = Path(__file__).resolve().parent
FEATURES = ["vib_rms", "vib_mean", "vib_std", "current_rms", "current_mean", "current_std", "rpm", "temp_C"]
CLASSES = ["NORMAL", "OVERCURRENT", "OVERHEAT", "STOPPED", "UNBALANCE"]
COLUMNS = ["time_ms", *FEATURES, "status"]


def load_dataset(path):
    # pandas may silently use a surplus CSV field as the index. Reject it first.
    with Path(path).open(newline="", encoding="utf-8-sig") as stream:
        rows = csv.reader(stream)
        if next(rows, None) != COLUMNS:
            raise ValueError(f"Expected CSV header: {','.join(COLUMNS)}")
        for number, row in enumerate(rows, 2):
            if row and len(row) != len(COLUMNS):
                raise ValueError(f"Line {number}: expected 10 fields, got {len(row)}")
    data = pd.read_csv(path)
    if data.empty or data.isna().any().any():
        raise ValueError("Dataset must contain complete, independently labeled rows")
    for name in COLUMNS[:-1]:
        data[name] = pd.to_numeric(data[name], errors="raise")
    if not np.isfinite(data[COLUMNS[:-1]].to_numpy(dtype=float)).all():
        raise ValueError("Dataset contains nonfinite measurements")
    if set(data.status) != set(CLASSES):
        raise ValueError(f"Labels must cover {CLASSES}; remove or label UNLABELED rows")
    if (data[FEATURES[:-1] + ["time_ms"]] < 0).any().any() or not data.temp_C.between(-55, 125).all():
        raise ValueError("Dataset contains negative magnitudes or invalid temperatures")
    if (data.groupby(FEATURES, dropna=False).status.nunique() > 1).any():
        raise ValueError("Identical feature vectors have conflicting class labels")
    cleaned = data.drop_duplicates(subset=FEATURES + ["status"]).reset_index(drop=True)
    print(f"Loaded {len(data)} rows; retained {len(cleaned)} distinct labeled sensor vectors")
    return cleaned


def training_weights(data):
    """Equal class mass; soften the influence of dense class/time blocks.

    Weights are computed separately inside each training fold. No observations
    are invented, repeated or removed to make minority classes look larger.
    """
    block = data.time_ms // 30000
    density = data.groupby([data.status, block]).status.transform("size")
    weights = 1.0 / np.sqrt(density.to_numpy(dtype=float))
    for label in CLASSES:
        mask = data.status.to_numpy() == label
        if mask.any():
            weights[mask] *= len(data) / (data.status.nunique() * weights[mask].sum())
    return weights


def evaluate(model, data, splits, title, use_synthetic=False):
    print(f"\n{title}")
    scores = []
    predicted = np.empty(len(data), dtype=object)
    for fold, (train, test) in enumerate(splits, 1):
        training = data.iloc[train]
        missing = sorted(set(CLASSES) - set(training.status))
        if missing:
            print(f"Fold {fold}: no training examples for {missing}; independent class coverage is insufficient")
        weights = training_weights(training)
                    training, weights = augment_training(training, weights)
        candidate = clone(model)
        candidate.fit(training[FEATURES], training.status, sample_weight=weights)
        values = candidate.predict(data[FEATURES].iloc[test])
        predicted[test] = values
        scores.append(accuracy_score(data.status.iloc[test], values))
    metrics = {
        "accuracy": float(accuracy_score(data.status, predicted)),
        "balanced_accuracy": float(balanced_accuracy_score(data.status, predicted)),
        "macro_f1": float(f1_score(data.status, predicted, labels=CLASSES, average="macro", zero_division=0)),
        "normal_false_fault_rate": float(np.mean(np.isin(
            predicted[data.status.to_numpy() == "NORMAL"], ["OVERCURRENT", "OVERHEAT", "UNBALANCE"]))),
    }
    print("Fold accuracies:", [round(score, 4) for score in scores])
    print("Out-of-fold metrics:", metrics)
    print(classification_report(data.status, predicted, labels=CLASSES, zero_division=0))
    print("Confusion matrix; class order:", CLASSES)
    print(confusion_matrix(data.status, predicted, labels=CLASSES))
    return metrics


def train_and_save(model, model_path, dataset_path=None, use_synthetic=False):
    dataset_path = Path(dataset_path or BASE_DIR / "dataset.txt")
    data = load_dataset(dataset_path)
    # Use contiguous acquisition blocks and keep all records of a timestamp
    # together. Blocks are approximate: the file has no independent run IDs.
    blocks = (data.time_ms // 30000).to_numpy()
    if pd.Series(blocks).nunique() < 5:
        raise ValueError("Need at least five time blocks for grouped evaluation")
    splitter = StratifiedGroupKFold(n_splits=5, shuffle=True, random_state=42)
    splits = list(splitter.split(data[FEATURES], data.status, groups=blocks))
    coverage = data.assign(block=blocks).groupby("status").agg(samples=("status", "size"), blocks=("block", "nunique"))
    print("Class/time-block concentration:\n", coverage.to_string())
            fitting, weights = augment_training(data, weights)
    model.fit(fitting[FEATURES], fitting.status, sample_weight=weights)
    model.training_metadata_ = {
        "dataset_sha256": hashlib.sha256(dataset_path.read_bytes()).hexdigest(),
        "distinct_rows": len(data),
        "class_counts": {key: int(value) for key, value in data.status.value_counts().items()},
        "class_block_counts": {key: int(value) for key, value in coverage.blocks.items()},
        "weighting": "equal_class_mass_inverse_sqrt_class_block_density",
        "grouped_evaluation": grouped,
        "stratified_diagnostic": stratified,
        "evaluation_real_rows_only": True,
        "synthetic_augmentation": {"enabled": False},
    }
    if use_synthetic:
        from generate_synthetic_data import RECIPE, SAMPLES_PER_CLASS, SEED, MAX_SYNTHETIC_WEIGHT_RATIO
        model.training_metadata_["synthetic_augmentation"] = {
            "enabled": True, "recipe": RECIPE, "seed": SEED,
            "samples_per_class": SAMPLES_PER_CLASS,
            "training_classes": ["OVERHEAT"],
            "synthetic_training_rows": SAMPLES_PER_CLASS,
            "maximum_weight_ratio_to_real": MAX_SYNTHETIC_WEIGHT_RATIO,
            "independent_recordings": False,
            "thermal_rule_celsius": [60, 90],
        }
    model_path = Path(model_path)
    temporary = model_path.with_suffix(model_path.suffix + ".tmp")
    try:
        joblib.dump(model, temporary)
        restored = joblib.load(temporary)
        if not np.array_equal(restored.predict(fitting[FEATURES]), model.predict(fitting[FEATURES])):
            raise ValueError("Serialized model failed prediction verification")
        try:
            temporary.replace(model_path)
        except PermissionError:
            # Some Windows repositories grant file writes but deny deletion/
            # rename of inherited files. Keep their ACLs and support that case.
            # This fallback is not crash-atomic; normal saves still use replace.
            if not model_path.is_file():
                raise
            payload = temporary.read_bytes()
            with model_path.open("r+b") as stream:
                original = stream.read()
                try:
                    stream.seek(0)
                    stream.write(payload)
                    stream.truncate()
                    stream.flush()
                    os.fsync(stream.fileno())
                except OSError:
                    stream.seek(0)
                    stream.write(original)
                    stream.truncate()
                    stream.flush()
                    raise
            print("Saved using in-place write: this file's Windows ACL denies rename/replace")
    finally:
        temporary.unlink(missing_ok=True)
    print("Saved model:", model_path)
    return model
