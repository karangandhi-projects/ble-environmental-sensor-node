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

esp_err_t   tinyml_inference_init(void);
ml_result_t tinyml_infer(float temp_c, float humidity_pct, float pressure_hpa);
