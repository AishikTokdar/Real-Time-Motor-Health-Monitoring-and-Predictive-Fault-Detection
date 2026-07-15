import pandas as pd
from sklearn.model_selection import train_test_split
from sklearn.tree import DecisionTreeClassifier
from sklearn.metrics import accuracy_score
import joblib

data = pd.read_csv("dataset.txt")

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

joblib.dump(model, "tinyml_model.pkl")

print("TinyML model saved!")
