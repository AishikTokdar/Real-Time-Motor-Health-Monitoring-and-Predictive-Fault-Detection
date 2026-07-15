import pandas as pd
from sklearn.model_selection import train_test_split
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import accuracy_score

data = pd.read_csv("dataset.txt")

print("Dataset loaded successfully")
print(data.head())

X = data.drop(["time_ms","status"], axis=1)

y = data["status"]

X_train, X_test, y_train, y_test = train_test_split(
    X, y, test_size=0.2, random_state=42
)

model = RandomForestClassifier(n_estimators=100)
model.fit(X_train, y_train)

print("Training completed")

predictions = model.predict(X_test)

accuracy = accuracy_score(y_test, predictions)

print("Model Accuracy:", accuracy)

import joblib

joblib.dump(model, "motor_fault_model.pkl")

print("Model saved successfully!")
