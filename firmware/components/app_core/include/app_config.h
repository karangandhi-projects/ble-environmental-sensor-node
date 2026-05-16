#pragma once

#define BLE_ENV_DEVICE_NAME              "BLE_ENV_NODE"
#define BLE_ENV_DEFAULT_INTERVAL_MS      2000
#define BLE_ENV_MIN_INTERVAL_MS          500
#define BLE_ENV_MAX_INTERVAL_MS          60000

#define BLE_ENV_TELEMETRY_VERSION        1
#define BLE_ENV_CONFIG_VERSION           1

#define BLE_ENV_FLAG_SENSOR_VALID        (1u << 0)
#define BLE_ENV_FLAG_SIMULATED_DATA      (1u << 1)
#define BLE_ENV_FLAG_LOW_BATTERY         (1u << 2)

/* OLED display (added in Phase 1.5). */
#define BLE_ENV_I2C_SDA_GPIO             5
#define BLE_ENV_I2C_SCL_GPIO             6
#define BLE_ENV_I2C_FREQ_HZ              400000
#define BLE_ENV_OLED_I2C_ADDR            0x3C
#define BLE_ENV_OLED_WIDTH               72
#define BLE_ENV_OLED_HEIGHT              40
#define BLE_ENV_OLED_X_OFFSET            28
