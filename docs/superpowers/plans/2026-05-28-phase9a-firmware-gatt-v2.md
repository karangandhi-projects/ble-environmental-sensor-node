# Firmware GATT v2 + Sensor Override Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Upgrade the GATT profile to v2 (add `b7e00006` Sensor Override + `b7e00007` ML Alert + User Description descriptors on all characteristics) and implement a BLE-controllable sensor override in `env_sensor` using TDD.

**Architecture:** Minimal firmware touch — new UUID constants and characteristics added to `ble_env_service.c`, override state added to `sensor_provider.c` behind the existing `sensor_provider_read()` interface. TDD-first for override logic, manual nRF Connect verification for GATT changes.

**Tech Stack:** ESP-IDF v5.2.3, NimBLE, Unity (on-target tests), ESP32-C3

---

## File Map

| File | Change |
|------|--------|
| `firmware/components/env_sensor/test/test_sensor_override.c` | **Create** — Unity TDD tests (write first) |
| `firmware/components/env_sensor/test/CMakeLists.txt` | **Modify** — add new test file to SRCS |
| `firmware/components/env_sensor/include/sensor_provider.h` | **Modify** — add `set_override` / `clear_override` declarations |
| `firmware/components/env_sensor/sensor_provider.c` | **Modify** — add override state + modify `sensor_provider_read()` |
| `firmware/components/ble_env/ble_env_service.c` | **Modify** — new UUIDs, GATT table, write handler, notify function |
| `firmware/components/ble_env/include/ble_env_service.h` | **Modify** — add `ble_env_service_notify_ml_alert` declaration |
| `firmware/components/app_core/include/app_config.h` | **Modify** — add ML class defines |
| `docs/gatt_profile.md` | **Modify** — unfreeze, add v2 characteristics, re-freeze |

---

### Task 1: Approval gate

Request explicit "yes approve" from the user for all existing file edits listed in the File Map above before proceeding to any other task.

- [ ] **Step 1: Present approval request**

List the 7 existing files requiring edits and ask: "Approve all of these for editing? Reply 'yes approve' to continue."

Do not proceed until the user replies "yes approve".

---

### Task 2: Write failing TDD tests for sensor override

New file — no approval needed.

**Files:**
- Create: `firmware/components/env_sensor/test/test_sensor_override.c`

- [ ] **Step 1: Create the test file**

```c
#include "unity.h"
#include "sensor_provider.h"

TEST_CASE("override: read returns set values", "[env_sensor]")
{
    sensor_provider_set_override(2550, 6020, 10132);
    sensor_sample_t s = sensor_provider_read();
    TEST_ASSERT_EQUAL_INT16(2550,   s.temperature_c_x100);
    TEST_ASSERT_EQUAL_UINT16(6020,  s.humidity_pct_x100);
    TEST_ASSERT_EQUAL_UINT32(101320, s.pressure_pa);  /* 10132 * 10 */
    TEST_ASSERT_TRUE(s.valid);
    TEST_ASSERT_TRUE(s.simulated);
}

TEST_CASE("override: simulated flag stays set", "[env_sensor]")
{
    sensor_provider_set_override(2000, 5000, 10000);
    sensor_sample_t s = sensor_provider_read();
    TEST_ASSERT_TRUE(s.simulated);
}

TEST_CASE("override: clear restores valid simulated sample", "[env_sensor]")
{
    sensor_provider_set_override(2550, 6020, 10132);
    sensor_provider_clear_override();
    sensor_sample_t s = sensor_provider_read();
    TEST_ASSERT_TRUE(s.valid);
    TEST_ASSERT_TRUE(s.simulated);
    /* Must NOT be the override value — simulated drifts with time */
    /* Just verify it's in a plausible range, not the exact override value */
    TEST_ASSERT_TRUE(s.temperature_c_x100 != 2550 || s.humidity_pct_x100 != 6020);
}

TEST_CASE("override: clear without prior set is safe", "[env_sensor]")
{
    sensor_provider_clear_override();
    sensor_sample_t s = sensor_provider_read();
    TEST_ASSERT_TRUE(s.valid);
}

TEST_CASE("override: min boundary values", "[env_sensor]")
{
    sensor_provider_set_override(-1000, 0, 9000);
    sensor_sample_t s = sensor_provider_read();
    TEST_ASSERT_EQUAL_INT16(-1000, s.temperature_c_x100);
    TEST_ASSERT_EQUAL_UINT16(0,    s.humidity_pct_x100);
    TEST_ASSERT_EQUAL_UINT32(90000, s.pressure_pa);
}

TEST_CASE("override: max boundary values", "[env_sensor]")
{
    sensor_provider_set_override(6000, 10000, 11000);
    sensor_sample_t s = sensor_provider_read();
    TEST_ASSERT_EQUAL_INT16(6000,  s.temperature_c_x100);
    TEST_ASSERT_EQUAL_UINT16(10000, s.humidity_pct_x100);
    TEST_ASSERT_EQUAL_UINT32(110000, s.pressure_pa);
}
```

---

### Task 3: Update test CMakeLists.txt

**Files:**
- Modify: `firmware/components/env_sensor/test/CMakeLists.txt`

- [ ] **Step 1: Add test_sensor_override.c to SRCS**

Replace the full file with:
```cmake
idf_component_register(
    SRCS
        "test_sensor_provider.c"
        "test_sensor_override.c"
    INCLUDE_DIRS "."
    REQUIRES env_sensor unity
)
```

---

### Task 4: Build test app — expect linker failure

- [ ] **Step 1: Run build**

```bash
source ~/esp/esp-idf/export.sh && cd /home/karan-gandhi/ble_skill_project_package_reviewed/firmware/test_app && idf.py build 2>&1 | tail -20
```

Expected: linker error — `undefined reference to sensor_provider_set_override` and `sensor_provider_clear_override`. This is the expected TDD red state. If it builds clean, stop — the symbols already exist somewhere and the plan needs revision.

---

### Task 5: Add override declarations to sensor_provider.h

**Files:**
- Modify: `firmware/components/env_sensor/include/sensor_provider.h`

- [ ] **Step 1: Add the two new function declarations after `sensor_provider_read`**

The file currently ends with:
```c
esp_err_t sensor_provider_init(void);
sensor_sample_t sensor_provider_read(void);
```

Replace with:
```c
esp_err_t sensor_provider_init(void);
sensor_sample_t sensor_provider_read(void);

/* Override simulated values from BLE. pressure_hpa_x10: pressure in 0.1 hPa units
 * (e.g. 10132 = 1013.2 hPa). Passing all-zeros to set_override is valid; use
 * clear_override to return to random simulation. */
void sensor_provider_set_override(int16_t temp_cdeg,
                                   uint16_t humidity_cpct,
                                   uint16_t pressure_hpa_x10);
void sensor_provider_clear_override(void);
```

---

### Task 6: Implement sensor override in sensor_provider.c

**Files:**
- Modify: `firmware/components/env_sensor/sensor_provider.c`

- [ ] **Step 1: Replace the full file**

```c
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
```

---

### Task 7: Build test app — expect success

- [ ] **Step 1: Run build**

```bash
source ~/esp/esp-idf/export.sh && cd /home/karan-gandhi/ble_skill_project_package_reviewed/firmware/test_app && idf.py build 2>&1 | tail -5
```

Expected output ends with:
```
[100%] Linking C executable test_app.elf
...
Project build complete.
```

If build fails, fix the error before proceeding.

---

### Task 8: Run TDD tests on-target

- [ ] **Step 1: Flash and run**

```bash
source ~/esp/esp-idf/export.sh && cd /home/karan-gandhi/ble_skill_project_package_reviewed/firmware/test_app && idf.py -T env_sensor flash monitor
```

- [ ] **Step 2: Verify all 6 override tests pass**

Expected output includes:
```
PASS - override: read returns set values
PASS - override: simulated flag stays set
PASS - override: clear restores valid simulated sample
PASS - override: clear without prior set is safe
PASS - override: min boundary values
PASS - override: max boundary values
```

Also verify existing tests still pass:
```
PASS - sensor_provider: init succeeds
PASS - sensor_provider: read returns valid simulated sample
```

If any test fails, fix the implementation before proceeding.

---

### Task 9: Add new UUIDs and static variables to ble_env_service.c

**Files:**
- Modify: `firmware/components/ble_env/ble_env_service.c`

- [ ] **Step 1: Add include for string.h at the top**

After line 1 (`#include "ble_env_service.h"`), add:
```c
#include "sensor_provider.h"
#include <string.h>
```

- [ ] **Step 2: Add new static variables after existing ones (after line 26)**

After `static esp_timer_handle_t s_deep_sleep_timer;`, add:
```c
static uint16_t s_ml_alert_val_handle;
static bool     s_ml_alert_subscribed = false;
```

- [ ] **Step 3: Add new UUID constants after the existing five (after line 32)**

After `static const ble_uuid128_t STATUS_UUID = ...`, add:
```c
static const ble_uuid128_t SENSOR_OVERRIDE_UUID = BLE_UUID128_INIT(0x00,0x00,0x00,0x6c,0x6a,0x2f,0x7d,0x8b,0x2a,0x4c,0x4a,0x4f,0x06,0x00,0xe0,0xb7);
static const ble_uuid128_t ML_ALERT_UUID        = BLE_UUID128_INIT(0x00,0x00,0x00,0x6c,0x6a,0x2f,0x7d,0x8b,0x2a,0x4c,0x4a,0x4f,0x07,0x00,0xe0,0xb7);
```

---

### Task 10: Add gatt_user_desc_cb and update GATT table

**Files:**
- Modify: `firmware/components/ble_env/ble_env_service.c`

- [ ] **Step 1: Add gatt_user_desc_cb before gatt_access_cb (before line 61)**

```c
static int gatt_user_desc_cb(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle; (void)attr_handle;
    const char *desc = (const char *)arg;
    return os_mbuf_append(ctxt->om, desc, strlen(desc)) == 0
           ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}
```

- [ ] **Step 2: Replace the entire gatt_svcs array (lines 174–187)**

```c
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
                .descriptors = (struct ble_gatt_dscr_def[]) {
                    { .uuid = BLE_UUID16_DECLARE(0x2901), .att_flags = BLE_ATT_F_READ,
                      .access_cb = gatt_user_desc_cb, .arg = "Telemetry" },
                    { 0 }
                },
            },
            {
                .uuid      = &CONTROL_UUID.u,
                .access_cb = gatt_access_cb,
                .flags     = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC,
                .descriptors = (struct ble_gatt_dscr_def[]) {
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
                .descriptors = (struct ble_gatt_dscr_def[]) {
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
                .descriptors = (struct ble_gatt_dscr_def[]) {
                    { .uuid = BLE_UUID16_DECLARE(0x2901), .att_flags = BLE_ATT_F_READ,
                      .access_cb = gatt_user_desc_cb, .arg = "Status" },
                    { 0 }
                },
            },
            {
                .uuid      = &SENSOR_OVERRIDE_UUID.u,
                .access_cb = gatt_access_cb,
                .flags     = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC,
                .descriptors = (struct ble_gatt_dscr_def[]) {
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
                .descriptors = (struct ble_gatt_dscr_def[]) {
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
```

---

### Task 11: Add SENSOR_OVERRIDE handler in gatt_access_cb

**Files:**
- Modify: `firmware/components/ble_env/ble_env_service.c`

- [ ] **Step 1: Add the handler before the final `return BLE_ATT_ERR_UNLIKELY` (before line 171)**

Insert after the closing brace of the CONTROL_UUID handler block (after line 168 `return 0;`), before `return BLE_ATT_ERR_UNLIKELY;`:

```c
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
```

---

### Task 12: Update gap_event_cb for ML_ALERT subscription

**Files:**
- Modify: `firmware/components/ble_env/ble_env_service.c`

- [ ] **Step 1: Add ML alert subscription tracking in SUBSCRIBE case**

Find the `BLE_GAP_EVENT_SUBSCRIBE` case (lines 218–222):
```c
        case BLE_GAP_EVENT_SUBSCRIBE:
            if (event->subscribe.attr_handle == s_telemetry_val_handle) {
                app_state_set_telemetry_subscribed(event->subscribe.cur_notify);
            } else if (event->subscribe.attr_handle == s_status_val_handle) {
                app_state_set_status_subscribed(event->subscribe.cur_notify);
            }
            break;
```

Replace with:
```c
        case BLE_GAP_EVENT_SUBSCRIBE:
            if (event->subscribe.attr_handle == s_telemetry_val_handle) {
                app_state_set_telemetry_subscribed(event->subscribe.cur_notify);
            } else if (event->subscribe.attr_handle == s_status_val_handle) {
                app_state_set_status_subscribed(event->subscribe.cur_notify);
            } else if (event->subscribe.attr_handle == s_ml_alert_val_handle) {
                s_ml_alert_subscribed = event->subscribe.cur_notify;
            }
            break;
```

- [ ] **Step 2: Reset ml_alert_subscribed on disconnect**

Find the `BLE_GAP_EVENT_DISCONNECT` case. After `app_state_set_connected(false);`, add:
```c
            s_ml_alert_subscribed = false;
```

---

### Task 13: Add ble_env_service_notify_ml_alert to header and implementation

**Files:**
- Modify: `firmware/components/ble_env/include/ble_env_service.h`
- Modify: `firmware/components/ble_env/ble_env_service.c`

- [ ] **Step 1: Add declaration to ble_env_service.h**

After the existing `ble_env_service_notify_status` declaration, add:
```c
esp_err_t ble_env_service_notify_ml_alert(uint8_t ml_class, uint8_t confidence);
```

- [ ] **Step 2: Add implementation to ble_env_service.c**

After the `ble_env_service_notify_status` function (end of file), add:
```c
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
```

---

### Task 14: Add ML class defines to app_config.h

**Files:**
- Modify: `firmware/components/app_core/include/app_config.h`

- [ ] **Step 1: Add ML alert class constants at end of file**

```c
/* Phase 9C: TinyML alert classes (b7e00007 ML Alert characteristic). */
#define BLE_ENV_ML_CLASS_COMFORTABLE  0
#define BLE_ENV_ML_CLASS_WARM         1
#define BLE_ENV_ML_CLASS_COLD         2
#define BLE_ENV_ML_CLASS_HUMID        3
#define BLE_ENV_ML_CLASS_DANGER       4
#define BLE_ENV_ML_CLASS_ANOMALY      5
```

---

### Task 15: Update docs/gatt_profile.md (unfreeze → add v2 → re-freeze)

**Files:**
- Modify: `docs/gatt_profile.md`

- [ ] **Step 1: Read the current file header**

```bash
head -5 /home/karan-gandhi/ble_skill_project_package_reviewed/docs/gatt_profile.md
```

- [ ] **Step 2: Update the header to indicate in-progress**

Add or replace the freeze status line at the top of the document with:
```
> **Profile Status: [IN PROGRESS v2]** — Adding Sensor Override + ML Alert characteristics and User Description descriptors.
```

- [ ] **Step 3: Add the two new characteristics to the Characteristics section**

After the Status characteristic entry, add:

```markdown
### Sensor Override Characteristic

UUID: `b7e00006-4f4a-4c2a-8b7d-2f6a6c000000`

User Description (0x2901): `"Sensor Override"`

Properties: Write (encrypted)

Payload: 6 bytes, little-endian

| Bytes | Field | Type | Units |
|-------|-------|------|-------|
| 0–1 | temperature | int16 | °C × 100 |
| 2–3 | humidity | uint16 | % × 100 |
| 4–5 | pressure | uint16 | hPa × 10 |

Write all-zeros (`0x00 0x00 0x00 0x00 0x00 0x00`) to clear the override and resume random simulation.

Slider ranges: Temp −10–60 °C, Humidity 0–100%, Pressure 900–1100 hPa.

---

### ML Alert Characteristic

UUID: `b7e00007-4f4a-4c2a-8b7d-2f6a6c000000`

User Description (0x2901): `"ML Alert"`

Properties: Notify (open — no encryption required)

Payload: 2 bytes

| Byte | Field | Values |
|------|-------|--------|
| 0 | class | 0=comfortable, 1=warm, 2=cold, 3=humid, 4=danger, 5=anomaly |
| 1 | confidence | 0–100 (model softmax × 100) |

Notification sent only when class changes. Used in Phase C (TFLite Micro edge deployment).
```

- [ ] **Step 4: Add User Description note to all existing characteristics**

For each of the five existing characteristics (Telemetry, Control, Configuration, Status), add a line:
```
User Description (0x2901): `"<Name>"`
```

- [ ] **Step 5: Update the freeze header to v2**

Replace the in-progress header from Step 2 with:
```
> **Profile Status: [FROZEN v2]** — Characteristics b7e00002–b7e00007 and all 0x2901 descriptors are locked. No changes without explicit user approval.
```

---

### Task 16: Build main firmware

- [ ] **Step 1: Run build**

```bash
source ~/esp/esp-idf/export.sh && cd /home/karan-gandhi/ble_skill_project_package_reviewed/firmware && idf.py build 2>&1 | tail -10
```

Expected:
```
[100%] Linking C executable ble_env_node.elf
...
Project build complete.
```

If build fails, fix all errors before proceeding. Do not flash a broken build.

---

### Task 17: Run TDD tests on-target (confirm still passing)

- [ ] **Step 1: Flash and run env_sensor tests**

```bash
source ~/esp/esp-idf/export.sh && cd /home/karan-gandhi/ble_skill_project_package_reviewed/firmware/test_app && idf.py -T env_sensor flash monitor
```

All 8 tests must show PASS before proceeding.

---

### Task 18: Flash main app and verify with nRF Connect

- [ ] **Step 1: Flash main app**

```bash
source ~/esp/esp-idf/export.sh && cd /home/karan-gandhi/ble_skill_project_package_reviewed/firmware && idf.py -p /dev/ttyUSB0 flash monitor
```

- [ ] **Step 2: Connect with nRF Connect, verify named descriptors**

Open nRF Connect on phone, connect to `BLE_ENV_NODE`, pair when prompted.
Navigate to the service. Confirm each characteristic shows its name:
- `b7e00002` → "Telemetry"
- `b7e00003` → "Control"
- `b7e00004` → "Configuration"
- `b7e00005` → "Status"
- `b7e00006` → "Sensor Override"
- `b7e00007` → "ML Alert"

- [ ] **Step 3: Test sensor override write**

In nRF Connect, write to `b7e00006` (must be bonded/encrypted):
Value: `0xF6 0x09 0x8C 0x17 0x94 0x27`
Decodes as: temp=2550 (25.5°C), humidity=6020 (60.2%), pressure=10132 (1013.2hPa)

Then read or subscribe to Telemetry (`b7e00002`) — confirm the values match.

- [ ] **Step 4: Test clear override**

Write all-zeros to `b7e00006`: `0x00 0x00 0x00 0x00 0x00 0x00`
Confirm Telemetry values return to drifting simulated values.

---

### Task 19: Commit

- [ ] **Step 1: Stage and commit**

```bash
git add \
  firmware/components/env_sensor/include/sensor_provider.h \
  firmware/components/env_sensor/sensor_provider.c \
  firmware/components/env_sensor/test/test_sensor_override.c \
  firmware/components/env_sensor/test/CMakeLists.txt \
  firmware/components/ble_env/ble_env_service.c \
  firmware/components/ble_env/include/ble_env_service.h \
  firmware/components/app_core/include/app_config.h \
  docs/gatt_profile.md

git commit -m "phase-9a: GATT v2 — sensor override, ML alert char, user descriptions"
```
