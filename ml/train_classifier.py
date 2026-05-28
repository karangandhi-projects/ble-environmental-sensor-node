"""
Train a 5-class environmental MLP classifier for BLE_ENV_NODE.

Architecture: 3 inputs → Dense(16, ReLU) → Dense(8, ReLU) → Dense(5, softmax)
Total parameters: 3×16+16 + 16×8+8 + 8×5+5 = 245

Inputs (normalized to [0,1]):
    temp_c       normalized by (x - (-10)) / 70   [range -10..60°C]
    humidity_pct normalized by x / 100             [range 0..100%]
    pressure_hpa normalized by (x - 900) / 200     [range 900..1100 hPa]

CRITICAL: CLASS_ORDER must match:
    - BLE_ENV_ML_CLASS_* defines in firmware/components/app_core/include/app_config.h
    - ml_class_t enum in firmware/components/tinyml_inference/include/tinyml_inference.h
    - ML_CLASS_NAMES in android/.../ui/DataAlertsScreen.kt
Do NOT use sklearn's LabelEncoder (it sorts alphabetically and would change indices).

Outputs:
    models/model.tflite     — float32 TFLite model (for Android MlClassifier, Phase 9B)
    models/saved_model/     — Keras SavedModel format (for quantize.py, Phase 9C)

Usage:
    source .venv/bin/activate
    python3 train_classifier.py

After training, run verify_model.py to smoke-test, then quantize.py to generate
firmware/components/tinyml_inference/include/ml_weights.h.
"""
import pandas as pd
import numpy as np
import tensorflow as tf
from sklearn.model_selection import train_test_split
import os, glob

# Class order MUST match app_config.h BLE_ENV_ML_CLASS_* defines.
CLASS_ORDER = ['comfortable', 'warm', 'cold', 'humid', 'danger']
CLASS_MAP   = {cls: i for i, cls in enumerate(CLASS_ORDER)}

# Load all available data: synthetic baseline + real device CSVs from Android export.
files = ['data/synthetic_train.csv'] + glob.glob('data/ble_env_*.csv')
df = pd.concat([pd.read_csv(f) for f in files], ignore_index=True)
print(f"Total samples: {len(df)}")
print(df['label'].value_counts())

# Normalization ranges — MUST match NORM dict in this file AND in tinyml_inference.c.
# Changing these requires retraining AND updating the firmware constants.
NORM = {'temp_c': (-10, 60), 'humidity_pct': (0, 100), 'pressure_hpa': (900, 1100)}


def normalize(df):
    """Normalize input features to [0, 1] using fixed per-feature ranges.

    Args:
        df: DataFrame with columns temp_c, humidity_pct, pressure_hpa.

    Returns:
        float32 numpy array of shape (n_samples, 3).
    """
    out = df[['temp_c', 'humidity_pct', 'pressure_hpa']].copy()
    for col, (lo, hi) in NORM.items():
        out[col] = (out[col] - lo) / (hi - lo)
    return out.values.astype(np.float32)


X = normalize(df)
y = df['label'].map(CLASS_MAP).values
X_train, X_test, y_train, y_test = train_test_split(
    X, y, test_size=0.2, stratify=y, random_state=42)

# 3-layer MLP: two ReLU hidden layers + softmax output.
# Layer sizes (16, 8) chosen to balance accuracy vs. flash footprint.
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
# SavedModel format required by quantize.py (tf.lite.TFLiteConverter.from_saved_model).
model.export('models/saved_model')

# Float32 TFLite model for Android MlClassifier (Phase 9B integration).
converter = tf.lite.TFLiteConverter.from_keras_model(model)
tflite_model = converter.convert()
with open('models/model.tflite', 'wb') as f:
    f.write(tflite_model)

print(f"Saved models/model.tflite ({len(tflite_model)} bytes)")
print("Class order:", CLASS_ORDER)
