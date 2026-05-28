"""
Post-training int8 quantization of the BLE_ENV_NODE classifier.

Converts the float32 SavedModel (models/saved_model/) to a fully int8-quantized
TFLite model (models/model_quantized.tflite) and generates a C byte array
(firmware/components/tinyml_inference/model_data.cc) for potential use with
a TFLite Micro runtime.

Why int8 quantization?
    Float32: 4 bytes/weight → 245 weights × 4 = 980 bytes
    Int8:    1 byte/weight  → 245 weights × 1 = 245 bytes + scale/zero-point per tensor
    Quantization reduces model size by ~4× with minimal accuracy loss (<1%).

Note: model_data.cc is currently NOT used by the firmware (which uses the
pure-C ml_weights.h approach instead). It is retained here for reference in
case TFLite Micro support is added later (ESP-IDF component availability
permitting). See DD-018 in docs/design_decisions.md.

Representative dataset:
    Full int8 quantization (both weights AND activations) requires a calibration
    step where the quantizer observes the activation ranges. We pass the entire
    training dataset as the representative_dataset() generator. More diverse
    data here = better quantization accuracy.

Usage:
    source .venv/bin/activate
    python3 quantize.py

Prerequisites:
    models/saved_model/ must exist (run train_classifier.py first).
"""
import tensorflow as tf
import numpy as np
import pandas as pd
import glob
import subprocess
import os

# Load same data used for training to build the representative dataset.
files = ['data/synthetic_train.csv'] + glob.glob('data/ble_env_*.csv')
df = pd.concat([pd.read_csv(f) for f in files], ignore_index=True)


def normalize_row(row):
    """Normalize a single DataFrame row to [0,1] using training NORM ranges."""
    return np.array([
        (row['temp_c']       - (-10.0)) / 70.0,
        (row['humidity_pct'] -   0.0)   / 100.0,
        (row['pressure_hpa'] - 900.0)   / 200.0,
    ], dtype=np.float32)


samples = np.array([normalize_row(r) for _, r in df.iterrows()])


def representative_dataset():
    """Yield calibration inputs for the TFLite int8 quantizer.

    Each yield is a list of one input tensor, shape (1, 3), float32.
    The quantizer uses these to determine the min/max ranges of all activations,
    which determines the int8 scale and zero-point per tensor.
    """
    for s in samples:
        yield [s.reshape(1, 3)]


converter = tf.lite.TFLiteConverter.from_saved_model('models/saved_model')
# DEFAULT optimization enables weight quantization; combined with the representative
# dataset and TFLITE_BUILTINS_INT8, this produces full integer quantization.
converter.optimizations = [tf.lite.Optimize.DEFAULT]
converter.representative_dataset = representative_dataset
converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
converter.inference_input_type  = tf.int8  # input tensor is int8
converter.inference_output_type = tf.int8  # output tensor is int8

quantized = converter.convert()
with open('models/model_quantized.tflite', 'wb') as f:
    f.write(quantized)

orig_size = len(open('models/model.tflite', 'rb').read())
print(f"Quantized model: {len(quantized)} bytes (was {orig_size} bytes)")

# Generate C byte array using xxd, then rename variables to match the
# extern declarations in tinyml_inference.cpp (if TFLite Micro is used).
result = subprocess.run(
    ['xxd', '-i', 'model_quantized.tflite'],
    capture_output=True, text=True, cwd='models'
)
cc_content = result.stdout \
    .replace('unsigned char model_quantized_tflite[]', 'const unsigned char g_model_data[]') \
    .replace('unsigned int model_quantized_tflite_len', 'const unsigned int g_model_data_len')

os.makedirs('../firmware/components/tinyml_inference', exist_ok=True)
with open('../firmware/components/tinyml_inference/model_data.cc', 'w') as f:
    f.write(cc_content)
print("Generated firmware/components/tinyml_inference/model_data.cc")
