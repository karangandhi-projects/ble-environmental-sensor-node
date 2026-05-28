/**
 * @file tinyml_inference.h
 * @brief Pure-C environmental condition classifier for ESP32-C3.
 *
 * Implements a 3→16→8→5 MLP (Multi-Layer Perceptron) that classifies
 * temperature / humidity / pressure readings into one of five environmental
 * classes, plus an anomaly class for uncertain inputs.
 *
 * The model weights are embedded in ml_weights.h at compile time — no
 * external runtime (TFLite Micro, ONNX, etc.) is required. The full forward
 * pass fits in ~70 lines of pure C and adds ~5KB to the binary.
 *
 * @par Inference pipeline
 * @code
 * tinyml_infer(temp, hum, press)
 *   -> normalize inputs to [0,1]
 *   -> Dense(16, ReLU)  [ML_W1, ML_b1]
 *   -> Dense(8,  ReLU)  [ML_W2, ML_b2]
 *   -> Dense(5, softmax)[ML_W3, ML_b3]
 *   -> if max_prob < 0.50 => ML_CLASS_ANOMALY
 *   -> else => class with highest probability
 * @endcode
 *
 * @par Training
 * Model trained with Keras (train_classifier.py) on 1879 samples:
 * 1500 synthetic + 379 real device readings with ±2°C/±2%/±2hPa drift.
 * Test accuracy: 99.7%.
 *
 * @note This component is called from telemetry_task in app_main.c on every
 *       sensor cycle. The result is broadcast over BLE characteristic b7e00007
 *       (ML Alert) only when the class changes.
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"

/**
 * @brief Environmental classification result codes.
 *
 * Matches BLE_ENV_ML_CLASS_* defines in app_config.h and the CLASS_ORDER
 * list in ml/train_classifier.py — do not reorder without retraining.
 */
typedef enum {
    ML_CLASS_COMFORTABLE = 0, /**< 18–25°C, 30–60% RH, 1000–1025 hPa */
    ML_CLASS_WARM        = 1, /**< 28–45°C, 20–50% RH, 990–1020 hPa  */
    ML_CLASS_COLD        = 2, /**< -5–15°C, 20–50% RH, 985–1015 hPa  */
    ML_CLASS_HUMID       = 3, /**< 18–28°C, 75–99% RH, 995–1020 hPa  */
    ML_CLASS_DANGER      = 4, /**< 46–60°C, 0–20% RH,  970–995 hPa   */
    ML_CLASS_ANOMALY     = 5, /**< Input doesn't match any class (max softmax < 50%) */
} ml_class_t;

/**
 * @brief Result of a single inference call.
 */
typedef struct {
    ml_class_t class_id;   /**< Predicted class (0–5). */
    uint8_t    confidence; /**< Softmax max × 100, i.e. 0–100. For anomaly: (1 - max_prob) × 100. */
} ml_result_t;

/**
 * @brief Initialise the inference engine and log a readiness message.
 *
 * Currently a lightweight log-only call — no heap allocation, no I2C, no
 * flash access. Call once from app_main() after sensor_provider_init().
 *
 * @return ESP_OK always.
 */
esp_err_t tinyml_inference_init(void);

/**
 * @brief Run one forward pass of the MLP classifier.
 *
 * Normalises the three inputs, runs two ReLU hidden layers, applies softmax
 * to the output layer, and returns the winning class. If the winning class
 * has probability < 0.50, returns ML_CLASS_ANOMALY instead.
 *
 * This function is pure (no side effects, no globals written) and re-entrant.
 * Execution time on ESP32-C3 at 160 MHz is <50 µs.
 *
 * @param temp_c        Temperature in degrees Celsius (e.g. 22.5).
 * @param humidity_pct  Relative humidity in percent (e.g. 55.0).
 * @param pressure_hpa  Barometric pressure in hPa (e.g. 1013.0).
 * @return              ml_result_t with class_id and confidence (0–100).
 */
ml_result_t tinyml_infer(float temp_c, float humidity_pct, float pressure_hpa);
