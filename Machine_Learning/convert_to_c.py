import joblib
from micromlgen import port

model = joblib.load("tinyml_model.pkl")

c_code = port(model)

with open("motor_model.h", "w") as f:
    f.write(c_code)

print("Model converted to C successfully!")
