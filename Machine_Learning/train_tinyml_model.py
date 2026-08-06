from pathlib import Path
import pandas as pd
from sklearn.model_selection import train_test_split
from sklearn.tree import DecisionTreeClassifier
from sklearn.metrics import accuracy_score
import joblib

BASE_DIR = Path(__file__).resolve().parent
dataset_path = BASE_DIR / "dataset.txt"
model_path = BASE_DIR / "tinyml_model.pkl"

data = pd.read_csv(dataset_path)

print("Dataset loaded successfully")

X = data.drop(["time_ms","status"], axis=1)
y = data["status"]

X_train, X_test, y_train, y_test = train_test_split(
    X, y, test_size=0.2, random_state=42
)

model = DecisionTreeClassifier(max_depth=6)
model.fit(X_train, y_train)

print("TinyML model trained")

pred = model.predict(X_test)

acc = accuracy_score(y_test, pred)
print("TinyML Model Accuracy:", acc)

joblib.dump(model, model_path)

print("TinyML model saved!")
