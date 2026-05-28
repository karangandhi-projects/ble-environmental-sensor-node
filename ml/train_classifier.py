import pandas as pd
import numpy as np
import tensorflow as tf
from sklearn.model_selection import train_test_split
import os, glob

# Class order MUST match app_config.h BLE_ENV_ML_CLASS_* defines
CLASS_ORDER = ['comfortable', 'warm', 'cold', 'humid', 'danger']
CLASS_MAP   = {cls: i for i, cls in enumerate(CLASS_ORDER)}

files = ['data/synthetic_train.csv'] + glob.glob('data/ble_env_*.csv')
df = pd.concat([pd.read_csv(f) for f in files], ignore_index=True)
print(f"Total samples: {len(df)}")
print(df['label'].value_counts())

NORM = {'temp_c': (-10, 60), 'humidity_pct': (0, 100), 'pressure_hpa': (900, 1100)}

def normalize(df):
    out = df[['temp_c', 'humidity_pct', 'pressure_hpa']].copy()
    for col, (lo, hi) in NORM.items():
        out[col] = (out[col] - lo) / (hi - lo)
    return out.values.astype(np.float32)

X = normalize(df)
y = df['label'].map(CLASS_MAP).values
X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, stratify=y, random_state=42)

model = tf.keras.Sequential([
    tf.keras.layers.Input(shape=(3,)),
    tf.keras.layers.Dense(16, activation='relu'),
    tf.keras.layers.Dense(8,  activation='relu'),
    tf.keras.layers.Dense(5,  activation='softmax'),
])
model.compile(optimizer='adam', loss='sparse_categorical_crossentropy', metrics=['accuracy'])
model.fit(X_train, y_train, epochs=50, batch_size=32, validation_split=0.1, verbose=1)

loss, acc = model.evaluate(X_test, y_test, verbose=0)
print(f"\nTest accuracy: {acc:.3f}  (target: >0.85)")
if acc < 0.85:
    raise SystemExit("Accuracy below 0.85 — increase sample count in collect_synthetic.py and re-run.")

os.makedirs('models', exist_ok=True)
model.export('models/saved_model')  # SavedModel dir for quantize.py (Keras 3 API)

converter = tf.lite.TFLiteConverter.from_keras_model(model)
tflite_model = converter.convert()
with open('models/model.tflite', 'wb') as f:
    f.write(tflite_model)

print(f"Saved models/model.tflite ({len(tflite_model)} bytes)")
print("Class order:", CLASS_ORDER)
