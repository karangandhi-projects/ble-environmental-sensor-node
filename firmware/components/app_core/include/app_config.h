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

/* Phase 7: advertising interval. NimBLE unit = 0.625 ms. */
#define BLE_ENV_ADV_ITVL_MS              250
#define BLE_ENV_ADV_ITVL_UNITS           ((BLE_ENV_ADV_ITVL_MS * 8) / 5)   /* = 400 */

/* Phase 7: preferred connection interval. NimBLE unit = 1.25 ms. */
#define BLE_ENV_CONN_ITVL_MIN_UNITS      400    /* 500 ms */
#define BLE_ENV_CONN_ITVL_MAX_UNITS      800    /* 1000 ms */
#define BLE_ENV_CONN_LATENCY             0
#define BLE_ENV_CONN_SUPERVISION_UNITS   400    /* 4000 ms */

/* Phase 7: control characteristic power-mode opcodes. */
#define BLE_ENV_CMD_FORCE_SAMPLE         0x10
#define BLE_ENV_CMD_SET_POWER_MODE       0x20
#define BLE_ENV_POWER_MODE_ACTIVE        0x00
#define BLE_ENV_POWER_MODE_LIGHT_SLEEP   0x01
#define BLE_ENV_POWER_MODE_DEEP_SLEEP    0x02
#define BLE_ENV_DEEP_SLEEP_DURATION_US   (30ULL * 1000000ULL)  /* 30 s wakeup */

/* Phase 7: control characteristic display opcodes. */
#define BLE_ENV_CMD_SET_DISPLAY          0x30
#define BLE_ENV_DISPLAY_OFF              0x00
#define BLE_ENV_DISPLAY_ON               0x01
#define BLE_ENV_DISPLAY_DIM              0x02

/* Phase 7: config characteristic flags. */
#define BLE_ENV_CONFIG_FLAG_DISPLAY_OFF  (1u << 1)  /* display off by default on boot */

/* Phase 9C: TinyML alert classes (b7e00007 ML Alert characteristic). */
#define BLE_ENV_ML_CLASS_COMFORTABLE  0
#define BLE_ENV_ML_CLASS_WARM         1
#define BLE_ENV_ML_CLASS_COLD         2
#define BLE_ENV_ML_CLASS_HUMID        3
#define BLE_ENV_ML_CLASS_DANGER       4
#define BLE_ENV_ML_CLASS_ANOMALY      5
