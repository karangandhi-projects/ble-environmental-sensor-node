"""
Regression check — validate the saved_model against five known class vectors.

Tests one representative input per class that should be clearly inside the
class region (not near a boundary). All five must classify correctly with
confidence >= 0.90 for the model to pass.

This test catches regressions in the training pipeline (e.g. if CLASS_ORDER
was accidentally reordered, all class indices would be wrong).

Usage:
    source .venv/bin/activate
    python3 verify_model.py

Prerequisites:
    models/saved_model/ must exist (run train_classifier.py first).
    TensorFlow 2.13+ must be installed (pip install -r requirements.txt).
"""
import sys
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

model = tf.saved_model.load('models/saved_model')
infer = model.signatures['serving_default']
output_key = list(infer.structured_outputs.keys())[0]

print("saved_model verification:")
all_pass = True
for t, h, p, expected in test_vectors:
    result = infer(keras_tensor=tf.constant(norm(t, h, p)))
    scores = result[output_key].numpy()[0]
    pred = CLASS_NAMES[int(np.argmax(scores))]
    conf = float(scores.max())
    ok = (pred == expected) and (conf >= 0.90)
    all_pass = all_pass and ok
    status = 'OK' if ok else 'FAIL (expected {}, conf {:.2f})'.format(expected, conf)
    print(f"  {t}C {h}% {p}hPa -> {pred} (conf {conf:.2f}) {status}")

if all_pass:
    print("All pass!")
    sys.exit(0)
else:
    print("FAILURES -- retrain with more data or check CLASS_ORDER")
    sys.exit(1)
