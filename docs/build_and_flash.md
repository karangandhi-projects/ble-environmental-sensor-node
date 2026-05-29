# Build and Flash Guide

## Prerequisites

- ESP32-C3 development board.
- USB cable that supports data.
- ESP-IDF installed and exported in the shell.
- Python environment used by ESP-IDF.
- Phone with nRF Connect or LightBlue.

## Wiring

The 0.42" SSD1306-based OLED (72x40 visible pixels inside a 128x64 framebuffer, column offset 28) connects over I2C.

| ESP32-C3 pin | OLED pin |
|--------------|----------|
| 3V3          | VCC      |
| GND          | GND      |
| GPIO5        | SDA      |
| GPIO6        | SCL      |

I2C address is `0x3C`. Bus runs at 400 kHz.

Pull-ups: most breakout modules ship with on-board pull-ups on SDA/SCL. If yours does not, add ~4.7 kOhm pull-ups from each line to 3V3.

Warning: when a BME280 is added in Phase 9 on the same I2C bus, make sure addresses do not collide. BME280 lives at `0x76` or `0x77`, so it coexists fine with the OLED at `0x3C`. Do not strap any other device to `0x3C` or you will see bus contention and intermittent OLED corruption.

## Verifying the OLED on the I2C Bus

Before chasing graphics bugs, confirm the panel ACKs at `0x3C`.

Least intrusive approach: add a one-off probe at the top of `app_main` using the ESP-IDF v5.2 I2C master driver:

```c
i2c_master_bus_handle_t bus;
i2c_master_bus_config_t cfg = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .i2c_port   = I2C_NUM_0,
    .sda_io_num = GPIO_NUM_5,
    .scl_io_num = GPIO_NUM_6,
    .glitch_ignore_cnt = 7,
};
ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &bus));
if (i2c_master_probe(bus, 0x3C, 100) == ESP_OK) {
    ESP_LOGI("i2c", "OLED ACK at 0x3C");
} else {
    ESP_LOGW("i2c", "no device at 0x3C");
}
```

Remove the probe once verified, or guard it behind a debug Kconfig.

Alternatively, run a standalone scanner. The ESP-IDF tree includes I2C examples under:

```text
~/esp/esp-idf/examples/peripherals/i2c/
```

Flash one of those temporarily and observe `0x3C` in the scan output.

## Running Unit Tests

On-target Unity tests live alongside each component in a `test/` subdirectory and are driven by the unit-test-app project at `firmware/test_app/`. Each test component's `CMakeLists.txt` includes `WHOLE_ARCHIVE` so TEST_CASE registrations are not stripped by the linker.

### Quick run (non-interactive, from repo root)

Flash the test app once, then collect results with the helper script:

```bash
cd firmware/test_app
idf.py set-target esp32c3
idf.py build flash
cd ../..
python3 firmware/test_app/run_tests.py   # opens /dev/ttyACM0, sends *, captures output
```

The script opens the port without resetting the device, sends `*` to the Unity menu, and reads output until the summary line appears (30 s timeout). A passing run ends with:

```text
37 Tests 0 Failures 1 Ignored
OK

All tests passed.
```

The 1 Ignored is the Phase 1.5 placeholder test — expected.

### Interactive run

For interactive debugging (run a specific tag, iterate on a failure):

```bash
cd firmware/test_app && idf.py -p /dev/ttyACM0 flash monitor
```

At the Unity menu prompt, type:

- `*` to run every registered test.
- `[display]` (or any tag in square brackets) to run only that component's tests.
- A test name to run a single case.

A failure prints the file, line, expected, and actual values. Re-run with the specific `[tag]` to iterate quickly.

### After testing — restore main firmware

The test_app replaces the main firmware on flash. After a test run, reflash the application:

```bash
cd firmware && idf.py -p /dev/ttyACM0 flash
```

## Build Commands

From `firmware/`:

```bash
idf.py set-target esp32c3
idf.py menuconfig
idf.py build
```

## Flash and Monitor

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

On some systems the port may be:

```bash
/dev/ttyACM0
```

or on macOS:

```bash
/dev/cu.usbmodemXXXX
```

## Expected Serial Logs

Minimum expected logs:

```text
BLE_ENV_NODE booting
NVS initialized
App state initialized
Sensor provider initialized
BLE service initialized
Advertising started
```

## BLE Verification

1. Open nRF Connect.
2. Scan for `BLE_ENV_NODE`.
3. Connect.
4. Discover services.
5. Find Environmental Node Service.
6. Read Telemetry.
7. Enable Telemetry notifications.
8. Write Control command.
9. Read Status.

## Troubleshooting Build Issues

### NimBLE headers missing
Check sdkconfig options and ESP-IDF version.

### Device not visible
- Confirm advertising started in logs.
- Restart scan on phone.
- Reset board.
- Make sure another central is not already connected.

### Connects but no service visible
- Confirm GATT service registration success.
- Restart phone Bluetooth.
- Clear app cache/bond if stale.
