from pathlib import Path
import warnings
import joblib
from micromlgen import port
from sklearn.exceptions import InconsistentVersionWarning
from sklearn.tree import DecisionTreeClassifier
from training import FEATURES, CLASSES

BASE_DIR = Path(__file__).resolve().parent
REPO_ROOT = BASE_DIR.parent

model_path = BASE_DIR / "tinyml_model.pkl"

with warnings.catch_warnings():
    warnings.simplefilter("error", InconsistentVersionWarning)
    model = joblib.load(model_path)
if not isinstance(model, DecisionTreeClassifier):
    raise ValueError("The ESP32 exporter expects the saved DecisionTreeClassifier")
if list(model.feature_names_in_) != FEATURES or list(model.classes_) != CLASSES:
    raise ValueError("Model feature order or class IDs differ from the firmware contract")

c_code = port(model)
synthetic = getattr(model, "training_metadata_", {}).get("synthetic_augmentation", {})
if synthetic.get("enabled", False):
    print("Exporting an experimental synthetic-augmented model; it is not independently validated.")
    c_code = "// EXPERIMENTAL: synthetic thermal priors; not independent motor validation.\n" + c_code

target_paths = [
    BASE_DIR / "motor_model.h",
    REPO_ROOT / "Firmware" / "motor_model.h",
    REPO_ROOT / "Firmware_old" / "motor_model.h",
]

for p in target_paths:
    p.parent.mkdir(parents=True, exist_ok=True)
    with open(p, "w", encoding="utf-8") as f:
        f.write(c_code)

print(f"Model converted to C successfully! Exported to {len(target_paths)} locations.")
