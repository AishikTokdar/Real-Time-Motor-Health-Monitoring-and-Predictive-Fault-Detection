from sklearn.ensemble import RandomForestClassifier
import argparse
from training import BASE_DIR, train_and_save


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Train the offline Random Forest")
    parser.add_argument("--synthetic", action="store_true", help="Add low-weight rule-based synthetic training scenarios; validation stays real-only")
    args = parser.parse_args()
    train_and_save(RandomForestClassifier(n_estimators=100, min_samples_leaf=2, random_state=42),
                   BASE_DIR / "motor_fault_model.pkl", use_synthetic=args.synthetic)
