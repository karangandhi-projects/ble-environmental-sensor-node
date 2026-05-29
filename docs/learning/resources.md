# Learning Resources

Reference material for the technologies used in this project. Organised by domain.

---

## ESP32 / ESP-IDF

### Official Documentation

- **ESP-IDF Programming Guide (v5.2)**
  https://docs.espressif.com/projects/esp-idf/en/v5.2.3/
  The definitive reference. Key sections for this project:
  - API Reference → Bluetooth → NimBLE
  - API Reference → Storage → NVS Flash
  - API Reference → Power Management (`esp_pm`)
  - API Reference → Timer → ESP Timer
  - API Reference → Peripherals → I2C

- **ESP32-C3 Technical Reference Manual**
  https://www.espressif.com/sites/default/files/documentation/esp32-c3_technical_reference_manual_en.pdf
  Deep hardware reference: memory map, peripheral register layout, power domains.

- **Espressif GitHub Organisation**
  https://github.com/espressif
  All official SDKs, component libraries, and tooling.

### ESP-IDF Examples (primary code inspiration)

The firmware structure in this project was directly inspired by these official examples:

- **`examples/bluetooth/nimble/blehr`** — heart-rate BLE peripheral
  The pattern for `gatt_access_cb`, `gap_event_cb`, `advertise()`, and the NimBLE host task structure all derive from here.
  https://github.com/espressif/esp-idf/tree/v5.2.3/examples/bluetooth/nimble/blehr

- **`examples/bluetooth/nimble/bleprph`** — BLE peripheral with security
  Shows NimBLE SM configuration (`sm_sc`, `sm_bonding`, `sm_mitm`), repeat-pairing handler, and bond store init.
  https://github.com/espressif/esp-idf/tree/v5.2.3/examples/bluetooth/nimble/bleprph

- **`examples/bluetooth/nimble/`** — full NimBLE example directory
  Contains `gatt_server_service_table`, `gatt_client`, `ble_spp_client`, and others. The `gatt_server_service_table` example shows UUID definition macros (`BLE_UUID128_INIT`), characteristic/descriptor table structure, and `ble_gatts_count_cfg` / `ble_gatts_add_svcs` pattern.
  https://github.com/espressif/esp-idf/tree/v5.2.3/examples/bluetooth/nimble

- **`examples/storage/nvs_rw_value`** — NVS read/write
  The `nvs_open → nvs_get_u32 → nvs_commit` pattern used in `storage_config.c`.
  https://github.com/espressif/esp-idf/tree/v5.2.3/examples/storage/nvs_rw_value

- **`examples/system/unit_test`** — Unity on-target testing
  The `firmware/test_app/` structure and `UNITY_TEST_RUNNER` setup are modelled on this.
  https://github.com/espressif/esp-idf/tree/v5.2.3/examples/system/unit_test

### NimBLE Upstream

- **Apache NimBLE GitHub** (the BLE stack ESP-IDF bundles)
  https://github.com/apache/mynewt-nimble
  When the ESP-IDF docs are incomplete, search the upstream source for struct definitions (`ble_gap_event`, `ble_gatt_access_ctxt`, etc.) and `MODLOG_DEBUG` level constants.

---

## BLE Protocol & GATT

- **Bluetooth Core Specification** (free download)
  https://www.bluetooth.com/specifications/specs/core-specification-5-4/
  Volumes 1–3 for architecture; Vol 3 Part G (GATT) and Part H (ATT) are most relevant.
  Vol 3 Part C Section 9 covers GAP advertising.

- **Bluetooth Assigned Numbers** — UUID registry, company IDs, appearance values
  https://www.bluetooth.com/specifications/assigned-numbers/

- **Bluetooth GATT Characteristics browser** — standard characteristic UUIDs and formats
  https://www.bluetooth.com/specifications/gatt/

- **Bluetooth SIG Developer Blog** — articles on GATT, ATT, security, and BLE architecture by the Bluetooth SIG team
  https://www.bluetooth.com/blog/a-developers-guide-to-bluetooth/
  A good entry point; search the site for "GATT", "security", and "BLE data transfer" for deeper articles.

---

## TinyML on Embedded

### Core Concepts

- **TinyML: Machine Learning with TensorFlow Lite on Arduino and Ultra-Low-Power Microcontrollers**
  Pete Warden & Daniel Situnayake — O'Reilly, 2019
  The foundational text. Chapters 1–4 cover MLP forward-pass mechanics and quantization.
  The "why pure C over TFLite Micro" decision (DD-018) was informed by chapter 14.

- **TensorFlow Lite for Microcontrollers** — official guide
  https://www.tensorflow.org/lite/microcontrollers
  Explains the TFLite flatbuffer format, interpreter API, and operator support.
  This project chose NOT to use TFLite Micro (245 weights doesn't need the overhead) but this is the path for larger models.

- **TFLite Micro GitHub** — reference implementations
  https://github.com/tensorflow/tflite-micro
  Useful for understanding quantization types (int8 vs float32), model conversion workflow,
  and the reference operator kernels that a pure-C implementation must match.

### Tools Used for This Project's ML Pipeline

- **NumPy** — `ml/collect_synthetic.py` generates training data with NumPy random distributions
  https://numpy.org/doc/stable/

- **scikit-learn** — `MLPClassifier` used for training; `train_test_split`, `accuracy_score`
  https://scikit-learn.org/stable/modules/neural_networks_supervised.html
  The MLP implementation in `tinyml_inference.c` replicates scikit-learn's default hidden-layer activation (ReLU) and output layer (softmax).

- **Edge Impulse** — commercial TinyML toolchain (not used here but worth knowing)
  https://edgeimpulse.com
  Handles data collection, feature engineering, training, quantization, and deployment to embedded targets in one platform. Good alternative for real sensor data.

### Learning Courses

- **HarvardX TinyML Series (edX)** — Fundamentals, Applications, Deploying
  https://www.edx.org/professional-certificate/harvardx-tiny-machine-learning
  Best free/low-cost structured course for embedded ML. Covers the full pipeline from Keras training to deployment.

- **Fast.ai "Practical Deep Learning"** — Part 1 is excellent for building intuition about MLPs and softmax before the embedded course
  https://course.fast.ai/

---

## Android BLE + Jetpack Compose

### Android BLE

- **Android BLE guide** — official developer docs
  https://developer.android.com/develop/connectivity/bluetooth/ble/ble-overview
  Covers `BluetoothLeScanner`, `BluetoothGatt`, GATT callbacks, and permissions model.

- **Android connectivity samples** — official GitHub with BLE examples
  https://github.com/android/connectivity-samples
  The `BluetoothLeGatt` sample was the starting point for the GATT client in this project's Android app. Look at `BluetoothLeService.kt` for the pattern.

- **Android GATT characteristic write patterns** — particularly the sequential CCCD write pattern used in Phase 9A
  https://developer.android.com/develop/connectivity/bluetooth/ble/transfer-ble-data
  This project ran into the "write one CCCD at a time" issue (see Phase 9B fix commit) that this page documents but doesn't make obvious.

### Jetpack Compose

- **Jetpack Compose** — official developer docs
  https://developer.android.com/jetpack/compose

- **Compose State guide** — `remember`, `mutableStateOf`, `collectAsState` patterns used throughout the app
  https://developer.android.com/develop/ui/compose/state

- **ViewModel + StateFlow guide** — the `BleViewModel` in this project follows this pattern
  https://developer.android.com/topic/libraries/architecture/viewmodel

- **Material 3 components** — `Card`, `Slider`, `Switch`, `OutlinedButton` used in the app
  https://m3.material.io/components

### Kotlin Coroutines (used for BLE scan + connect)

- **Coroutines guide**
  https://kotlinlang.org/docs/coroutines-overview.html

- **Flow guide** — `SharedFlow` and `StateFlow` used for BLE event streams
  https://kotlinlang.org/docs/flow.html

---

## FreeRTOS

- **FreeRTOS documentation** — tasks, queues, semaphores, timers
  https://www.freertos.org/Documentation/RTOS_book.html
  This project uses `xTaskCreate`, `vTaskDelay`, `portMUX_TYPE` spinlocks. The task model section is directly relevant.

- **ESP-IDF FreeRTOS notes** — ESP32 differences from vanilla FreeRTOS (SMP, tick rate, WDT)
  https://docs.espressif.com/projects/esp-idf/en/v5.2.3/api-reference/system/freertos.html

---

## SSD1306 OLED + I2C

- **SSD1306 datasheet** — command set, initialisation sequence, memory addressing modes
  https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf
  The `ssd1306_init()` command sequence in `display/ssd1306.c` follows section 3 of this datasheet.

- **ESP-IDF I2C driver docs** — `i2c_param_config`, `i2c_master_write_to_device`
  https://docs.espressif.com/projects/esp-idf/en/v5.2.3/api-reference/peripherals/i2c.html
  Note: this project uses the deprecated `driver/i2c.h` API (the boot warning about this is expected). The new API is `driver/i2c_master.h`.

---

## Code Structure Inspirations

| Pattern | Source |
|---|---|
| Multi-component ESP-IDF layout (`firmware/components/`) | ESP-IDF component model — `idf.py create-component` convention |
| `gatt_access_cb` / `gap_event_cb` structure | `blehr` + `bleprph` NimBLE examples |
| `put_le16()` / `get_le16()` payload encoding | ESP-IDF BLE host internals (`nimble/host/include/host/ble_hs.h`) |
| `portMUX_TYPE` spinlock for shared state | ESP-IDF FreeRTOS examples (SMP-safe shared variable pattern) |
| Unity on-target tests via `unity_test_app` | ESP-IDF `examples/system/unit_test` |
| `APP_ERROR_CHECK()` / `ESP_ERROR_CHECK()` chaining | ESP-IDF error handling conventions throughout official examples |
| Kotlin ViewModel + StateFlow + Compose | Google's Now in Android sample (`https://github.com/android/nowinandroid`) |
| scikit-learn → pure-C weight export | TinyML book chapter 14 + Pete Warden's blog on model deployment |

---

## Further Reading

- **"Making Embedded Systems" — Elecia White** (O'Reilly)
  Best book on embedded firmware design. Chapter 4 (outputs), Chapter 7 (state machines), and Chapter 9 (peripherals) map directly to this project's architecture.

- **"Programming Embedded Systems" — Michael Barr & Anthony Massa** (O'Reilly)
  Good companion for understanding C on bare-metal, memory layout, and interrupt handling.

- **Bluetooth Developer Resources** — tools, SDKs, and qualification resources from the Bluetooth SIG
  https://www.bluetooth.com/develop-with-bluetooth/
