import tensorflow as tf
import numpy as np
import pandas as pd
import glob
import subprocess
import os

files = ['data/synthetic_train.csv'] + glob.glob('data/ble_env_*.csv')
df = pd.concat([pd.read_csv(f) for f in files], ignore_index=True)

def normalize_row(row):
    return np.array([
        (row['temp_c']       - (-10.0)) / 70.0,
        (row['humidity_pct'] -   0.0)   / 100.0,
        (row['pressure_hpa'] - 900.0)   / 200.0,
    ], dtype=np.float32)

samples = np.array([normalize_row(r) for _, r in df.iterrows()])

def representative_dataset():
    for s in samples:
        yield [s.reshape(1, 3)]

converter = tf.lite.TFLiteConverter.from_saved_model('models/saved_model')
converter.optimizations = [tf.lite.Optimize.DEFAULT]
converter.representative_dataset = representative_dataset
converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
converter.inference_input_type  = tf.int8
converter.inference_output_type = tf.int8

quantized = converter.convert()
with open('models/model_quantized.tflite', 'wb') as f:
    f.write(quantized)

orig_size = len(open('models/model.tflite', 'rb').read())
print(f"Quantized model: {len(quantized)} bytes (was {orig_size} bytes)")

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
