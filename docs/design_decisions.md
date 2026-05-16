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

## DD-014 Phase-by-Phase Human Checkpoints with Approval Gate

Decision: Every phase ends with a structured report (code changes, build result, Unity result, manual TC result, doc updates, known issues), then waits for the user's go-ahead. Edits to existing source files require explicit user approval before the change is made; new files (tests, new modules, new docs) may be added freely.

Reason:
- Frozen contracts (GATT UUIDs, payload layouts) must not be silently mutated mid-implementation; the approval gate forces a deliberate decision.
- Per-phase commits + push give the user a clean rollback point at each checkpoint.

Tradeoff:
- Slower than fully autonomous execution. Acceptable for a learning project where reviewing each phase is itself part of the value.
