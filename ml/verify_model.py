"""
Smoke-test the float32 TFLite model against five known class vectors.

Tests one representative input per class that should be clearly inside the
class region (not near a boundary). All five must classify correctly with
confidence ≥ 0.90 for the model to pass.

This test catches regressions in the training pipeline (e.g. if CLASS_ORDER
was accidentally reordered, all class indices would be wrong).

Usage:
    source .venv/bin/activate
    python3 verify_model.py

Prerequisites:
    models/model.tflite must exist (run train_classifier.py first).
"""
import numpy as np
import tensorflow as tf

CLASS_NAMES = ['comfortable', 'warm', 'cold', 'humid', 'danger']
NORM = {'temp_c': (-10, 60), 'humidity_pct': (0, 100), 'pressure_hpa': (900, 1100)}

def norm(t, h, p):
    return np.array([
        (t - (-10)) / 70,
        (h -   0)  / 100,
        (p - 900)  / 200,
    ], dtype=np.float32).reshape(1, 3)

test_vectors = [
    (22.0, 50.0, 1013.0, 'comfortable'),
    (35.0, 30.0, 1005.0, 'warm'),
    (5.0,  40.0, 1000.0, 'cold'),
    (22.0, 85.0, 1010.0, 'humid'),
    (55.0, 10.0,  980.0, 'danger'),
]

interp = tf.lite.Interpreter(model_path='models/model.tflite')
interp.allocate_tensors()
inp = interp.get_input_details()[0]
out = interp.get_output_details()[0]

print("Float32 model verification:")
all_pass = True
for t, h, p, expected in test_vectors:
    interp.set_tensor(inp['index'], norm(t, h, p))
    interp.invoke()
    scores = interp.get_tensor(out['index'])[0]
    pred = CLASS_NAMES[np.argmax(scores)]
    ok = pred == expected
    all_pass = all_pass and ok
    print(f"  {t}°C {h}% {p}hPa → {pred} (conf {scores.max():.2f}) {'✓' if ok else '✗ expected ' + expected}")

print("All pass!" if all_pass else "FAILURES — retrain with more data")
