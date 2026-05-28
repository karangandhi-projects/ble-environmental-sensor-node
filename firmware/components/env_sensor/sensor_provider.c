#include "sensor_provider.h"
#include "esp_timer.h"

static bool     s_override_active        = false;
static int16_t  s_override_temp_cdeg     = 0;
static uint16_t s_override_humidity_cpct = 0;
static uint16_t s_override_pressure_hpa_x10 = 0;

esp_err_t sensor_provider_init(void)
{
    return ESP_OK;
}

void sensor_provider_set_override(int16_t temp_cdeg,
                                   uint16_t humidity_cpct,
                                   uint16_t pressure_hpa_x10)
{
    s_override_temp_cdeg        = temp_cdeg;
    s_override_humidity_cpct    = humidity_cpct;
    s_override_pressure_hpa_x10 = pressure_hpa_x10;
    s_override_active           = true;
}

void sensor_provider_clear_override(void)
{
    s_override_active = false;
}

sensor_sample_t sensor_provider_read(void)
{
    if (s_override_active) {
        return (sensor_sample_t){
            .temperature_c_x100 = s_override_temp_cdeg,
            .humidity_pct_x100  = s_override_humidity_cpct,
            .pressure_pa        = (uint32_t)s_override_pressure_hpa_x10 * 10,
            .valid              = true,
            .simulated          = true,
        };
    }
    int64_t t = esp_timer_get_time() / 1000000;
    return (sensor_sample_t){
        .temperature_c_x100 = (int16_t)(2450 + (t % 20)),
        .humidity_pct_x100  = (uint16_t)(5200 + (t % 50)),
        .pressure_pa        = (uint32_t)(101325 + (t % 100)),
        .valid              = true,
        .simulated          = true,
    };
}
