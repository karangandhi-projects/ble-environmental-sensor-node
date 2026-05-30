# Design Decisions

## DD-001 Use BLE Peripheral/GATT Server First

Decision: The node acts as a BLE peripheral and GATT server.

Reason:
- The project models a common embedded product pattern: sensor device + phone/gateway central.
- Peripheral mode teaches advertising, connection handling, GATT services, notifications, and low-power behavior.

Alternatives:
- Central scanner: useful later for gateways, but not the best first BLE product pattern.
- Broadcaster-only beacon: simpler, but does not teach GATT reads/writes or connection behavior.

## DD-002 Use ESP-IDF + NimBLE

Decision: Use ESP-IDF with NimBLE as the BLE host stack.

Reason:
- NimBLE is appropriate for BLE-focused applications and integrates with ESP-IDF/FreeRTOS.
- It keeps the project closer to modern BLE-only architecture.
- It avoids starting with Bluetooth Classic features that are not needed.

Tradeoff:
- Some examples online use Bluedroid. Developers must be careful not to mix APIs.

## DD-003 Simulated Sensor First

Decision: Implement simulated telemetry before adding real BME280/BMP280.

Reason:
- BLE behavior can be validated without I2C bring-up.
- Separates wireless learning from hardware debugging.
- Provides deterministic test data.

Tradeoff:
- Real sensor timing, bus failure, and calibration are not exercised until later.

## DD-004 Stable Custom GATT Profile

Decision: Define a custom 128-bit UUID service and characteristics.

Reason:
- Custom services teach real product data modeling.
- 128-bit UUIDs avoid conflicts with standard Bluetooth SIG services.

Tradeoff:
- Generic clients do not understand semantics automatically; documentation is required.

## DD-005 Binary Telemetry Payload

Decision: Use a compact binary telemetry frame rather than JSON strings.

Reason:
- BLE payloads are size-constrained.
- Binary frames are closer to embedded product design.
- MTU and serialization tradeoffs become visible.

Tradeoff:
- Requires a decoder tool and documentation.

## DD-006 Do Not Block in BLE Callbacks

Decision: BLE callbacks update state, enqueue events, or copy data only.

Reason:
- Blocking in stack callbacks can cause timing problems and unstable behavior.
- Long operations belong in app tasks/timers.

## DD-007 Configuration Stored in NVS

Decision: Use NVS for small configuration items.

Reason:
- ESP-IDF provides NVS for key-value persistent storage.
- Reporting interval and flags are small and fit well.

Tradeoff:
- Frequent writes must be avoided to reduce flash wear.

## DD-008 Keep OTA Out of MVP

Decision: OTA is documented as a later extension, not part of MVP.

Reason:
- OTA adds bootloader, partitioning, image validation, security, and failure recovery complexity.
- BLE fundamentals should work first.

## DD-009 Documentation Is Part of the Product

Decision: Each implementation phase must update docs.

Reason:
- The package is meant to train system design, not just code writing.
- Documentation allows agents and humans to continue work safely.

## DD-010 Multi-Component Firmware Layout

Decision: Production source lives under `firmware/components/{app_core, ble_env, env_sensor, display}/` with `firmware/main/` containing only `app_main.c`.

Reason:
- Forces explicit `REQUIRES` edges between components, surfacing accidental coupling.
- Each component owns its public headers under `include/` and its own `test/` subdir for Unity tests.
- Aligns with AGENT_BRIEF constraint 6 ("hardware-specific code behind interfaces"). The `display` component is the first new boundary added under this layout.

Tradeoff:
- Slightly more CMake boilerplate per component. A flat `main/` layout would compile faster but blur the layering and make per-component testing impractical.

## DD-011 TDD for Pure Logic via On-Target Unity

Decision: Pure-logic modules (encoders, validators, state setters, storage parsing, display formatters) are developed test-first using ESP-IDF Unity, run on-target via `firmware/test_app/` (a unit-test-app project).

Reason:
- The encoders and validators have explicit byte-level contracts (per `gatt_profile.md`) that benefit from regression locks.
- Running on-target ensures host/target ABI parity (size_t width, endianness) without a separate cross-build.
- Failing tests act as design pressure: anything that can't be tested without the BLE stack stays out of the pure-logic boundary.

Tradeoff:
- Each test run requires a flash cycle (no host-only fast path). NimBLE callbacks, the SSD1306 register sequence, and font bitmaps are NOT TDD'd — they remain manually verified.

## DD-012 OLED Display as First-Class Output

Decision: Add a 0.42" SSD1306 OLED on I2C (SDA=GPIO5, SCL=GPIO6, addr 0x3C, 72×40 inside 128×64 with X-offset 28) showing rotating pages for BLE state, temperature, and humidity. A `SIM` badge appears on the temperature/humidity pages while `BLE_ENV_FLAG_SIMULATED_DATA` is set.

Reason:
- During Phases 2–6 the device has no serial console handy on a benchtop; the screen makes BLE state visible at a glance.
- Binding the SIM badge to the existing telemetry flag means Phase 9 (real sensor) clears the badge automatically — no display code change required.
- The display component is testable in isolation: the page scheduler, label mapping, and number formatters are pure logic.

Tradeoff:
- ~5–10 mA active current; the panel dominates idle power during development. Blanking the panel when disconnected is a documented stretch optimization (see `docs/power_budget.md`).

## DD-013 Multi-Agent Orchestration with Scope-Containment

Decision: Parallelize code-generation work via sub-agents where the work is independent (pre-phase scaffolding, Phase 1.5 display sub-modules, Phase 3 encoder TDD, Phase 5/6 TDD). Every sub-agent runs under a strict preamble that confines writes to `/home/karan-gandhi/ble_skill_project_package_reviewed/` and reads-only from `~/esp/esp-idf/`.

Reason:
- Hardware-bound work (flashing, nRF Connect, manual TC verification) is inherently single-threaded; parallelism only helps where work is code-gen-bound.
- Scope containment prevents agents from drifting into unrelated parts of the filesystem when they hit an ambiguous instruction.
- Side-effecting commands (`gh repo create`, `git push`, `idf.py flash`) are reserved for the orchestrator (main thread).

Tradeoff:
- Sub-agents lack the orchestrator's full context, so prompts must be self-contained. Co-edits to the same file must be serialized.

## DD-015 Explicit Power Interval Tuning, Sleep Modes, and Display Power Control

Decision: Set advertising interval to 250 ms. Expose power mode (active / light sleep / deep sleep) and display state (on / off / dim) via BLE Control characteristic writes, using the same opcode pattern established for LED control.

Reason:

- **Advertising interval (250 ms)**: halves radio duty cycle vs the NimBLE default ~100 ms; discovery still completes in <1 s so developer UX is not impacted. Explicitly chosen so the intent is documented and traceable.
- **Connection interval**: the firmware does **not** call `ble_gap_update_params()`; the call was added during Phase 7 but removed during Phase 8 pairing debug to avoid a race with `BLE_GAP_EVENT_ENC_CHANGE` (the update request arrived before the link was encrypted, causing pairing failures on Android 16). The four `BLE_ENV_CONN_*` constants that supported the call (`BLE_ENV_CONN_ITVL_MIN_UNITS`, `BLE_ENV_CONN_ITVL_MAX_UNITS`, `BLE_ENV_CONN_LATENCY`, `BLE_ENV_CONN_SUPERVISION_UNITS`) had no users after the removal and have been deleted. Power tuning on the connection side is therefore reduced to the peripheral-side advertising interval (250 ms) plus whatever connection interval the central negotiates on its own. Re-introducing the negotiation would require ordering it after `BLE_GAP_EVENT_ENC_CHANGE`.
- **Power mode via opcode 0x20**: reuses the existing Control characteristic, no new GATT characteristics needed. Deep sleep disconnects BLE and wakes after 30 s via timer — demonstrable in a portfolio context. Light sleep (opcode 0x01) uses `CONFIG_PM_ENABLE` + `esp_pm_configure()` and keeps the BLE connection alive.
- **Display power via opcode 0x30**: SSD1306 DISPLAYOFF drops panel current from ~5–10 mA to ~20 µA. Ephemeral (opcode 0x30) for runtime control, persistent (Config flags bit 1) for boot-time preference — mirrors the report_interval pattern (live control + NVS persistence).

Tradeoff:

- Deep sleep resets all volatile state (power mode, ephemeral display) — only NVS-backed config survives.
- Light sleep on ESP32-C3 with BLE requires careful clock config; `CONFIG_PM_ENABLE=y` added to sdkconfig.defaults — a full clean rebuild is required after this change.

## DD-016 GATT v2 — User Description Descriptors on All Characteristics

Decision: Add a Characteristic User Description descriptor (UUID 0x2901) to all six characteristics in the GATT service, exposing human-readable names ("Telemetry", "Control", "Configuration", "Status", "Sensor Override", "ML Alert").

Reason:
- nRF Connect displays these names instead of "Unknown Characteristic", making manual testing and debugging dramatically faster.
- Android's `BluetoothGattDescriptor` can read 0x2901 at service discovery time, enabling programmatic labeling without UUID lookup tables.
- Zero runtime cost — the strings are in flash, the `gatt_user_desc_cb` callback fires only on explicit descriptor reads.

Implementation: `gatt_user_desc_cb()` in `ble_env_service.c` uses `os_mbuf_append()` to serve the `(const char *)arg` string passed as the descriptor's `arg` field in the GATT table.

Tradeoff:
- NimBLE's struct is `ble_gatt_dsc_def` (not `ble_gatt_dscr_def` as some examples show) — discovered at build time.

## DD-017 Sensor Override with ±2°C/±2%/±2hPa Drift

Decision: When the BLE Sensor Override characteristic (b7e00006) is written, `sensor_provider_read()` returns the set values plus a time-based drift of ±2°C, ±2% RH, and ±2 hPa, cycling through the pattern every 5 seconds using `esp_timer_get_time()`.

Reason:
- Exact override values (no drift) would produce degenerate training data: 30 identical samples at exactly 22.0°C/45.0%/1013 hPa. A model trained on this would likely memorize exact values rather than learning the class region.
- ±2°C drift creates realistic variation that forces the classifier to learn class boundaries robustly. Real BME280 sensors have ±0.5°C noise; ±2°C is deliberately generous for training coverage.
- The same drift makes the Dashboard display animate naturally, giving the device a "live sensor" feel during demos.

Tradeoff:
- Unit tests for exact override values had to be updated to use `TEST_ASSERT_INT16_WITHIN(200, ...)` instead of exact equality.

## DD-018 Pure-C MLP Inference Instead of TFLite Micro

Decision: Implement the TinyML classifier as a plain C forward pass with weights compiled into a header file (`ml_weights.h`), rather than using the TFLite Micro runtime.

Reason:
- `tensorflow/lite-micro` is not available in the ESP-IDF v5.2.3 component registry (`idf_component_manager add tensorflow/lite-micro` returns "Component not found").
- The model is architecturally tiny: 3→16→8→5 MLP = 245 weights and biases. Embedded as `static const float` arrays, they occupy ~980 bytes — smaller than TFLite Micro's runtime alone (~100KB).
- The forward pass is 70 lines of pure C: `dense()`, `relu()`, `softmax()`, `tinyml_infer()`. No C++ toolchain, no external dependencies, no schema version mismatches.
- Binary footprint increase: +3KB vs Phase 9A baseline (0x98410 → 0x99520).

Tradeoff:
- Model updates require retraining, re-running `ml/extract_weights.py`, and reflashing. There is no hot-swap mechanism. Acceptable for a portfolio/learning project.
- The int8 quantized `model_data.cc` (also in the component) is retained for reference if TFLite Micro support is added later.

## DD-019 Anomaly Detection via Classifier Confidence Threshold

Decision: Declare `ML_CLASS_ANOMALY` (class 5) when the classifier's maximum softmax probability is below 0.50, rather than using a separate autoencoder.

Reason:
- The initial autoencoder approach (3→8→3, trained on comfortable-only data) was conceptually wrong for this use case: an autoencoder trained on one class flags ALL other classes as anomalies, including well-known labeled classes like "danger". At 55°C/10%/980 hPa, the device returned "anomaly" instead of "danger".
- The confidence threshold approach is correct by construction: if the classifier is genuinely uncertain between two classes (e.g., 26°C temperature between comfortable and warm), max softmax < 0.5. If a known class matches strongly (e.g., danger), max softmax → 0.99 and anomaly is never triggered.
- No additional model weights needed — the existing 245-weight classifier handles anomaly detection as a side effect of its uncertainty.

Implementation: In `tinyml_infer()`, after softmax, if `out[best] < 0.50f` → return `ML_CLASS_ANOMALY` with confidence = `(1 - out[best]) × 100`.

Tradeoff:
- Cannot detect anomalies that happen to land inside a trained class region (e.g., a sensor fault that reads a plausible but wrong temperature). A real production system would need additional monitoring.

## DD-020 MITM Passkey Display (Phase B)

Decision: Replace Just Works pairing (`BLE_HS_IO_NO_INPUT_OUTPUT`, `sm_mitm=0`) with Passkey Display (`BLE_HS_IO_DISPLAY_ONLY`, `sm_mitm=1`).

Reason:
- Phase A validation (`firmware/test_mitm/`, 2026-05-29) confirmed that Passkey Entry works on ESP32-C3 + Android 16.
- The SSD1306 OLED is already present — Passkey Display is the natural fit; no new hardware needed.
- Just Works provides encryption but no MITM protection; any BLE device in range can impersonate the central during pairing.

Key finding: three fields beyond changing `io_cap` and `mitm` flag are required for passkey pairing to succeed on Android — `store_status_cb`, `sm_our_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC`, `sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC`. Without them, pairing produces "Incorrect PIN or passkey" even with the correct passkey.

Tradeoff:
- Slightly more friction on first pair (user must type 6 digits). All subsequent reconnects are seamless (stored LTK).

## DD-021 Shared-Static Access Pattern on Single-Core ESP32-C3

### Context

Three shared `static` variables are read and written across FreeRTOS task boundaries without an explicit lock:

1. `s_conn_handle` (`uint16_t`, `ble_env_service.c`) — written by the NimBLE host task in CONNECT (line 366) and DISCONNECT (line 395) GAP events; read by `telemetry_task` in `notify_telemetry` / `notify_status` / `notify_ml_alert` (lines 513, 575, 585, 592, 602, 609–616).
2. `s_ml_alert_subscribed` (`bool`, `ble_env_service.c`) — written by the NimBLE host task in DISCONNECT (line 397) and SUBSCRIBE (line 413); read by `telemetry_task` in `notify_ml_alert` (lines 609–610).
3. `s_last_page` (`uint8_t`, `display.c`) — protected by `s_mux` inside `display_set_passkey` (line 160) and `display_clear_passkey` (line 169), but **not** protected inside `display_set_power` (line 129) and `display_tick` (lines 231/233/240/243).

Independent review (REVIEW_FINDINGS A5) flagged these as "inconsistent locking." This DD records the analysis and the accepted position.

### Decision

**Option 1 — document why the existing access pattern is safe; no code change.**

- ESP32-C3 is a single-core RV32IMC processor. Single-word loads and stores (`uint16_t`, `bool`, `uint8_t`) are atomic at the ISA level — no torn reads or writes are possible regardless of FreeRTOS preemption.
- The `notify_*` call sites in `ble_env_service.c` follow a "check then call" pattern (e.g., `if (s_conn_handle != NONE) ble_gatts_notify_custom(s_conn_handle, ...)`). Wrapping only the check in a lock would not eliminate the race window between check and call; eliminating that window would require holding the lock *across* the BLE notify call itself — exactly the anti-pattern that AGENT_BRIEF §7 and DD-006 forbid (never block or hold a long critical section inside or across BLE callbacks/paths).
- The `s_mux` in `display.c` exists to protect **compound multi-field updates** (passkey active flag + passkey value + page set together; sensor sample `memcpy`). Single-word writes to `s_last_page` from `display_set_power` or `display_tick` do not need the mutex for atomicity; they are already atomic by the RV32 ISA guarantee.
- `display_tick` snapshots all multi-field state under `s_mux` into locals (lines 222–227), then reads/writes `s_last_page` outside the lock. That is correct: the lock guards the compound snapshot; `s_last_page` is then operated on as a single-word local-logic variable.
- The worst-case outcome of a "check then use" preemption race on `s_conn_handle` is one redundant `ble_gatts_notify_custom()` call after disconnect, which NimBLE rejects with `BLE_HS_ENOTCONN` — handled correctly downstream. The race window is bounded by FreeRTOS preemption latency (typically tens of microseconds).

### Alternatives considered

- **Option 2 — add `volatile`**: Would prevent the compiler from hoisting reads out of loops. However, each variable is already accessed through a function-call boundary, so the compiler already emits a fresh memory load and cannot cache across the call. `volatile` would be harmless but unnecessary; adding it for appearances would mislead future readers into thinking there is a memory-ordering concern that `volatile` actually addresses (it does not).
- **Option 3 — add `portMUX_TYPE` everywhere** (the pattern used in `app_state.c`): Would over-serialize interrupts for zero correctness benefit on a uniprocessor and add tens of cycles per access on the hot telemetry path. Appropriate where compound state must be read or written atomically (as in `app_state.c`); not appropriate for single-word fields with the constraints above.

### Consequences

- The existing access pattern is correct as-is for the single-core ESP32-C3 MVP scope.
- If a future port targets a dual-core variant (ESP32, ESP32-S3, etc.), the right migration is `_Atomic`-qualified declarations with C11 `stdatomic.h` load/store operations — not `volatile` (which provides no ordering on multi-core) and not `portMUX` (which serializes interrupts globally). This DD documents that decision so future reviewers do not re-open the question.
- REVIEW_FINDINGS A5 is closed.

## DD-014 Phase-by-Phase Human Checkpoints with Approval Gate

Decision: Every phase ends with a structured report (code changes, build result, Unity result, manual TC result, doc updates, known issues), then waits for the user's go-ahead. Edits to existing source files require explicit user approval before the change is made; new files (tests, new modules, new docs) may be added freely.

Reason:
- Frozen contracts (GATT UUIDs, payload layouts) must not be silently mutated mid-implementation; the approval gate forces a deliberate decision.
- Per-phase commits + push give the user a clean rollback point at each checkpoint.

Tradeoff:
- Slower than fully autonomous execution. Acceptable for a learning project where reviewing each phase is itself part of the value.
