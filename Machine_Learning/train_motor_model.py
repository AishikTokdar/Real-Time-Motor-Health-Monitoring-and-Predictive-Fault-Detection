from sklearn.ensemble import RandomForestClassifier
from training import BASE_DIR, train_and_save

if __name__ == "__main__":
    train_and_save(RandomForestClassifier(n_estimators=100, min_samples_leaf=2, random_state=42),
                   BASE_DIR / "motor_fault_model.pkl")
