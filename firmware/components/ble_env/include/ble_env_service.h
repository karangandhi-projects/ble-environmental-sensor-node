#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "sensor_provider.h"

esp_err_t ble_env_service_init(void);
esp_err_t ble_env_service_notify_telemetry(const sensor_sample_t *sample, uint16_t sequence);
esp_err_t ble_env_service_notify_status(void);
esp_err_t ble_env_service_notify_ml_alert(uint8_t ml_class, uint8_t confidence);
