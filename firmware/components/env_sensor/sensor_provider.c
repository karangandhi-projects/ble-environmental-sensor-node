#include "sensor_provider.h"
#include "esp_timer.h"

esp_err_t sensor_provider_init(void)
{
    return ESP_OK;
}

sensor_sample_t sensor_provider_read(void)
{
    int64_t t = esp_timer_get_time() / 1000000;
    sensor_sample_t sample = {
        .temperature_c_x100 = (int16_t)(2450 + (t % 20)),
        .humidity_pct_x100 = (uint16_t)(5200 + (t % 50)),
        .pressure_pa = (uint32_t)(101325 + (t % 100)),
        .valid = true,
        .simulated = true,
    };
    return sample;
}
