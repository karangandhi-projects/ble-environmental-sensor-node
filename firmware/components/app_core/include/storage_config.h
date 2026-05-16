#pragma once

#include <stdint.h>
#include "esp_err.h"

typedef struct {
    uint8_t version;
    uint8_t flags;
    uint16_t report_interval_ms;
} storage_config_t;

esp_err_t storage_config_init(void);
storage_config_t storage_config_load_or_default(void);
esp_err_t storage_config_save(const storage_config_t *config);
