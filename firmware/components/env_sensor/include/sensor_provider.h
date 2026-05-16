#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    int16_t temperature_c_x100;
    uint16_t humidity_pct_x100;
    uint32_t pressure_pa;
    bool valid;
    bool simulated;
} sensor_sample_t;

esp_err_t sensor_provider_init(void);
sensor_sample_t sensor_provider_read(void);
