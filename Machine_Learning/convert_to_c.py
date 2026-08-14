import os
from pathlib import Path
import joblib
from micromlgen import port

BASE_DIR = Path(__file__).resolve().parent
REPO_ROOT = BASE_DIR.parent

model_path = BASE_DIR / "tinyml_model.pkl"

model = joblib.load(model_path)

c_code = port(model)

target_paths = [
    BASE_DIR / "motor_model.h",
    REPO_ROOT / "Firmware" / "motor_model.h",
    REPO_ROOT / "Firmware_old" / "motor_model.h",
]

for p in target_paths:
    p.parent.mkdir(parents=True, exist_ok=True)
    with open(p, "w") as f:
        f.write(c_code)

print(f"Model converted to C successfully! Exported to {len(target_paths)} locations.")
