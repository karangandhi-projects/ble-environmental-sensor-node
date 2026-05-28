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

/* Override simulated values from BLE. pressure_hpa_x10: pressure in 0.1 hPa units
 * (e.g. 10132 = 1013.2 hPa). Passing all-zeros to set_override is valid; use
 * clear_override to return to random simulation. */
void sensor_provider_set_override(int16_t temp_cdeg,
                                   uint16_t humidity_cpct,
                                   uint16_t pressure_hpa_x10);
void sensor_provider_clear_override(void);
