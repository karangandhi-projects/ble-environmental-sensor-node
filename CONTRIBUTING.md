# Contributing

## Development Setup

```bash
source ~/esp/esp-idf/export.sh   # required before any idf.py command
cd firmware
idf.py set-target esp32c3
idf.py build
```

See `docs/build_and_flash.md` for full toolchain setup including Android and ML.

## Code Style

- **Firmware (C):** Follow the existing style — 4-space indent, `snake_case` for functions and variables, `UPPER_CASE` for macros. All public API functions must be declared in the component's `include/` header.
- **Android (Kotlin):** Standard Kotlin style with Jetpack Compose patterns. One composable per logical screen section.
- **Python (ML):** PEP 8.
- No trailing whitespace. Unix line endings.

## Testing Requirements

- Any change to pure-logic firmware code (encoders, validators, state setters) must include or update a Unity test in the component's `test/` directory.
- GATT UUIDs and payload byte layouts are frozen — see `docs/gatt_profile.md`. Changes require explicit discussion.
- Run the unit test suite before submitting a PR:
  ```bash
  cd firmware/test_app
  idf.py set-target esp32c3
  idf.py build flash monitor   # requires connected ESP32-C3
  ```

## Pull Request Guidelines

- One logical change per PR. Keep diffs small and reviewable.
- Include a brief description of what changed and why.
- Reference the relevant design decision (DD-xxx) if the change affects architecture.
- Hardware-validated changes (BLE behaviour, OLED, power modes) should include a note on what was tested and how.

## Approval Gate

Edits to existing source files in `firmware/components/` require maintainer review before merge. New files (tests, docs, new modules) can be proposed freely.
