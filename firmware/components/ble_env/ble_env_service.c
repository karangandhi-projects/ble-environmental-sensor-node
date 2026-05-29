/**
 * @file ble_env_service.c
 * @brief NimBLE GATT service implementation — advertising, GATT, GAP, security.
 *
 * Owns the NimBLE host stack lifecycle, GATT service registration, and all
 * BLE event handling for BLE_ENV_NODE. Key responsibilities:
 *
 * @par GATT table (gatt_svcs[])
 * Six characteristics registered via ble_gatts_add_svcs(). Each characteristic
 * carries a 0x2901 User Description descriptor served by gatt_user_desc_cb().
 * The table is static const so it lives in flash after boot.
 *
 * @par gatt_access_cb()
 * Single callback dispatched for all characteristic reads and writes. Uses
 * ble_uuid_cmp() to route each access to the correct handler block. Returns
 * BLE_ATT_ERR_UNLIKELY for any unmatched access (should never happen if the
 * GATT table is correctly constructed).
 *
 * @par gap_event_cb()
 * Handles CONNECT, DISCONNECT, SUBSCRIBE, ENC_CHANGE, PASSKEY_ACTION, and
 * REPEAT_PAIRING. Advertising restarts automatically on disconnect unless
 * deep sleep was requested via opcode 0x20/0x02.
 *
 * @par Security
 * Just Works pairing with SC=1 (Secure Connections). Keys persisted in NVS
 * via ble_store_config_init(). ble_store_util_delete_peer() handles
 * re-pairing after a bond is deleted on the central side. See DD-008 and
 * docs/security_model.md.
 *
 * @par Deep sleep coordination
 * Opcode 0x20/0x02 (deep sleep) sets a 500 ms one-shot timer before
 * disconnecting, so the ATT write response reaches the central before the
 * link drops. Actual sleep is triggered from gap_event_cb() on disconnect.
 */
#include "ble_env_service.h"
#include "sensor_provider.h"
#include <string.h>
#include "app_config.h"
#include "app_state.h"
#include "storage_config.h"
#include "display.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/ble_gatt.h"
#include "host/ble_gap.h"
#include "host/ble_sm.h"
#include "host/ble_store.h"

void ble_store_config_init(void);
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

static const char *TAG = "ble_env";
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_telemetry_val_handle;
static uint16_t s_status_val_handle;
static esp_timer_handle_t s_deep_sleep_timer;
static uint16_t s_ml_alert_val_handle;
static bool     s_ml_alert_subscribed = false;

static const ble_uuid128_t ENV_SERVICE_UUID = BLE_UUID128_INIT(0x00,0x00,0x00,0x6c,0x6a,0x2f,0x7d,0x8b,0x2a,0x4c,0x4a,0x4f,0x01,0x00,0xe0,0xb7);
static const ble_uuid128_t TELEMETRY_UUID   = BLE_UUID128_INIT(0x00,0x00,0x00,0x6c,0x6a,0x2f,0x7d,0x8b,0x2a,0x4c,0x4a,0x4f,0x02,0x00,0xe0,0xb7);
static const ble_uuid128_t CONTROL_UUID     = BLE_UUID128_INIT(0x00,0x00,0x00,0x6c,0x6a,0x2f,0x7d,0x8b,0x2a,0x4c,0x4a,0x4f,0x03,0x00,0xe0,0xb7);
static const ble_uuid128_t CONFIG_UUID      = BLE_UUID128_INIT(0x00,0x00,0x00,0x6c,0x6a,0x2f,0x7d,0x8b,0x2a,0x4c,0x4a,0x4f,0x04,0x00,0xe0,0xb7);
static const ble_uuid128_t STATUS_UUID          = BLE_UUID128_INIT(0x00,0x00,0x00,0x6c,0x6a,0x2f,0x7d,0x8b,0x2a,0x4c,0x4a,0x4f,0x05,0x00,0xe0,0xb7);
static const ble_uuid128_t SENSOR_OVERRIDE_UUID = BLE_UUID128_INIT(0x00,0x00,0x00,0x6c,0x6a,0x2f,0x7d,0x8b,0x2a,0x4c,0x4a,0x4f,0x06,0x00,0xe0,0xb7);
static const ble_uuid128_t ML_ALERT_UUID        = BLE_UUID128_INIT(0x00,0x00,0x00,0x6c,0x6a,0x2f,0x7d,0x8b,0x2a,0x4c,0x4a,0x4f,0x07,0x00,0xe0,0xb7);


static void encode_telemetry(uint8_t out[16], const sensor_sample_t *sample, uint16_t sequence)
{
    uint8_t flags = 0;
    if (sample->valid) flags |= BLE_ENV_FLAG_SENSOR_VALID;
    if (sample->simulated) flags |= BLE_ENV_FLAG_SIMULATED_DATA;

    out[0] = BLE_ENV_TELEMETRY_VERSION;
    out[1] = flags;
    put_le16(&out[2], sequence);
    put_le32(&out[4], (uint32_t)(esp_timer_get_time() / 1000));
    put_le16(&out[8], (uint16_t)sample->temperature_c_x100);
    put_le16(&out[10], sample->humidity_pct_x100);
    put_le32(&out[12], sample->pressure_pa);
}

static void encode_status(uint8_t out[6])
{
    app_state_t s = app_state_get_snapshot();
    out[0] = (uint8_t)s.runtime_state;
    out[1] = (uint8_t)s.last_error;
    out[2] = s.connected ? 1 : 0;
    out[3] = s.telemetry_subscribed ? 1 : 0;
    out[4] = s.led_on ? 1 : 0;
    out[5] = s.sensor_valid ? 1 : 0;
}

static int gatt_user_desc_cb(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle; (void)attr_handle;
    const char *desc = (const char *)arg;
    return os_mbuf_append(ctxt->om, desc, strlen(desc)) == 0
           ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int gatt_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    const ble_uuid_t *uuid = ctxt->chr->uuid;

    if (ble_uuid_cmp(uuid, &TELEMETRY_UUID.u) == 0 && ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        sensor_sample_t sample = sensor_provider_read();
        uint8_t frame[16];
        encode_telemetry(frame, &sample, app_state_next_sequence());
        return os_mbuf_append(ctxt->om, frame, sizeof(frame)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    if (ble_uuid_cmp(uuid, &STATUS_UUID.u) == 0 && ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        uint8_t frame[6];
        encode_status(frame);
        return os_mbuf_append(ctxt->om, frame, sizeof(frame)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    if (ble_uuid_cmp(uuid, &CONFIG_UUID.u) == 0) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            app_state_t s = app_state_get_snapshot();
            uint8_t cfg[4] = { BLE_ENV_CONFIG_VERSION, 0, 0, 0 };
            put_le16(&cfg[2], s.report_interval_ms);
            return os_mbuf_append(ctxt->om, cfg, sizeof(cfg)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            uint8_t buf[4];
            uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
            if (len != sizeof(buf)) {
                ESP_LOGW(TAG, "config write: bad length %u (expected 4)", len);
                app_state_set_error(APP_ERROR_INVALID_CONFIG);
                return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            }
            ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), NULL);
            uint16_t interval = (uint16_t)buf[2] | ((uint16_t)buf[3] << 8);
            if (app_state_set_report_interval(interval) != ESP_OK) {
                ESP_LOGW(TAG, "config write: interval %u ms rejected (out of range)", interval);
                return BLE_ATT_ERR_UNLIKELY;
            }
            storage_config_t cfg = { .version = BLE_ENV_CONFIG_VERSION, .flags = buf[1], .report_interval_ms = interval };
            storage_config_save(&cfg);
            ESP_LOGI(TAG, "config: interval=%u ms", interval);
            return 0;
        }
    }

    if (ble_uuid_cmp(uuid, &CONTROL_UUID.u) == 0 && ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint8_t buf[2];
        uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
        if (len != sizeof(buf)) {
            ESP_LOGW(TAG, "control write: bad length %u (expected 2)", len);
            app_state_set_error(APP_ERROR_INVALID_COMMAND);
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), NULL);
        switch (buf[0]) {
            case 0x01:
                app_state_set_led(false);
                app_state_set_error(APP_ERROR_OK);
                ESP_LOGI(TAG, "LED off");
                break;
            case 0x02:
                app_state_set_led(true);
                app_state_set_error(APP_ERROR_OK);
                ESP_LOGI(TAG, "LED on");
                break;
            case 0x03:
                app_state_toggle_led();
                app_state_set_error(APP_ERROR_OK);
                ESP_LOGI(TAG, "LED toggle");
                break;
            case BLE_ENV_CMD_FORCE_SAMPLE:
                app_state_set_force_sample();
                app_state_set_error(APP_ERROR_OK);
                ESP_LOGI(TAG, "force sample requested");
                break;
            case BLE_ENV_CMD_SET_POWER_MODE:
                switch (buf[1]) {
                    case BLE_ENV_POWER_MODE_ACTIVE:
                        app_state_set_power_mode(POWER_MODE_ACTIVE);
                        app_state_set_error(APP_ERROR_OK);
                        ESP_LOGI(TAG, "power mode: active");
                        break;
                    case BLE_ENV_POWER_MODE_LIGHT_SLEEP:
                        app_state_set_power_mode(POWER_MODE_LIGHT_SLEEP);
                        app_state_set_error(APP_ERROR_OK);
                        ESP_LOGI(TAG, "power mode: light sleep");
                        break;
                    case BLE_ENV_POWER_MODE_DEEP_SLEEP:
                        app_state_request_deep_sleep();
                        app_state_set_error(APP_ERROR_OK);
                        ESP_LOGI(TAG, "power mode: deep sleep");
                        /* Disconnect after 500 ms so the write response completes first. */
                        esp_timer_start_once(s_deep_sleep_timer, 500 * 1000);
                        break;
                    default:
                        ESP_LOGW(TAG, "unknown power mode 0x%02x", buf[1]);
                        app_state_set_error(APP_ERROR_INVALID_COMMAND);
                        return BLE_ATT_ERR_UNLIKELY;
                }
                break;
            case BLE_ENV_CMD_SET_DISPLAY:
                switch (buf[1]) {
                    case BLE_ENV_DISPLAY_OFF:
                        display_set_power(DISPLAY_POWER_OFF);
                        app_state_set_display_on(false);
                        app_state_set_error(APP_ERROR_OK);
                        ESP_LOGI(TAG, "display off");
                        break;
                    case BLE_ENV_DISPLAY_ON:
                        display_set_power(DISPLAY_POWER_ON);
                        app_state_set_display_on(true);
                        app_state_set_error(APP_ERROR_OK);
                        ESP_LOGI(TAG, "display on");
                        break;
                    case BLE_ENV_DISPLAY_DIM:
                        display_set_power(DISPLAY_POWER_DIM);
                        app_state_set_display_on(true);
                        app_state_set_error(APP_ERROR_OK);
                        ESP_LOGI(TAG, "display dim");
                        break;
                    default:
                        ESP_LOGW(TAG, "unknown display mode 0x%02x", buf[1]);
                        app_state_set_error(APP_ERROR_INVALID_COMMAND);
                        return BLE_ATT_ERR_UNLIKELY;
                }
                break;
            default:
                ESP_LOGW(TAG, "unknown control opcode 0x%02x", buf[0]);
                app_state_set_error(APP_ERROR_INVALID_COMMAND);
                return BLE_ATT_ERR_UNLIKELY;
        }
        ble_env_service_notify_status();
        return 0;
    }

    if (ble_uuid_cmp(uuid, &SENSOR_OVERRIDE_UUID.u) == 0
        && ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint8_t buf[6];
        uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
        if (len != sizeof(buf)) {
            app_state_set_error(APP_ERROR_INVALID_COMMAND);
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), NULL);

        bool all_zeros = true;
        for (int i = 0; i < 6; i++) {
            if (buf[i] != 0) { all_zeros = false; break; }
        }

        if (all_zeros) {
            sensor_provider_clear_override();
        } else {
            int16_t  temp     = (int16_t) ((uint16_t)buf[0] | ((uint16_t)buf[1] << 8));
            uint16_t humidity = (uint16_t)((uint16_t)buf[2] | ((uint16_t)buf[3] << 8));
            uint16_t pressure = (uint16_t)((uint16_t)buf[4] | ((uint16_t)buf[5] << 8));
            sensor_provider_set_override(temp, humidity, pressure);
        }
        app_state_set_error(APP_ERROR_OK);
        ble_env_service_notify_status();
        return 0;
    }

    return BLE_ATT_ERR_UNLIKELY;
}

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &ENV_SERVICE_UUID.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid       = &TELEMETRY_UUID.u,
                .access_cb  = gatt_access_cb,
                .val_handle = &s_telemetry_val_handle,
                .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .descriptors = (struct ble_gatt_dsc_def[]) {
                    { .uuid = BLE_UUID16_DECLARE(0x2901), .att_flags = BLE_ATT_F_READ,
                      .access_cb = gatt_user_desc_cb, .arg = "Telemetry" },
                    { 0 }
                },
            },
            {
                .uuid      = &CONTROL_UUID.u,
                .access_cb = gatt_access_cb,
                .flags     = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC,
                .descriptors = (struct ble_gatt_dsc_def[]) {
                    { .uuid = BLE_UUID16_DECLARE(0x2901), .att_flags = BLE_ATT_F_READ,
                      .access_cb = gatt_user_desc_cb, .arg = "Control" },
                    { 0 }
                },
            },
            {
                .uuid      = &CONFIG_UUID.u,
                .access_cb = gatt_access_cb,
                .flags     = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE
                           | BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_WRITE_ENC,
                .descriptors = (struct ble_gatt_dsc_def[]) {
                    { .uuid = BLE_UUID16_DECLARE(0x2901), .att_flags = BLE_ATT_F_READ,
                      .access_cb = gatt_user_desc_cb, .arg = "Configuration" },
                    { 0 }
                },
            },
            {
                .uuid       = &STATUS_UUID.u,
                .access_cb  = gatt_access_cb,
                .val_handle = &s_status_val_handle,
                .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .descriptors = (struct ble_gatt_dsc_def[]) {
                    { .uuid = BLE_UUID16_DECLARE(0x2901), .att_flags = BLE_ATT_F_READ,
                      .access_cb = gatt_user_desc_cb, .arg = "Status" },
                    { 0 }
                },
            },
            {
                .uuid      = &SENSOR_OVERRIDE_UUID.u,
                .access_cb = gatt_access_cb,
                .flags     = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC,
                .descriptors = (struct ble_gatt_dsc_def[]) {
                    { .uuid = BLE_UUID16_DECLARE(0x2901), .att_flags = BLE_ATT_F_READ,
                      .access_cb = gatt_user_desc_cb, .arg = "Sensor Override" },
                    { 0 }
                },
            },
            {
                .uuid       = &ML_ALERT_UUID.u,
                .access_cb  = gatt_access_cb,
                .val_handle = &s_ml_alert_val_handle,
                .flags      = BLE_GATT_CHR_F_NOTIFY,
                .descriptors = (struct ble_gatt_dsc_def[]) {
                    { .uuid = BLE_UUID16_DECLARE(0x2901), .att_flags = BLE_ATT_F_READ,
                      .access_cb = gatt_user_desc_cb, .arg = "ML Alert" },
                    { 0 }
                },
            },
            { 0 }
        },
    },
    { 0 }
};

static void advertise(void);

static int gap_event_cb(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                s_conn_handle = event->connect.conn_handle;
                app_state_set_connected(true);
                ESP_LOGI(TAG, "Connected");
            } else {
                ESP_LOGW(TAG, "Connect failed; restarting advertising");
                advertise();
            }
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Disconnected (reason=0x%02x)", event->disconnect.reason);
            s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            app_state_set_connected(false);
            s_ml_alert_subscribed = false;
            if (app_state_get_deep_sleep_pending()) {
                app_state_clear_deep_sleep_pending();
                ESP_LOGI(TAG, "Entering deep sleep for 30 s");
                esp_sleep_enable_timer_wakeup(BLE_ENV_DEEP_SLEEP_DURATION_US);
                esp_deep_sleep_start();
            } else {
                advertise();
            }
            break;
        case BLE_GAP_EVENT_SUBSCRIBE:
            if (event->subscribe.attr_handle == s_telemetry_val_handle) {
                app_state_set_telemetry_subscribed(event->subscribe.cur_notify);
            } else if (event->subscribe.attr_handle == s_status_val_handle) {
                app_state_set_status_subscribed(event->subscribe.cur_notify);
            } else if (event->subscribe.attr_handle == s_ml_alert_val_handle) {
                s_ml_alert_subscribed = event->subscribe.cur_notify;
            }
            break;
        case BLE_GAP_EVENT_ENC_CHANGE:
            if (event->enc_change.status == 0) {
                ESP_LOGI(TAG, "Encryption established (conn %u)",
                         event->enc_change.conn_handle);
            } else {
                ESP_LOGW(TAG, "Encryption failed (conn %u, status %d)",
                         event->enc_change.conn_handle, event->enc_change.status);
            }
            return 0;
        case BLE_GAP_EVENT_PASSKEY_ACTION: {
            struct ble_sm_io pkey = {0};
            if (event->passkey.params.action == BLE_SM_IOACT_NUMCMP) {
                /* SC Numeric Comparison: peripheral auto-accepts; Android shows the same
                 * 6-digit code and the user taps Yes to confirm. */
                ESP_LOGI(TAG, "Numeric Comparison: %06lu — tap Yes on phone if it matches",
                         (unsigned long)event->passkey.params.numcmp);
                pkey.action = BLE_SM_IOACT_NUMCMP;
                pkey.numcmp_accept = 1;
                ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            } else if (event->passkey.params.action == BLE_SM_IOACT_DISP) {
                pkey.action  = BLE_SM_IOACT_DISP;
                pkey.passkey = 123456;
                ESP_LOGI(TAG, "Pairing passkey: %06lu — enter this in nRF Connect",
                         (unsigned long)pkey.passkey);
                ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            }
            return 0;
        }
        case BLE_GAP_EVENT_REPEAT_PAIRING: {
            struct ble_gap_conn_desc desc;
            ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
            ble_store_util_delete_peer(&desc.peer_id_addr);
            return BLE_GAP_REPEAT_PAIRING_RETRY;
        }
        default:
            ESP_LOGI(TAG, "GAP event %d (unhandled)", event->type);
            break;
    }
    return 0;
}

static void advertise(void)
{
    // Adv payload: flags + complete name = 17 bytes (fits in 31-byte limit)
    struct ble_hs_adv_fields adv = {0};
    adv.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    adv.name = (const uint8_t *)BLE_ENV_DEVICE_NAME;
    adv.name_len = sizeof(BLE_ENV_DEVICE_NAME) - 1;
    adv.name_is_complete = 1;
    int rc = ble_gap_adv_set_fields(&adv);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_set_fields failed: %d", rc);
        app_state_set_error(APP_ERROR_BLE);
        return;
    }

    // Scan response: UUID128 = 19 bytes (fits in 31-byte limit)
    struct ble_hs_adv_fields rsp = {0};
    rsp.uuids128 = &ENV_SERVICE_UUID;
    rsp.num_uuids128 = 1;
    rsp.uuids128_is_complete = 1;
    rc = ble_gap_adv_rsp_set_fields(&rsp);
    if (rc != 0) {
        ESP_LOGE(TAG, "rsp_set_fields failed: %d", rc);
        app_state_set_error(APP_ERROR_BLE);
        return;
    }

    struct ble_gap_adv_params params = {0};
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    params.itvl_min  = BLE_ENV_ADV_ITVL_UNITS;
    params.itvl_max  = BLE_ENV_ADV_ITVL_UNITS;
    rc = ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, NULL, BLE_HS_FOREVER, &params, gap_event_cb, NULL);
    if (rc == 0) {
        app_state_set_runtime(APP_STATE_ADVERTISING);
        ESP_LOGI(TAG, "Advertising started as %s", BLE_ENV_DEVICE_NAME);
    } else {
        app_state_set_error(APP_ERROR_BLE);
        ESP_LOGE(TAG, "Advertising failed: %d", rc);
    }
}

static void on_sync(void)
{
    /* Random static address (bits 7:6 of byte[5] = 0b11 per BT spec).
     * Using a fixed value avoids stale-LTK rejections from Android during
     * development — Android has never seen this address and pairs fresh. */
    static const uint8_t rnd_addr[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0xC2};
    ble_hs_id_set_rnd(rnd_addr);
    advertise();
}

static void deep_sleep_timer_cb(void *arg)
{
    if (s_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        ble_gap_terminate(s_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        /* Deep sleep triggered from gap_event_cb on disconnect. */
    } else {
        /* Already disconnected, sleep immediately. */
        ESP_LOGI(TAG, "Entering deep sleep for 30 s (already disconnected)");
        esp_sleep_enable_timer_wakeup(BLE_ENV_DEEP_SLEEP_DURATION_US);
        esp_deep_sleep_start();
    }
}

static void nimble_host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t ble_env_service_init(void)
{
    esp_timer_create_args_t ta = {
        .callback = deep_sleep_timer_cb,
        .arg = NULL,
        .name = "deep_sleep",
    };
    esp_timer_create(&ta, &s_deep_sleep_timer);

    nimble_port_init();
    ble_store_config_init();
    esp_log_level_set("NimBLE", ESP_LOG_WARN);
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set(BLE_ENV_DEVICE_NAME);
    ble_gatts_count_cfg(gatt_svcs);
    ble_gatts_add_svcs(gatt_svcs);

    /* Must be set AFTER nimble_port_init(), which resets ble_hs_cfg to defaults. */
    ble_hs_cfg.sm_io_cap         = BLE_HS_IO_NO_INPUT_OUTPUT;
    ble_hs_cfg.sm_bonding        = 1;
    ble_hs_cfg.sm_mitm           = 0;
    ble_hs_cfg.sm_sc             = 1;   /* Android 16 AuthReq always has SC=1; Legacy path fails immediately */
    ble_hs_cfg.sm_our_key_dist   = BLE_SM_PAIR_KEY_DIST_ENC;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC;
    ble_hs_cfg.store_status_cb   = ble_store_util_status_rr;
    ble_hs_cfg.sync_cb           = on_sync;

    nimble_port_freertos_init(nimble_host_task);
    return ESP_OK;
}

esp_err_t ble_env_service_notify_telemetry(const sensor_sample_t *sample, uint16_t sequence)
{
    app_state_t s = app_state_get_snapshot();
    if (!sample || !s.connected || !s.telemetry_subscribed || s_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        return ESP_OK;
    }

    uint8_t frame[16];
    encode_telemetry(frame, sample, sequence);
    struct os_mbuf *om = ble_hs_mbuf_from_flat(frame, sizeof(frame));
    if (!om) {
        return ESP_ERR_NO_MEM;
    }
    int rc = ble_gatts_notify_custom(s_conn_handle, s_telemetry_val_handle, om);
    return rc == 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t ble_env_service_notify_status(void)
{
    app_state_t s = app_state_get_snapshot();
    if (!s.connected || !s.status_subscribed || s_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        return ESP_OK;
    }

    uint8_t frame[6];
    encode_status(frame);
    struct os_mbuf *om = ble_hs_mbuf_from_flat(frame, sizeof(frame));
    if (!om) {
        return ESP_ERR_NO_MEM;
    }
    int rc = ble_gatts_notify_custom(s_conn_handle, s_status_val_handle, om);
    return rc == 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t ble_env_service_notify_ml_alert(uint8_t ml_class, uint8_t confidence)
{
    app_state_t s = app_state_get_snapshot();
    if (!s.connected || !s_ml_alert_subscribed
        || s_conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        return ESP_OK;
    }
    uint8_t buf[2] = { ml_class, confidence };
    struct os_mbuf *om = ble_hs_mbuf_from_flat(buf, sizeof(buf));
    if (!om) return ESP_ERR_NO_MEM;
    int rc = ble_gatts_notify_custom(s_conn_handle, s_ml_alert_val_handle, om);
    return rc == 0 ? ESP_OK : ESP_FAIL;
}
