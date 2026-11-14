from sklearn.tree import DecisionTreeClassifier
import argparse
from training import BASE_DIR, train_and_save


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Train the embedded Decision Tree")
    parser.add_argument("--synthetic", action="store_true", help="Add low-weight rule-based synthetic training scenarios; validation stays real-only")
    args = parser.parse_args()
    train_and_save(DecisionTreeClassifier(max_depth=6, min_samples_leaf=3, random_state=42),
                   BASE_DIR / "tinyml_model.pkl", use_synthetic=args.synthetic)
