#include "ble_env_service.h"
#include "app_config.h"
#include "app_state.h"
#include "storage_config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/ble_gatt.h"
#include "host/ble_gap.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

static const char *TAG = "ble_env";
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t s_telemetry_val_handle;
static uint16_t s_status_val_handle;

static const ble_uuid128_t ENV_SERVICE_UUID = BLE_UUID128_INIT(0x00,0x00,0x00,0x6c,0x6a,0x2f,0x7d,0x8b,0x2a,0x4c,0x4a,0x4f,0x01,0x00,0xe0,0xb7);
static const ble_uuid128_t TELEMETRY_UUID   = BLE_UUID128_INIT(0x00,0x00,0x00,0x6c,0x6a,0x2f,0x7d,0x8b,0x2a,0x4c,0x4a,0x4f,0x02,0x00,0xe0,0xb7);
static const ble_uuid128_t CONTROL_UUID     = BLE_UUID128_INIT(0x00,0x00,0x00,0x6c,0x6a,0x2f,0x7d,0x8b,0x2a,0x4c,0x4a,0x4f,0x03,0x00,0xe0,0xb7);
static const ble_uuid128_t CONFIG_UUID      = BLE_UUID128_INIT(0x00,0x00,0x00,0x6c,0x6a,0x2f,0x7d,0x8b,0x2a,0x4c,0x4a,0x4f,0x04,0x00,0xe0,0xb7);
static const ble_uuid128_t STATUS_UUID      = BLE_UUID128_INIT(0x00,0x00,0x00,0x6c,0x6a,0x2f,0x7d,0x8b,0x2a,0x4c,0x4a,0x4f,0x05,0x00,0xe0,0xb7);


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
                app_state_set_error(APP_ERROR_INVALID_CONFIG);
                return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            }
            ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), NULL);
            uint16_t interval = (uint16_t)buf[2] | ((uint16_t)buf[3] << 8);
            if (app_state_set_report_interval(interval) != ESP_OK) {
                return BLE_ATT_ERR_UNLIKELY;
            }
            storage_config_t cfg = { .version = BLE_ENV_CONFIG_VERSION, .flags = buf[1], .report_interval_ms = interval };
            storage_config_save(&cfg);
            return 0;
        }
    }

    if (ble_uuid_cmp(uuid, &CONTROL_UUID.u) == 0 && ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint8_t buf[2];
        uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
        if (len != sizeof(buf)) {
            app_state_set_error(APP_ERROR_INVALID_COMMAND);
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), NULL);
        switch (buf[0]) {
            case 0x01: app_state_set_led(false); app_state_set_error(APP_ERROR_OK); break;
            case 0x02: app_state_set_led(true); app_state_set_error(APP_ERROR_OK); break;
            case 0x03: app_state_toggle_led(); app_state_set_error(APP_ERROR_OK); break;
            case 0x10: app_state_set_error(APP_ERROR_OK); break;
            default:
                app_state_set_error(APP_ERROR_INVALID_COMMAND);
                return BLE_ATT_ERR_UNLIKELY;
        }
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
            { .uuid = &TELEMETRY_UUID.u, .access_cb = gatt_access_cb, .val_handle = &s_telemetry_val_handle, .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY },
            { .uuid = &CONTROL_UUID.u, .access_cb = gatt_access_cb, .flags = BLE_GATT_CHR_F_WRITE },
            { .uuid = &CONFIG_UUID.u, .access_cb = gatt_access_cb, .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE },
            { .uuid = &STATUS_UUID.u, .access_cb = gatt_access_cb, .val_handle = &s_status_val_handle, .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY },
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
            ESP_LOGI(TAG, "Disconnected");
            s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            app_state_set_connected(false);
            advertise();
            break;
        case BLE_GAP_EVENT_SUBSCRIBE:
            if (event->subscribe.attr_handle == s_telemetry_val_handle) {
                app_state_set_telemetry_subscribed(event->subscribe.cur_notify);
            } else if (event->subscribe.attr_handle == s_status_val_handle) {
                app_state_set_status_subscribed(event->subscribe.cur_notify);
            }
            break;
        default:
            break;
    }
    return 0;
}

static void advertise(void)
{
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (const uint8_t *)BLE_ENV_DEVICE_NAME;
    fields.name_len = sizeof(BLE_ENV_DEVICE_NAME) - 1;
    fields.name_is_complete = 1;
    fields.uuids128 = &ENV_SERVICE_UUID;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;
    ble_gap_adv_set_fields(&fields);

    struct ble_gap_adv_params params = {0};
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    int rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &params, gap_event_cb, NULL);
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
    advertise();
}

static void nimble_host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t ble_env_service_init(void)
{
    nimble_port_init();
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set(BLE_ENV_DEVICE_NAME);
    ble_gatts_count_cfg(gatt_svcs);
    ble_gatts_add_svcs(gatt_svcs);
    ble_hs_cfg.sync_cb = on_sync;
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
