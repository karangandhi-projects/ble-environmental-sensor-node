# TinyML Edge Inference Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Train a 5-class environmental classifier (comfortable/warm/cold/humid/danger), validate it on the Android app (Phase B), then deploy as TFLite Micro on the ESP32-C3 so the device sends BLE `b7e00007` alerts autonomously (Phase C).

**Architecture:** Phase B — Python/Keras training on PC, TFLite Android integration, live inference in the app. Phase C — int8 quantized TFLite Micro model embedded in a new `tinyml_inference` ESP-IDF component, called from `telemetry_task`, notifies `b7e00007` on class change.

**Tech Stack:** Python 3.10+, TensorFlow 2.x/Keras, pandas, scikit-learn (Phase B). TFLite Micro runtime for ESP-IDF (Phase C). Kotlin TFLite Android library (Phase B app integration).

**Prerequisites:** 
- `2026-05-28-phase9a-firmware-gatt-v2.md` complete (b7e00007 characteristic exists in firmware)
- `2026-05-28-phase9b-android-companion.md` complete (Data tab collects labeled CSVs)
- At least 100 labeled telemetry samples collected (20+ per class) exported from the Android app

---

## File Map

```
ml/
  requirements.txt               — Python deps
  collect_synthetic.py           — generate synthetic training data
  train_classifier.py            — train + evaluate + export model.tflite
  quantize.py                    — int8 quantize + generate model_data.cc
  verify_model.py                — run inference against test vectors
  data/                          — CSV files exported from Android app (gitignored)
  models/
    model.tflite                 — float32 model (Phase B)
    model_quantized.tflite       — int8 model (Phase C)

android/BleEnvNode/app/src/main/java/com/bleenvnode/
  MlClassifier.kt                — wraps TFLite interpreter (Phase B)
  BleViewModel.kt                — add liveClass StateFlow (Phase B)
  ui/DataAlertsScreen.kt         — show live class from phone inference (Phase B)

firmware/components/tinyml_inference/
  CMakeLists.txt                 — new component
  include/tinyml_inference.h     — public header
  tinyml_inference.cpp           — inference wrapper (C++ — TFLite Micro is C++)
  model_data.cc                  — quantized model as C byte array (generated)

firmware/main/app_main.c         — call tinyml_inference_init() (approval gate, Phase C)
firmware/components/app_core/include/app_config.h — already has ML class defines (Phase 9A)
```

---

## Phase B — Phone-side ML Validation

### Task 1: Set up Python training environment

- [ ] **Step 1: Create requirements.txt**

`ml/requirements.txt`:
```
tensorflow>=2.13.0
pandas>=2.0.0
scikit-learn>=1.3.0
numpy>=1.24.0
```

- [ ] **Step 2: Install dependencies**

```bash
cd /home/karan-gandhi/ble_skill_project_package_reviewed/ml
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
```

Expected: packages installed without errors.

---

### Task 2: Generate synthetic training data

- [ ] **Step 1: Create collect_synthetic.py**

`ml/collect_synthetic.py`:
```python
import pandas as pd
import numpy as np

rng = np.random.default_rng(42)

def samples(n, temp_range, hum_range, press_range, label):
    return pd.DataFrame({
        'timestamp_ms': np.arange(n) * 2000,
        'temp_c':       rng.uniform(*temp_range, n).round(2),
        'humidity_pct': rng.uniform(*hum_range, n).round(2),
        'pressure_hpa': rng.uniform(*press_range, n).round(2),
        'label':        label
    })

df = pd.concat([
    samples(300, (18, 25), (30, 60), (1000, 1025), 'comfortable'),
    samples(300, (28, 45), (20, 50), (990, 1020),  'warm'),
    samples(300, (-5, 15), (20, 50), (985, 1015),  'cold'),
    samples(300, (18, 28), (75, 99), (995, 1020),  'humid'),
    samples(300, (46, 60), (0,  20), (970, 995),   'danger'),
], ignore_index=True).sample(frac=1, random_state=42)

df.to_csv('data/synthetic_train.csv', index=False)
print(f"Generated {len(df)} samples → data/synthetic_train.csv")
print(df['label'].value_counts())
```

- [ ] **Step 2: Run it**

```bash
mkdir -p data && python3 collect_synthetic.py
```

Expected:
```
Generated 1500 samples → data/synthetic_train.csv
comfortable    300
warm           300
cold           300
humid          300
danger         300
```

---

### Task 3: Train and export the classifier

- [ ] **Step 1: Create train_classifier.py**

Class indices are fixed to match `app_config.h` defines (comfortable=0, warm=1, cold=2, humid=3, danger=4). Do NOT use LabelEncoder which sorts alphabetically.

`ml/train_classifier.py`:
```python
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
model.save('models/saved_model')  # needed by quantize.py

converter = tf.lite.TFLiteConverter.from_keras_model(model)
tflite_model = converter.convert()
with open('models/model.tflite', 'wb') as f:
    f.write(tflite_model)

print(f"Saved models/model.tflite ({len(tflite_model)} bytes)")
print("Class order:", CLASS_ORDER)
```

- [ ] **Step 2: Run training**

```bash
python3 train_classifier.py
```

Expected: test accuracy ≥ 0.85, `models/model.tflite` and `models/saved_model/` created.

If accuracy < 0.85: increase sample count in `collect_synthetic.py` (300 → 500 per class) and re-run.

---

### Task 4: Integrate TFLite model into Android app (Phase B)

**Files:**
- Modify: `android/BleEnvNode/app/build.gradle.kts` — add TFLite dependency
- Create: `android/BleEnvNode/app/src/main/assets/model.tflite` — copy model file
- Create: `android/BleEnvNode/app/src/main/java/com/bleenvnode/MlClassifier.kt`
- Modify: `android/BleEnvNode/app/src/main/java/com/bleenvnode/BleViewModel.kt` — add liveClass

- [ ] **Step 1: Add TFLite dependency to app/build.gradle.kts**

In the `dependencies` block, add:
```kotlin
implementation("org.tensorflow:tensorflow-lite:2.14.0")
implementation("org.tensorflow:tensorflow-lite-support:0.4.4")
```

In the `android` block, add:
```kotlin
aaptOptions { noCompress += "tflite" }
```

- [ ] **Step 2: Copy model to assets**

```bash
mkdir -p /home/karan-gandhi/ble_skill_project_package_reviewed/android/BleEnvNode/app/src/main/assets
cp /home/karan-gandhi/ble_skill_project_package_reviewed/ml/models/model.tflite \
   /home/karan-gandhi/ble_skill_project_package_reviewed/android/BleEnvNode/app/src/main/assets/
```

- [ ] **Step 3: Create MlClassifier.kt**

```kotlin
package com.bleenvnode

import android.content.Context
import org.tensorflow.lite.Interpreter
import java.io.FileInputStream
import java.nio.MappedByteBuffer
import java.nio.channels.FileChannel

class MlClassifier(context: Context) {

    private val interpreter: Interpreter

    init {
        val afd = context.assets.openFd("model.tflite")
        val fis = FileInputStream(afd.fileDescriptor)
        val buffer: MappedByteBuffer = fis.channel.map(
            FileChannel.MapMode.READ_ONLY, afd.startOffset, afd.declaredLength)
        interpreter = Interpreter(buffer)
    }

    // Normalization constants matching train_classifier.py NORM dict
    private fun normalize(tempC: Float, humPct: Float, pressHpa: Float): FloatArray {
        return floatArrayOf(
            (tempC   - (-10f)) / (60f   - (-10f)),
            (humPct  -   0f)  / (100f  -   0f),
            (pressHpa - 900f) / (1100f - 900f)
        )
    }

    val classNames = listOf("comfortable", "warm", "cold", "humid", "danger")

    data class Result(val className: String, val classIndex: Int, val confidence: Int)

    fun classify(tempC: Float, humPct: Float, pressHpa: Float): Result {
        val input  = arrayOf(normalize(tempC, humPct, pressHpa))
        val output = Array(1) { FloatArray(5) }
        interpreter.run(input, output)
        val scores = output[0]
        val maxIdx = scores.indices.maxByOrNull { scores[it] }!!
        return Result(
            className   = classNames.getOrElse(maxIdx) { "unknown" },
            classIndex  = maxIdx,
            confidence  = (scores[maxIdx] * 100).toInt()
        )
    }
}
```

- [ ] **Step 4: Add live classification to BleViewModel.kt**

Add after the `repo` declaration:
```kotlin
private val classifier by lazy { MlClassifier(app.applicationContext) }

private val _liveClass = MutableStateFlow<MlClassifier.Result?>(null)
val liveClass: StateFlow<MlClassifier.Result?> = _liveClass
```

In the `init` block, add alongside the telemetry collection:
```kotlin
scope.kotlinx.coroutines.launch {
    telemetry.collect { t ->
        t ?: return@collect
        _liveClass.value = classifier.classify(t.tempC, t.humidityPct, t.pressureHpa)
    }
}
```

- [ ] **Step 5: Show live class in DataAlertsScreen.kt**

In `DataAlertsScreen`, add:
```kotlin
val liveClass by vm.liveClass.collectAsState()
```

After the `mlAlert` card, add:
```kotlin
liveClass?.let { result ->
    Spacer(Modifier.height(8.dp))
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(12.dp)) {
            Text("Phone ML (live)", style = MaterialTheme.typography.labelLarge)
            Text("Class: ${result.className}")
            Text("Confidence: ${result.confidence}%")
        }
    }
}
```

- [ ] **Step 6: Build and verify**

```bash
cd /home/karan-gandhi/ble_skill_project_package_reviewed/android/BleEnvNode && ./gradlew assembleDebug
```

Install and open Data tab. Move sliders through different ranges and confirm the "Phone ML (live)" card shows expected class labels.

---

## Phase C — Edge Deployment

### Task 5: Quantize model to int8

- [ ] **Step 1: Create quantize.py**

`ml/quantize.py`:
```python
import tensorflow as tf
import numpy as np
import pandas as pd
import glob
import subprocess

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

import os
os.makedirs('../firmware/components/tinyml_inference', exist_ok=True)
with open('../firmware/components/tinyml_inference/model_data.cc', 'w') as f:
    f.write(cc_content)
print("Generated firmware/components/tinyml_inference/model_data.cc")
```

- [ ] **Step 2: Run quantize.py**

Requires `models/saved_model/` from `train_classifier.py` (Task 3 Step 2 saves it).

```bash
source .venv/bin/activate && python3 quantize.py
```

Expected output:
```
Quantized model: ~2000 bytes (was ~5000 bytes)
Generated firmware/components/tinyml_inference/model_data.cc
```

---

### Task 6: Create tinyml_inference firmware component

**Files:**
- Create: `firmware/components/tinyml_inference/CMakeLists.txt`
- Create: `firmware/components/tinyml_inference/include/tinyml_inference.h`
- Create: `firmware/components/tinyml_inference/tinyml_inference.cpp` (C++ — TFLite Micro is a C++ library)
- Create: `firmware/components/tinyml_inference/model_data.cc` (generated in Task 5)

- [ ] **Step 1: Add TFLite Micro to ESP-IDF**

TFLite Micro for ESP-IDF is available as an IDF component. Add it to the firmware:

```bash
cd /home/karan-gandhi/ble_skill_project_package_reviewed/firmware
idf_component_manager add tensorflow/lite-micro
```

Or add to `firmware/main/idf_component.yml` (create if absent):
```yaml
dependencies:
  tensorflow/lite-micro: ">=1.0.0"
```

Then run:
```bash
source ~/esp/esp-idf/export.sh && idf.py update-dependencies
```

- [ ] **Step 2: Create CMakeLists.txt**

`firmware/components/tinyml_inference/CMakeLists.txt`:
```cmake
idf_component_register(
    SRCS
        "tinyml_inference.cpp"
        "model_data.cc"
    INCLUDE_DIRS "include"
    REQUIRES app_core
)
target_compile_options(${COMPONENT_LIB} PRIVATE -fno-exceptions -fno-rtti)
```

- [ ] **Step 3: Create tinyml_inference.h**

`firmware/components/tinyml_inference/include/tinyml_inference.h`:
```c
#pragma once

#include <stdint.h>
#include "esp_err.h"

typedef enum {
    ML_CLASS_COMFORTABLE = 0,
    ML_CLASS_WARM        = 1,
    ML_CLASS_COLD        = 2,
    ML_CLASS_HUMID       = 3,
    ML_CLASS_DANGER      = 4,
    ML_CLASS_ANOMALY     = 5,
} ml_class_t;

typedef struct {
    ml_class_t class_id;
    uint8_t    confidence;  /* 0–100, softmax max × 100 */
} ml_result_t;

esp_err_t  tinyml_inference_init(void);
ml_result_t tinyml_infer(float temp_c, float humidity_pct, float pressure_hpa);
```

- [ ] **Step 4: Create tinyml_inference.cpp**

`firmware/components/tinyml_inference/tinyml_inference.cpp`:
```c
#include "tinyml_inference.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "tinyml";

extern const unsigned char g_model_data[];
extern const unsigned int  g_model_data_len;

static constexpr int kTensorArenaSize = 8 * 1024;
static uint8_t tensor_arena[kTensorArenaSize];

static tflite::MicroInterpreter *s_interpreter = nullptr;
static TfLiteTensor *s_input  = nullptr;
static TfLiteTensor *s_output = nullptr;

esp_err_t tinyml_inference_init(void)
{
    const tflite::Model *model = tflite::GetModel(g_model_data);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        ESP_LOGE(TAG, "Schema version mismatch: %lu vs %d",
                 (unsigned long)model->version(), TFLITE_SCHEMA_VERSION);
        return ESP_FAIL;
    }

    static tflite::MicroMutableOpResolver<4> resolver;
    resolver.AddFullyConnected();
    resolver.AddSoftmax();
    resolver.AddRelu();
    resolver.AddQuantize();

    static tflite::MicroInterpreter interpreter(model, resolver, tensor_arena, kTensorArenaSize);
    if (interpreter.AllocateTensors() != kTfLiteOk) {
        ESP_LOGE(TAG, "AllocateTensors failed");
        return ESP_FAIL;
    }

    s_interpreter = &interpreter;
    s_input       = interpreter.input(0);
    s_output      = interpreter.output(0);

    ESP_LOGI(TAG, "TFLite Micro initialized. Input shape: [%d, %d], Output shape: [%d, %d]",
             s_input->dims->data[0], s_input->dims->data[1],
             s_output->dims->data[0], s_output->dims->data[1]);
    return ESP_OK;
}

ml_result_t tinyml_infer(float temp_c, float humidity_pct, float pressure_hpa)
{
    ml_result_t fallback = { .class_id = ML_CLASS_COMFORTABLE, .confidence = 0 };
    if (!s_interpreter) return fallback;

    /* Normalize to [0,1] matching training script NORM constants */
    float norm[3] = {
        (temp_c       - (-10.0f)) / 70.0f,
        (humidity_pct -   0.0f)  / 100.0f,
        (pressure_hpa - 900.0f)  / 200.0f,
    };

    /* int8 quantization: value = (float - zero_point) / scale */
    float scale      = s_input->params.scale;
    int32_t zp       = s_input->params.zero_point;
    for (int i = 0; i < 3; i++) {
        s_input->data.int8[i] = (int8_t)((norm[i] / scale) + zp);
    }

    if (s_interpreter->Invoke() != kTfLiteOk) return fallback;

    /* Find max output class */
    int8_t max_val = s_output->data.int8[0];
    int    max_idx = 0;
    for (int i = 1; i < 5; i++) {
        if (s_output->data.int8[i] > max_val) {
            max_val = s_output->data.int8[i];
            max_idx = i;
        }
    }

    /* Dequantize confidence score to 0–100 */
    float out_scale = s_output->params.scale;
    int32_t out_zp  = s_output->params.zero_point;
    float confidence = (max_val - out_zp) * out_scale;

    return (ml_result_t){
        .class_id   = (ml_class_t)max_idx,
        .confidence = (uint8_t)(confidence * 100.0f),
    };
}
```

---

### Task 7: Wire tinyml_inference into telemetry_task (approval gate)

**Files:**
- Modify: `firmware/main/app_main.c`

- [ ] **Step 1: Request approval**

`app_main.c` is an existing file. Request explicit "yes approve" from user before making changes.

- [ ] **Step 2: Add include and init call in app_main.c**

Find the `app_main()` function and add after `sensor_provider_init()`:
```c
#include "tinyml_inference.h"

// In app_main, after sensor_provider_init():
if (tinyml_inference_init() != ESP_OK) {
    ESP_LOGW("main", "TinyML init failed — alerts disabled");
}
```

- [ ] **Step 3: Find the telemetry_task in app_main.c**

Look for the FreeRTOS task that calls `sensor_provider_read()` and `ble_env_service_notify_telemetry()`. Add inference after the read:

```c
sensor_sample_t sample = sensor_provider_read();

/* TinyML inference — only notify b7e00007 on class change */
static ml_class_t s_last_class = ML_CLASS_COMFORTABLE;
ml_result_t ml = tinyml_infer(
    sample.temperature_c_x100 / 100.0f,
    sample.humidity_pct_x100  / 100.0f,
    sample.pressure_pa        / 100.0f   /* Pa → hPa */
);
if (ml.class_id != s_last_class) {
    s_last_class = ml.class_id;
    ble_env_service_notify_ml_alert((uint8_t)ml.class_id, ml.confidence);
}
```

---

### Task 8: Build firmware with TFLite Micro

- [ ] **Step 1: Build**

```bash
source ~/esp/esp-idf/export.sh && cd /home/karan-gandhi/ble_skill_project_package_reviewed/firmware && idf.py build 2>&1 | tail -15
```

Expected:
```
Project build complete.
```

- [ ] **Step 2: Check binary size**

```bash
idf.py size
```

Expected: total binary < 1MB. TFLite Micro runtime ~100KB, model ~2KB. If over limit, enable size optimization in sdkconfig:
```
CONFIG_COMPILER_OPTIMIZATION_SIZE=y
```

---

### Task 9: Flash and verify edge inference

- [ ] **Step 1: Flash**

```bash
source ~/esp/esp-idf/export.sh && cd /home/karan-gandhi/ble_skill_project_package_reviewed/firmware && idf.py -p /dev/ttyUSB0 flash monitor
```

- [ ] **Step 2: Verify in serial monitor**

Expected boot log:
```
I (xxx) tinyml: TFLite Micro initialized. Input shape: [1, 3], Output shape: [1, 5]
```

- [ ] **Step 3: Verify with Android app**

Connect Android app. Navigate to Data/Alerts tab. Use Sensor tab to set:
- Temp 50°C, Humidity 10%, Pressure 980hPa → expect `ML Alert: danger`
- Temp 22°C, Humidity 50%, Pressure 1013hPa → expect `ML Alert: comfortable`
- Temp 35°C, Humidity 30%, Pressure 1005hPa → expect `ML Alert: warm`

Confirm the Alerts sub-tab shows incoming `b7e00007` notifications with correct class names.

---

### Task 10: Commit

- [ ] **Step 1: Stage and commit**

```bash
git add \
  ml/ \
  firmware/components/tinyml_inference/ \
  firmware/main/app_main.c \
  android/BleEnvNode/app/src/main/assets/model.tflite \
  android/BleEnvNode/app/src/main/java/com/bleenvnode/MlClassifier.kt \
  android/BleEnvNode/app/src/main/java/com/bleenvnode/BleViewModel.kt \
  android/BleEnvNode/app/src/main/java/com/bleenvnode/ui/DataAlertsScreen.kt

git commit -m "phase-9c: TinyML — phone-side validation + TFLite Micro edge inference on ESP32-C3"
```

---

## Phase C+ (Future) — Anomaly Detection

When you have sufficient "comfortable" labeled data (500+ samples), add an autoencoder:

1. Train autoencoder on comfortable data only: `3 → 8 → 3` architecture
2. Compute reconstruction error threshold on validation set (e.g., 95th percentile)
3. During inference: run classifier first; if reconstruction error > threshold, override class with `ML_CLASS_ANOMALY` (5)
4. Notify `b7e00007` with class=5

This can be added to `tinyml_inference.c` without changing the `b7e00007` characteristic payload format.
