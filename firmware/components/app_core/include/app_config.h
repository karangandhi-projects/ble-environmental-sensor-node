/**
 * @file app_config.h
 * @brief Compile-time configuration constants for BLE_ENV_NODE.
 *
 * All numeric constants that appear in more than one source file live here.
 * Grouped by the phase that introduced them so the history is traceable.
 * Values that are exposed over BLE (versions, flags, opcodes) are frozen —
 * change them only with explicit user approval and a gatt_profile.md update.
 *
 * @note Do not include implementation headers here — this file is included
 *       by nearly every component and must remain dependency-free.
 */
#pragma once

/** @defgroup ble_identity BLE Device Identity
 *  @{
 */
#define BLE_ENV_DEVICE_NAME         "BLE_ENV_NODE" /**< Advertised name; 12 bytes fits in 31-byte adv payload. */
/** @} */

/** @defgroup telemetry_config Telemetry Reporting
 *  @{
 */
#define BLE_ENV_DEFAULT_INTERVAL_MS  2000  /**< Default notification period in ms. */
#define BLE_ENV_MIN_INTERVAL_MS       500  /**< Fastest allowed notification rate. */
#define BLE_ENV_MAX_INTERVAL_MS     60000  /**< Slowest allowed notification rate. */
/** @} */

/** @defgroup payload_versions BLE Payload Version Bytes
 *  Increment these when the byte layout of a characteristic changes.
 *  @{
 */
#define BLE_ENV_TELEMETRY_VERSION    1  /**< Telemetry frame version (byte 0 of b7e00002). */
#define BLE_ENV_CONFIG_VERSION       1  /**< Config frame version (byte 0 of b7e00004).    */
/** @} */

/** @defgroup telemetry_flags Telemetry Flags Bitmask (byte 1 of b7e00002)
 *  @{
 */
#define BLE_ENV_FLAG_SENSOR_VALID    (1u << 0) /**< Set when sensor reading is trustworthy. */
#define BLE_ENV_FLAG_SIMULATED_DATA  (1u << 1) /**< Set for simulated or override data; drives OLED SIM badge. */
#define BLE_ENV_FLAG_LOW_BATTERY     (1u << 2) /**< Reserved — set when battery voltage drops below threshold. */
/** @} */

/** @defgroup oled_config OLED Display (Phase 1.5)
 *  SSD1306 on I2C_NUM_0. The physical panel is 128×64 but only a 72×40
 *  window starting at column 28 is visible through the bezel.
 *  @{
 */
#define BLE_ENV_I2C_SDA_GPIO     5        /**< I2C SDA pin (GPIO 5). */
#define BLE_ENV_I2C_SCL_GPIO     6        /**< I2C SCL pin (GPIO 6). */
#define BLE_ENV_I2C_FREQ_HZ      400000   /**< I2C bus speed: 400 kHz (fast mode). */
#define BLE_ENV_OLED_I2C_ADDR    0x3C     /**< SSD1306 I2C address (SA0 pin = GND). */
#define BLE_ENV_OLED_WIDTH       72       /**< Visible panel width in pixels. */
#define BLE_ENV_OLED_HEIGHT      40       /**< Visible panel height in pixels. */
#define BLE_ENV_OLED_X_OFFSET    28       /**< Column offset inside 128-wide controller buffer. */
/** @} */

/** @defgroup adv_config Advertising Interval (Phase 7)
 *  NimBLE uses 0.625 ms units for advertising.
 *  @{
 */
#define BLE_ENV_ADV_ITVL_MS          250  /**< Advertising interval in ms (halves default ~100 ms duty cycle). */
#define BLE_ENV_ADV_ITVL_UNITS       ((BLE_ENV_ADV_ITVL_MS * 8) / 5)  /**< = 400 NimBLE units (0.625 ms each). */
/** @} */

/** @defgroup control_opcodes BLE Control Characteristic Opcodes (b7e00003, Phase 7)
 *  2-byte payload: [opcode, value]. Writes require an encrypted link.
 *  @{
 */
#define BLE_ENV_CMD_LED_OFF           0x01  /**< Turn LED off; value byte ignored. */
#define BLE_ENV_CMD_LED_ON            0x02  /**< Turn LED on; value byte ignored. */
#define BLE_ENV_CMD_LED_TOGGLE        0x03  /**< Toggle LED state; value byte ignored. */
#define BLE_ENV_CMD_FORCE_SAMPLE      0x10  /**< Force immediate telemetry sample; value ignored. */
#define BLE_ENV_CMD_SET_POWER_MODE    0x20  /**< Set power mode; value = BLE_ENV_POWER_MODE_*. */
#define BLE_ENV_POWER_MODE_ACTIVE     0x00  /**< Full speed; cancel any pending sleep. */
#define BLE_ENV_POWER_MODE_LIGHT_SLEEP 0x01 /**< CPU sleeps between BLE events; connection maintained. */
#define BLE_ENV_POWER_MODE_DEEP_SLEEP 0x02  /**< Disconnect BLE, sleep 30 s, re-advertise on wake. */
#define BLE_ENV_DEEP_SLEEP_DURATION_US (30ULL * 1000000ULL) /**< Deep sleep wake timer: 30 seconds. */
#define BLE_ENV_CMD_SET_DISPLAY       0x30  /**< Set display state; value = BLE_ENV_DISPLAY_*. */
#define BLE_ENV_DISPLAY_OFF           0x00  /**< SSD1306 DISPLAYOFF: ~20 µA panel current. */
#define BLE_ENV_DISPLAY_ON            0x01  /**< SSD1306 DISPLAYON: restores last frame. */
#define BLE_ENV_DISPLAY_DIM           0x02  /**< Minimum contrast; panel remains on. */
/** @} */

/** @defgroup config_flags Config Characteristic Flags (b7e00004 byte 1)
 *  @{
 */
#define BLE_ENV_CONFIG_FLAG_DISPLAY_OFF (1u << 1) /**< Display off by default on boot (persisted in NVS). */
/** @} */

/** @defgroup ml_classes TinyML Alert Classes (b7e00007 ML Alert, Phase 9C)
 *  Byte 0 of the ML Alert notification payload. Order must match CLASS_ORDER
 *  in ml/train_classifier.py and ml_class_t in tinyml_inference.h.
 *  @{
 */
#define BLE_ENV_ML_CLASS_COMFORTABLE  0 /**< 18–25°C, 30–60% RH, 1000–1025 hPa. */
#define BLE_ENV_ML_CLASS_WARM         1 /**< 28–45°C, 20–50% RH,  990–1020 hPa. */
#define BLE_ENV_ML_CLASS_COLD         2 /**< -5–15°C, 20–50% RH,  985–1015 hPa. */
#define BLE_ENV_ML_CLASS_HUMID        3 /**< 18–28°C, 75–99% RH,  995–1020 hPa. */
#define BLE_ENV_ML_CLASS_DANGER       4 /**< 46–60°C,  0–20% RH,  970–995 hPa.  */
#define BLE_ENV_ML_CLASS_ANOMALY      5 /**< Input doesn't match any class (max softmax < 50%). */
/** @} */
