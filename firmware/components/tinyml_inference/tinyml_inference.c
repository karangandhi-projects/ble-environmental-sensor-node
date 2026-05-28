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

/* Dense layer: out[i] = sum(W[i*in_size + j] * in[j]) + b[i] */
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

static void relu(float *x, int n)
{
    for (int i = 0; i < n; i++) {
        if (x[i] < 0.0f) x[i] = 0.0f;
    }
}

ml_result_t tinyml_infer(float temp_c, float humidity_pct, float pressure_hpa)
{
    /* Normalize to [0,1] matching training NORM constants */
    float input[ML_INPUT_SIZE] = {
        (temp_c       - (-10.0f)) / 70.0f,
        (humidity_pct -   0.0f)  / 100.0f,
        (pressure_hpa - 900.0f)  / 200.0f,
    };

    float h1[ML_LAYER1_SIZE];
    float h2[ML_LAYER2_SIZE];
    float out[ML_OUTPUT_SIZE];

    dense(input, ML_INPUT_SIZE,  ML_W1, ML_b1, h1, ML_LAYER1_SIZE);
    relu(h1, ML_LAYER1_SIZE);
    dense(h1, ML_LAYER1_SIZE, ML_W2, ML_b2, h2, ML_LAYER2_SIZE);
    relu(h2, ML_LAYER2_SIZE);
    dense(h2, ML_LAYER2_SIZE, ML_W3, ML_b3, out, ML_OUTPUT_SIZE);

    /* Softmax */
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

    /* Argmax */
    int best = 0;
    for (int i = 1; i < ML_OUTPUT_SIZE; i++) {
        if (out[i] > out[best]) best = i;
    }

    /* Anomaly: classifier is uncertain — input doesn't fit any known class */
    if (out[best] < 0.50f) {
        uint8_t conf = (uint8_t)((1.0f - out[best]) * 100.0f);
        return (ml_result_t){ .class_id = ML_CLASS_ANOMALY, .confidence = conf };
    }

    return (ml_result_t){
        .class_id   = (ml_class_t)best,
        .confidence = (uint8_t)(out[best] * 100.0f),
    };
}
