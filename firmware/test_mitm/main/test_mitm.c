/**
 * @file test_mitm.c
 * @brief Minimal NimBLE passkey-display validation for ESP32-C3.
 *
 * PURPOSE
 * -------
 * Validate that BLE_HS_IO_DISPLAY_ONLY + sm_mitm=1 pairing works correctly
 * on this specific ESP32-C3 + Android combination BEFORE integrating passkey
 * pairing into the main BLE_ENV_NODE firmware.
 *
 * This app is intentionally minimal — no OLED, no sensor, no GATT profile
 * beyond what is needed to trigger the pairing flow. The passkey is printed
 * to the serial monitor only.
 *
 * HOW TO USE
 * ----------
 * 1. Build and flash:
 *      cd firmware/test_mitm
 *      idf.py set-target esp32c3
 *      idf.py build flash monitor
 *
 * 2. Open nRF Connect or your Android app.
 *
 * 3. Scan — you should see "BLE_MITM_TEST".
 *
 * 4. Connect. Then try writing to the test characteristic (UUID 0xFFF1).
 *    The write will fail with "Insufficient Authentication" (ATT error 0x05),
 *    which triggers Android to initiate pairing.
 *
 * 5. Android shows a "Bluetooth pairing — enter PIN" dialog.
 *
 * 6. The serial monitor prints:
 *      I (xxx) mitm_test: >>> PASSKEY: 042891 <<< — enter this on the phone
 *
 * 7. Enter the passkey on the phone. On success:
 *      I (xxx) mitm_test: Encryption established (conn 1)
 *
 * 8. Disconnect and reconnect — bond should restore, no passkey needed.
 *
 * SUCCESS CRITERIA (must all pass before integrating into BLE_ENV_NODE)
 * -----------------------------------------------------------------------
 *  [_] Android shows PIN entry dialog on first connect + write
 *  [_] Correct passkey → "Encryption established" in serial
 *  [_] Wrong passkey  → "Encryption failed" in serial, device re-advertises
 *  [_] Reconnect with existing bond → no passkey dialog, encryption auto-restored
 *  [_] Android "Forget device" → reconnect → new PIN dialog appears
 */

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_random.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_store.h"
/* ble_store_config_init() is in the store/config component, not a public header —
 * declare extern like the vendor bleprph example does. */
void ble_store_config_init(void);
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "mitm_test";
#define DEVICE_NAME "BLE_MITM_TEST"

/* Forward declaration — gap_event_cb is referenced by advertise() */
static int gap_event_cb(struct ble_gap_event *event, void *arg);

/* --------------------------------------------------------------------------
 * GATT service: one writable characteristic that requires authentication.
 * Writing to it will trigger Android to initiate pairing.
 * -------------------------------------------------------------------------- */

/* Test service UUID: 0000FFF0-0000-1000-8000-00805F9B34FB (16-bit 0xFFF0) */
static const ble_uuid16_t TEST_SVC_UUID   = BLE_UUID16_INIT(0xFFF0);
/* Test characteristic UUID: 0xFFF1 — WRITE, requires authentication */
static const ble_uuid16_t TEST_CHR_UUID   = BLE_UUID16_INIT(0xFFF1);

static int test_chr_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        ESP_LOGI(TAG, "Test characteristic written (encrypted write success)");
    }
    return 0;
}

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &TEST_SVC_UUID.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid       = &TEST_CHR_UUID.u,
                .access_cb  = test_chr_access_cb,
                /* WRITE + requires authenticated link (triggers pairing) */
                .flags      = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_AUTHEN,
            },
            { 0 }, /* terminator */
        },
    },
    { 0 }, /* terminator */
};

/* --------------------------------------------------------------------------
 * Advertising
 * -------------------------------------------------------------------------- */

static void advertise(void)
{
    struct ble_hs_adv_fields fields = {0};
    fields.flags                 = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name                  = (uint8_t *)DEVICE_NAME;
    fields.name_len              = strlen(DEVICE_NAME);
    fields.name_is_complete      = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_set_fields failed: %d", rc);
        return;
    }

    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, NULL, BLE_HS_FOREVER,
                            &adv_params, gap_event_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_start failed: %d", rc);
        return;
    }
    ESP_LOGI(TAG, "Advertising as \"%s\"", DEVICE_NAME);
}

/* --------------------------------------------------------------------------
 * GAP event handler
 * -------------------------------------------------------------------------- */

static int gap_event_cb(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {

    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            ESP_LOGI(TAG, "Connected (conn %u)", event->connect.conn_handle);
            /* Proactively initiate security — sends a Security Request PDU to the
             * central so Android sees the pairing negotiation immediately on connect,
             * rather than waiting for a failed write to trigger it.
             * Note: do NOT use a timer here (caused issues in Phase 8). Call directly. */
            ble_gap_security_initiate(event->connect.conn_handle);
            ESP_LOGI(TAG, "Security request sent — waiting for passkey action");
        } else {
            ESP_LOGW(TAG, "Connection failed (status %d); re-advertising", event->connect.status);
            advertise();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Disconnected (reason=0x%04x)", event->disconnect.reason);
        advertise();
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        if (event->enc_change.status == 0) {
            ESP_LOGI(TAG, "Encryption established (conn %u) — MITM pairing succeeded",
                     event->enc_change.conn_handle);
        } else {
            ESP_LOGW(TAG, "Encryption failed (conn %u, status %d) — wrong passkey?",
                     event->enc_change.conn_handle, event->enc_change.status);
        }
        return 0;

    /* BLE_GAP_EVENT_LINK_ESTAB (33) — new-style connect event used by vendor
     * bleprph example. Handle alongside BLE_GAP_EVENT_CONNECT for completeness. */
    case BLE_GAP_EVENT_LINK_ESTAB:
        if (event->connect.status == 0) {
            ESP_LOGI(TAG, "LINK_ESTAB: connected (conn %u)", event->connect.conn_handle);
        }
        return 0;

    case BLE_GAP_EVENT_PASSKEY_ACTION: {
        struct ble_sm_io pkey = {0};
        ESP_LOGI(TAG, "PASSKEY_ACTION fired — action=%d", event->passkey.params.action);

        if (event->passkey.params.action == BLE_SM_IOACT_DISP) {
            /* Peripheral generates passkey → phone user types it in */
            uint32_t passkey = esp_random() % 1000000;
            pkey.action  = BLE_SM_IOACT_DISP;
            pkey.passkey = passkey;
            ESP_LOGI(TAG, "");
            ESP_LOGI(TAG, ">>> PASSKEY: %06lu <<< — enter this on the phone",
                     (unsigned long)passkey);
            ESP_LOGI(TAG, "");
            int rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            ESP_LOGI(TAG, "ble_sm_inject_io result: %d", rc);

        } else if (event->passkey.params.action == BLE_SM_IOACT_NUMCMP) {
            /* Numeric comparison: auto-accept (shouldn't fire for DISPLAY_ONLY) */
            ESP_LOGI(TAG, "Numeric comparison %06lu (auto-accepting — unexpected for DISPLAY_ONLY)",
                     (unsigned long)event->passkey.params.numcmp);
            pkey.action        = BLE_SM_IOACT_NUMCMP;
            pkey.numcmp_accept = 1;
            int rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
            ESP_LOGI(TAG, "ble_sm_inject_io result: %d", rc);

        } else {
            ESP_LOGW(TAG, "Unexpected passkey action %d", event->passkey.params.action);
        }
        return 0;
    }

    case BLE_GAP_EVENT_REPEAT_PAIRING: {
        /* Central cleared its bond — delete ours and allow re-pair */
        struct ble_gap_conn_desc desc;
        ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        ble_store_util_delete_peer(&desc.peer_id_addr);
        ESP_LOGI(TAG, "Bond cleared by central — will re-pair");
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    }

    default:
        return 0;
    }
}

/* --------------------------------------------------------------------------
 * NimBLE host task + on-sync callback
 * -------------------------------------------------------------------------- */

static void on_sync(void)
{
    /* Random static address — different from main firmware's address so both
     * can be flashed to separate devices without collision. */
    static const uint8_t rnd_addr[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x02, 0xC2};
    ble_hs_id_set_rnd(rnd_addr);
    advertise();
}

static void on_reset(int reason)
{
    ESP_LOGE(TAG, "NimBLE host reset (reason %d)", reason);
}

static void nimble_host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/* --------------------------------------------------------------------------
 * app_main
 * -------------------------------------------------------------------------- */

void app_main(void)
{
    ESP_LOGI(TAG, "=== BLE MITM Passkey Validation ===");
    ESP_LOGI(TAG, "IO cap: DISPLAY_ONLY  |  sm_mitm=1  |  sm_sc=1  |  sm_bonding=1");

    /* NVS — required for NimBLE bond storage */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* NimBLE host initialisation */
    nimble_port_init();

    ble_hs_cfg.sync_cb        = on_sync;
    ble_hs_cfg.reset_cb       = on_reset;
    /* Required: handles bond-store overflow (round-robin eviction).
     * Without this, NimBLE has no store_status_cb and key storage can fail. */
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /* Security Manager configuration — diff vs bleprph vendor example */
    ble_hs_cfg.sm_io_cap  = BLE_HS_IO_DISPLAY_ONLY; /* peripheral shows passkey */
    ble_hs_cfg.sm_bonding = 1;                       /* store bond keys in NVS */
    ble_hs_cfg.sm_mitm    = 1;                       /* require MITM protection */
    ble_hs_cfg.sm_sc      = 1;                       /* Secure Connections (Android 16+) */
    /* Key distribution: tell both sides to exchange LTK for encrypted bonding.
     * Missing these caused pairing to fail silently — vendor example sets both. */
    ble_hs_cfg.sm_our_key_dist   |= BLE_SM_PAIR_KEY_DIST_ENC;
    ble_hs_cfg.sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC;

    /* GATT — register test service */
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set(DEVICE_NAME);
    ble_gatts_count_cfg(gatt_svcs);
    ble_gatts_add_svcs(gatt_svcs);

    /* Initialize NVS-backed bond store — required for key persistence.
     * Without this the bond store backend is not configured and key exchange
     * during pairing silently fails. Must be called after GATT init. */
    ble_store_config_init();

    /* NimBLE uses its own FreeRTOS task */
    nimble_port_freertos_init(nimble_host_task);
}
