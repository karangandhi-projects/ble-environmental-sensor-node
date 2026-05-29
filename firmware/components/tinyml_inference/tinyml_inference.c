/**
 * @file tinyml_inference.c
 * @brief Pure-C forward pass for the 3→16→8→5 environmental MLP classifier.
 *
 * This file implements the entire inference pipeline in ~80 lines of C with
 * no external dependencies. Weights are compiled in from ml_weights.h.
 *
 * @par Why pure C instead of TFLite Micro?
 * tensorflow/lite-micro was not available in the ESP-IDF v5.2.3 component
 * registry. The model is architecturally tiny (245 weights), so the overhead
 * of a full ML runtime is not justified. See DD-018 in design_decisions.md.
 *
 * @par Anomaly detection
 * Rather than a separate autoencoder (which was tried and failed — it flagged
 * all non-comfortable classes as anomalies), anomaly is detected by the
 * classifier itself: if max softmax < 0.50, the input doesn't fit any trained
 * class and ML_CLASS_ANOMALY is returned. See DD-019 in design_decisions.md.
 */
#include "tinyml_inference.h"
#include "ml_weights.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "tinyml";

esp_err_t tinyml_inference_init(void)
{
    ESP_LOGI(TAG, "Pure-C MLP inference ready (3→16→8→5, anomaly threshold: confidence < 50%%)");
    return ESP_OK;
}

/**
 * @brief Compute one dense (fully-connected) layer.
 *
 * Implements: out[i] = b[i] + sum_{j=0}^{in_size-1}( W[i*in_size + j] * in[j] )
 *
 * W is stored row-major with shape [out_size × in_size], matching the
 * transposed weight extraction in ml/extract_weights.py (model.layers[k].get_weights()[0].T).
 *
 * @param in        Input vector (length in_size).
 * @param in_size   Number of input features.
 * @param W         Weight matrix, row-major [out_size × in_size].
 * @param b         Bias vector (length out_size).
 * @param out       Output vector to fill (length out_size).
 * @param out_size  Number of output neurons.
 */
static void dense(const float *in, int in_size,
                  const float *W, const float *b,
                  float *out, int out_size)
{
    for (int i = 0; i < out_size; i++) {
        float acc = b[i];
        for (int j = 0; j < in_size; j++) {
            acc += W[i * in_size + j] * in[j];
        }
        out[i] = acc;
    }
}

/**
 * @brief Apply ReLU activation in-place: x[i] = max(0, x[i]).
 *
 * ReLU (Rectified Linear Unit) is the standard activation for hidden layers.
 * It avoids the vanishing-gradient problem of sigmoid/tanh and executes as a
 * simple comparison — no transcendental functions needed.
 *
 * @param x  Vector to modify in place.
 * @param n  Length of the vector.
 */
static void relu(float *x, int n)
{
    for (int i = 0; i < n; i++) {
        if (x[i] < 0.0f) x[i] = 0.0f;
    }
}

ml_result_t tinyml_infer(float temp_c, float humidity_pct, float pressure_hpa)
{
    /* --- Input range validation ---
     * Training ranges: temp [-10, 60]°C, humidity [0, 100]%, pressure [900, 1100] hPa.
     * Out-of-range inputs still produce a result but accuracy is not guaranteed.
     * Humidity is clamped (physical bounds); temperature and pressure are not.
     */
    if (temp_c < -10.0f || temp_c > 60.0f) {
        ESP_LOGW(TAG, "temp %.1f°C outside training range [-10, 60]; result may be unreliable", temp_c);
    }
    if (humidity_pct < 0.0f || humidity_pct > 100.0f) {
        ESP_LOGW(TAG, "humidity %.1f%% outside [0, 100]; clamping", humidity_pct);
        humidity_pct = humidity_pct < 0.0f ? 0.0f : 100.0f;
    }
    if (pressure_hpa < 900.0f || pressure_hpa > 1100.0f) {
        ESP_LOGW(TAG, "pressure %.0f hPa outside training range [900, 1100]; result may be unreliable", pressure_hpa);
    }

    /* --- Normalize inputs to [0,1] ---
     * Ranges match the NORM dict in ml/train_classifier.py. Without this,
     * temperature (range ~70) would dominate humidity (range 100) and pressure
     * (range 200) in the dot products, producing a biased model.
     *   temp:  [-10, 60]  → divide by (60 - (-10)) = 70
     *   hum:   [0,  100]  → divide by 100
     *   press: [900, 1100] → divide by 200
     */
    float input[ML_INPUT_SIZE] = {
        (temp_c       - (-10.0f)) / 70.0f,
        (humidity_pct -   0.0f)  / 100.0f,
        (pressure_hpa - 900.0f)  / 200.0f,
    };

    float h1[ML_LAYER1_SIZE];
    float h2[ML_LAYER2_SIZE];
    float out[ML_OUTPUT_SIZE];

    /* --- Forward pass: two ReLU hidden layers + linear output --- */
    dense(input, ML_INPUT_SIZE,  ML_W1, ML_b1, h1, ML_LAYER1_SIZE);
    relu(h1, ML_LAYER1_SIZE);

    dense(h1, ML_LAYER1_SIZE, ML_W2, ML_b2, h2, ML_LAYER2_SIZE);
    relu(h2, ML_LAYER2_SIZE);

    dense(h2, ML_LAYER2_SIZE, ML_W3, ML_b3, out, ML_OUTPUT_SIZE);

    /* --- Softmax: convert raw scores to probabilities summing to 1 ---
     * Subtract max_val before exp() for numerical stability — prevents
     * overflow on large positive logits without changing the result
     * (softmax is invariant to constant shifts in the exponent).
     */
    float max_val = out[0];
    for (int i = 1; i < ML_OUTPUT_SIZE; i++) {
        if (out[i] > max_val) max_val = out[i];
    }
    float sum = 0.0f;
    for (int i = 0; i < ML_OUTPUT_SIZE; i++) {
        out[i] = expf(out[i] - max_val);
        sum += out[i];
    }
    for (int i = 0; i < ML_OUTPUT_SIZE; i++) out[i] /= sum;

    /* --- Argmax: find the class with highest probability --- */
    int best = 0;
    for (int i = 1; i < ML_OUTPUT_SIZE; i++) {
        if (out[i] > out[best]) best = i;
    }

    /* --- Anomaly detection via confidence threshold ---
     * If no class wins with > 50% probability, the input is in a boundary
     * region not well represented by any trained class. Return ANOMALY.
     * Confidence for anomaly = (1 - max_prob) × 100, representing "how
     * uncertain" the model is (higher = more anomalous).
     */
    if (out[best] < 0.50f) {
        uint8_t conf = (uint8_t)((1.0f - out[best]) * 100.0f);
        return (ml_result_t){ .class_id = ML_CLASS_ANOMALY, .confidence = conf };
    }

    return (ml_result_t){
        .class_id   = (ml_class_t)best,
        .confidence = (uint8_t)(out[best] * 100.0f),
    };
}
