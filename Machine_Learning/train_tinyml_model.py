from sklearn.tree import DecisionTreeClassifier
from training import BASE_DIR, train_and_save

if __name__ == "__main__":
    train_and_save(DecisionTreeClassifier(max_depth=6, min_samples_leaf=3, random_state=42),
                   BASE_DIR / "tinyml_model.pkl")
