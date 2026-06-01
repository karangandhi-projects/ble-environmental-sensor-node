# Independent Review Findings & Fix Log

**Date:** 2026-05-29 (created); last updated 2026-06-01 end-of-session-3.
**Branch:** main (HEAD `e7b934b` after session 3 — all 21 session-3 commits pushed to origin; total since file creation: 28 commits).
**Reviewer:** principal-level pass (independent of `docs/principal_review_report.md`, retired to a pointer stub in session 3 — C7).
**Purpose:** Durable record of issues found, what's verified on hardware, and what needs fixing. Safe to resume from this file in a new session.

---

## Session 2026-06-01 — wrap & resume point

**Closed this session (10 punch-list items, 21 commits all pushed):** the remaining open items — A2, A3, A4, A5, A6, A7, B5, C5, C6, C7-full — executed via plan-driven multi-subagent dispatch from `docs/superpowers/plans/2026-05-30-review-findings-cleanup.md`. One Opus orchestrator + Haiku subagents for mechanical doc edits (T1, T2) + Sonnet subagents for the bulk (T3–T9) + a Sonnet executor for the A5 DD-021 write-up (decision made by the Opus orchestrator after reading the three sites). Each task = one work commit + one Status-tick commit, both pushed before the next task started — full per-task resume safety across `/clear` and session boundaries.

| Item | Commit | Summary |
|---|---|---|
| A7 | `449efdd` | sensor_provider.c header @par drift comment "±2 hPa" → "±4 hPa" to match code (`press_drift = ((t % 5) - 2) * 200` → ±400 Pa) and the inline comment on line 61. |
| C5 | `d4463cc` | requirements.md FR-011 page spec aligned with implementation: {temperature, humidity, pressure} @ 2000 ms each + persistent BLE-state badge (was {state, temp, humidity} @ 3000/1500/1500 ms). |
| A2 | `baa6403` | README override paragraph corrected — `SIM` badge stays on for override readings (matches sensor_provider.c setting `simulated=true` for override too); only a real on-board BME280/BMP280 driver will clear it. |
| A3 | `2ac5825` | README sensor-status paragraph: default sim is near-constant (≈24.5°C / 52% RH / 1013 hPa, sub-degree variation); realistic ±2°C / ±2% / ±4 hPa drift only happens under override. |
| C6 | `49d0f0f` | architecture.md: single canonical 5-component Module Layout (tinyml_inference inline) at the top; "Phase 9 Extensions — Component Map" subsection collapsed to a pointer up. Stale "display TBD — Phase 1.5" removed; dependency-graph sentence bumped to 5 components. |
| A4 | `178f180` | `gap_event_cb`: `ble_gap_conn_find` return now checked; on failure log + fall through to "assume unbonded → initiate pairing" instead of using uninitialized `_desc.peer_id_addr`. Firmware +160 B (new `ESP_LOGW` string). |
| A6 | `294bf6b` | Deleted `BLE_ENV_CONN_ITVL_MIN/MAX_UNITS`, `BLE_ENV_CONN_LATENCY`, `BLE_ENV_CONN_SUPERVISION_UNITS` from app_config.h; DD-015 + power_budget.md updated — firmware does not call `ble_gap_update_params()` (removed during Phase 8 pairing debug). |
| B5 | `85571ca` | Deleted `ML_AE_We/be/Wd/bd` arrays + `ML_AE_HIDDEN_SIZE` + `ML_ANOMALY_THRESHOLD` from ml_weights.h (DD-019 made them dead). `extract_weights.py` simplified to no longer preserve them. Binary size unchanged — linker `--gc-sections` had already stripped them. |
| C7 (full) | `6a55cb7` | `docs/principal_review_report.md` body replaced with a 4-line pointer stub at `docs/REVIEW_FINDINGS.md`. The SUPERSEDED banner added in `08d6420` (session 2) wasn't enough — the body still narrated stale facts. |
| A5 | `3a119d9` | DD-021 added — Option 1 (document why the existing access pattern on `s_conn_handle` / `s_ml_alert_subscribed` / `s_last_page` is safe on single-core ESP32-C3; no code change). `volatile` and `portMUX`-everywhere considered and rejected. README DD-range bumped to DD-021. |
| (cleanup) | `e7b934b` | Backfilled the A5 row's SHA in this table (chicken-and-egg — closed-row was written before the commit landed). |

Plus 10 per-task Status-tick commits on the plan file itself (`ade0455`, `88079a2`, `c455be3`, `2281945`, `c1a2bdd`, `27b708d`, `d1896cd`, `5a37d88`, `6d362bc`, `0f2e5b9`), and one SHA-fixup commit (`3d0e118`) — making 21 session-3 commits total.

**Build state at end of session:** firmware `0x95d50` (was `0x95cb0` at session-3 start; +160 B net, all from A4's log string — A6 deleted preprocessor constants (zero bytes), B5's AE arrays were already linker-stripped). test_app `0x373c0` (unchanged, links green). No on-target re-verify done this session.

**New DD this session:** DD-021 in `docs/design_decisions.md` (shared-static access pattern on single-core ESP32-C3 — single-word atomic; check-then-use races inherent and bounded; multi-field locks belong to compound state, not single-word).

**Pending on-target verifications carried over from sessions 2 + 3:**
- TC-006 (write report interval) + TC-011 (reboot persistence) — confirms session 2's A1 deferred NVS write is behaviour-preserving.
- TC-009 (notify subscribe/unsubscribe across reconnects) — confirms session 3's A4 added branch + DD-021 reasoning hold under real BLE traffic.
- All 62 Unity tests (`62 Tests / 0 Failures / 1 Ignored`) — re-run after the env_sensor / ble_env / app_core / tinyml_inference touches in this session.

**Resume point for next session:** all non-optional punch-list items are now closed. Remaining open work:
1. **T11 (B2 path-a)** — retrain classifier + redeploy weights to resolve the saved_model-vs-deployed mismatch surfaced by session 2's B1. OPTIONAL; needs Python TF venv + on-target TC-ML-* re-verify. Plan task at `docs/superpowers/plans/2026-05-30-review-findings-cleanup.md` § T11.
2. **B4 / D1** — wire up or delete the dead ML artefacts (`model_data.cc`, `model.tflite`, `model_quantized.tflite`, `saved_model/`). Couple with T11 or do as its own pass.
3. **D2 / E1–E5** — long-tail items (hardcoded BLE static address, no CI, no host-runnable tests, no real-sensor validation, Android `onConnectionStateChange` robustness, extending `issues_encountered.md` with the Phase 8 / Phase 9 sagas). Each is its own plan.

---

## Session 2026-05-30 — wrap (historical)

**Closed this session (10 items, 7 commits all pushed):**

| Item | Commit | Summary |
|---|---|---|
| TEST-COLLISION (docs) | `d0810f5` | Updated CLAUDE.md / CONTRIBUTING.md / architecture.md / issues_encountered.md to the post-rename `test_<name>/` layout; committed REVIEW_FINDINGS.md itself. |
| C1 (docs) | `08d6420` | Replaced "Just Works" with MITM Passkey Display across 10 live docs; pointed everything at `docs/security_model.md` as the source of truth. |
| C1 (source comments) | `fa0e802` | Same pairing-story fix in `ble_env_service.{c,h}` + `BleRepository.kt` / `DeviceState.kt` / `GattUuids.kt` doc-comments; corrected "See DD-008" → "See DD-020". |
| B1 | `53c27fd` | Added `ml/extract_weights.py` to make deployed weights reproducible from `models/saved_model`. Smoke test surfaced a real saved_model-vs-deployed mismatch. |
| B2 + B3 | `0f5adf8` | Propagated 98.83% (deployed-model truth) and the box-separability honesty caveat to README / tinyml_inference.h / RELEASE_NOTES. |
| A1 | `a3ef354` | Deferred `storage_config_save()` out of `gatt_access_cb` via a dirty-flag mirror of `force_sample` — NVS write now runs in `telemetry_task` after notify. Firmware +304 B; test_app unchanged. |
| C3 | `3b8acc2` | Reconciled binary size to `0x95cb0`, Unity to 62 / 0 / 1, manual to 24/25 + 1 Obsolete across README / RELEASE_NOTES / build_and_flash.md / tinyml_guide.md / implementation_plan.md. |
| C4 | `aa79796` | Fixed RELEASE_NOTES DD cross-refs (DD-001/002/003/004/015 all mislabeled), `model_data.h` → `ml_weights.h`, "IRAM" → flash `.rodata`, "20-entry history" → `take(50)`. |

**Build state at end of session 2:** firmware `0x95cb0` (post-A1; was `0x95b80` pre-A1), test_app `0x373c0`. On-target re-verify for A1 carried forward to session 3 (now combined with the session 3 verifications above).

---

## ✅ TEST-COLLISION fix — RESOLVED & COMMITTED

- **Code fix committed (2026-05-29, `e1ed479`):** 4 dir renames (`test/` → `test_<name>/`) + `firmware/test_app/CMakeLists.txt` EXTRA_COMPONENT_DIRS update + `static` removed from `encode_telemetry`/`encode_status` in `ble_env_service.c`.
- **On-target verified (2026-05-29):** `idf.py -p /dev/ttyACM0 flash monitor` printed **`62 Tests 0 Failures 1 Ignored / OK`** (was 37/0/1). All 25 previously-dropped tests pass, including the 3 `encode_*` frozen-GATT-layout regression locks. Capture note: agent-spawned serial (pyserial `run_tests.py` or raw `cat`) fails on the ESP32-C3 USB-Serial/JTAG CDC; only an interactive `!`-run terminal works.
- **Doc-path cleanup committed (2026-05-30, `d0810f5`):** `CLAUDE.md` (TDD rule), `CONTRIBUTING.md` (testing section), `docs/architecture.md` (module layout tree), `docs/issues_encountered.md` (Issue 3 — added 2026-05-29 follow-up + lesson).
- **All commits pushed to origin.**

## ✅ C1 / C2 / mini-C7 — RESOLVED & COMMITTED (2026-05-30)

- **C1 docs (`08d6420`):** SECURITY.md, README.md, gatt_profile.md, requirements.md (FR-010/FR-014), implementation_plan.md (Phase 8 superseded note), test_plan.md (TC-SEC-02 obsoleted, TC-SEC-05/06 added), manual_test_matrix.md (TC-SEC-02 → Obsolete), phase8_pairing_debug.md (historical banner — closes C2), learning/android_ble_guide.md (both sections rewritten), principal_review_report.md (stale banner — partial C7 + the Just Works line).
- **C1 source comments (`fa0e802`):** ble_env_service.{c,h} (@par Security + init sequence; corrected "See DD-008" → "See DD-020"), BleRepository.kt (## Security KDoc), DeviceState.kt (@property encrypted), GattUuids.kt (ControlOpcodes block).
- **Verification:** firmware build green at `0x95b80` (unchanged — pure comments). `grep -rn "[Jj]ust[ -][Ww]orks"` over `firmware/` and `android/` returns no live results. `docs/security_model.md` is now the single source of truth; every other doc either defers to it or carries a stale-banner.

## ✅ B1 — RESOLVED (2026-05-30)

- **Added `ml/extract_weights.py`** — loads `ml/models/saved_model`, extracts the three Dense layers (`sequential/dense{,_1,_2}/kernel|bias`), transposes each kernel to row-major (out_size × in_size) to match `tinyml_inference.c`'s `W[i * in_size + j]` access, and writes the 245 classifier weights into the six `ML_W*` / `ML_b*` arrays in `firmware/components/tinyml_inference/include/ml_weights.h`. The header doc-comment, `#defines`, and the autoencoder block (`ML_AE_*`) are preserved verbatim — those are stable across retrainings, and the AE arrays are dead per DD-019 and tracked separately under B5.
- **Smoke-tested:** ran the script against `ml/models/saved_model`; it produced a well-formed file with 245 weights regenerated. The output **differs from the deployed `ml_weights.h`** — confirming the B1 hypothesis that the deployed weights and the saved model on disk were out of sync (`saved_model dense/kernel[0,0] = 1.4600533` vs `ML_W1[0] = 1.45603526`). The deployed file was restored — choosing to retrain/redeploy is a separate decision (see B2/B3).
- **Cross-refs fixed:** `ml/train_classifier.py` docstring no longer claims `quantize.py` writes `ml_weights.h` (it doesn't — it writes `model_data.cc`); `docs/architecture.md` ml/ block now lists `extract_weights.py` alongside the other scripts. `docs/design_decisions.md` DD-018 and `docs/learning/tinyml_guide.md` already referenced the script correctly — they were dangling pointers until this commit.

## ✅ B2 / B3 — RESOLVED at the doc layer (2026-05-30)

- **B2 (accuracy reconciliation, path b — no retraining):** chose to propagate **98.83%** everywhere (matches the deployed `ml_weights.h` header — that is the historical accuracy recorded by `train_classifier.py` for the training run that produced the deployed weights). The mismatching `99.7%` was removed from `firmware/components/tinyml_inference/include/tinyml_inference.h` (@par Training doc-comment), `README.md` (pipeline tree + status table), and `docs/RELEASE_NOTES_v1_0_0.md`. `docs/learning/tinyml_guide.md` already showed `98.83%` correctly. `docs/architecture.md` already says "see ml_weights.h for actual deployed accuracy" (touched in B1).
- **B3 (box-separability caveat):** propagated to every place the accuracy is cited. The text makes three claims explicit: (1) the train/test split is drawn from the same disjoint, axis-aligned class boxes in `collect_synthetic.py`, so 98.83% measures box-separability not real-sensor skill; (2) the 379 "real device" override-generated readings are human slider entries inside those same boxes — not independent real-sensor data; (3) no real-sensor validation has been performed; the classifier should be retrained on BME280/SHT31 readings before the number can be claimed as a real-sensor result. Same wording in tinyml_inference.h / README / RELEASE_NOTES.
- **Verification:** firmware builds green at `0x95b80` (comment-only header change, no runtime effect). Grep for `99\.7` in firmware/Android source returns no live results.
- **Path-a follow-up (optional):** retrain via `train_classifier.py` + `extract_weights.py` to get a fresh accuracy on a fresh held-out test set, fix the saved_model-vs-deployed mismatch surfaced in B1, and update the numbers. This would change deployed weights → ML alert behaviour → needs on-target re-verify. Tracked as a separate open item.

## ✅ A1 — RESOLVED (2026-05-30)

- **Defer NVS write out of BLE callback.** `storage_config_save()` no longer runs inside `gatt_access_cb` (NimBLE host task). Instead the BLE write stages the new config via `app_state_request_config_save()` and `telemetry_task` drains it once per cycle via `app_state_get_and_clear_pending_config()` — same dirty-flag pattern as the existing `force_sample`. Drain runs after `ble_env_service_notify_telemetry()` so the BLE notify cadence is never delayed by an ~tens-of-ms flash write.
- **Files touched (4):** `app_state.h` (struct fields + 2 prototypes + `#include "storage_config.h"`), `app_state.c` (init + 2 implementations under the existing spinlock), `ble_env_service.c:154` (call swap + ESP_LOGI text + intent comment), `app_main.c` telemetry_task (drain block).
- **Build verified:** firmware `0x95cb0` (was `0x95b80`, +304 B); test_app `0x373c0` (unchanged). Test_app builds confirm no unit-test breakage from the new fields.
- **Trade-off (acceptable for MVP):** the synchronous save used to complete before the BLE write response; now it lags by up to one `report_interval_ms` (default 2 s). A power cut inside that window would lose the queued save. If this becomes a real concern, an early drain on disconnect (in `gap_event_cb`) would tighten the window — out of A1's scope.
- **On-target verify (user TODO):** TC-006 (write report interval) and TC-011 (reboot after config — interval should persist) should still pass; the code path is preserved end-to-end, just deferred. No changes to on-target unit tests required.

## ✅ C3 — RESOLVED (2026-05-30)

Numbers everywhere now match the verified state. Current build size **0x95cb0** (post-A1; 0x95b80 pre-A1), Unity **62 / 0 / 1**, manual **24/25 Pass + 1 Obsolete** after TC-SEC-02 superseded by TC-SEC-05.

- **Binary size** updated in `README.md` (features bullet + status table), `docs/RELEASE_NOTES_v1_0_0.md` header, `docs/learning/tinyml_guide.md` (lesson summary). Historical phase exit sizes in `docs/implementation_plan.md` (Phase 8: 0x97f20, Phase 9A: 0x98410, Phase 9C: 0x99220) left intact as accurate phase-close records.
- **Unity test count** updated in `README.md` status row, `docs/RELEASE_NOTES_v1_0_0.md` (intro bullet + coverage table), `docs/build_and_flash.md` (sample output). `docs/implementation_plan.md` Phase 10 exit criterion left as "37" *with* an inline correction note pointing at the TEST-COLLISION fix and REVIEW_FINDINGS, since that line is a phase-close record.
- **Manual TC count and ranges** updated in `README.md` (validate-it line + status row), `docs/RELEASE_NOTES_v1_0_0.md` (TC-001–TC-011 → TC-001–TC-012 — also resolves one of C4's items). TC-SEC-02 now consistently labelled Obsolete (already done in C1 sweep).
- **`docs/principal_review_report.md`** stale numbers untouched — the top SUPERSEDED banner from C1 already enumerates them; full retire-or-refresh remains under C7.

## ✅ C4 — RESOLVED (2026-05-30)

`docs/RELEASE_NOTES_v1_0_0.md` cleanup:
- **DD cross-refs fixed.** Bullets in the "Architecture Decisions" section were mislabelled against `docs/design_decisions.md`. Corrected: DD-001 is "BLE Peripheral/GATT Server first" (was attached to NimBLE); DD-002 is "ESP-IDF + NimBLE" (was attached to Simulated sensor); DD-003 is "Simulated sensor first" (was attached to Telemetry task); DD-004 is "Stable custom GATT profile" (was attached to NVS persistence). "Telemetry task" now correctly cites DD-006 (Do Not Block in BLE Callbacks). "NVS for config persistence" now correctly cites DD-007. "Pure-C MLP" now correctly cites DD-018 (was DD-015 which is power tuning).
- **`model_data.h` → `ml_weights.h`** in the "Embedded weights" bullet, plus a clarifying note that the int8 `model_data.cc` exists but is not compiled into the firmware.
- **"245 weights fit in IRAM" → flash `.rodata`** — `static const float` arrays land in `.rodata` (flash), not IRAM. ~980 bytes; the qualifier was technically inaccurate.
- **"20-entry history" → "last 50 entries (`take(50)`)"** to match `DataAlertsScreen.kt:97`.
- **TC range (C4 sub-item) — already fixed in C3** (`TC-001–TC-011` → `TC-001–TC-012`).

## Remaining punch-list (next-pickup)

Priority order in **Priority order** section. Open items at a glance:
- **B2 path-a follow-up** (optional, high if pursued) — retrain + redeploy; resolve the saved_model-vs-deployed mismatch surfaced in B1.
- **A5** (low) — inconsistent locking on s_conn_handle / s_ml_alert_subscribed / s_last_page.
- **A2–A7** (med-low) — small code issues (SIM-badge override docs, default-sim near-constant range, unchecked return, inconsistent locking, dead conn-param constants, drift comment).
- **C5** (low) — OLED page spec in requirements.md FR-011 still says {state, temp, humidity} @ 3000/1500/1500 ms; code is {temp, humidity, pressure} @ 2000/2000/2000 + persistent state badge.
- Then the rest of B, C, D, E in priority order.

---

## ✅ Verified on hardware / by clean build (2026-05-29)

Board: ESP32-C3 on `/dev/ttyACM0` (MAC 38:44:be:44:c0:a8). ESP-IDF v5.2.3.

| Claim in docs | Verdict | Evidence |
|---|---|---|
| Firmware builds green | ✅ TRUE | `idf.py build` of `firmware/` and `firmware/test_app/` both link cleanly from current `main` |
| Binary size `0x94f00`/`0x99520`/`0x98410` | ❌ all wrong | linker: **`0x95b80` bytes (613,248) — 58% used / 42% free** of the 0x100000 app partition |
| Unity suite "37 tests / all pass" | ✅ FIXED → 62 | Was 37 (display only) due to TEST-COLLISION; after the rename + encoder fix, on-target = **`62 Tests 0 Failures 1 Ignored / OK`** (2026-05-29) |
| Manual test matrix "19"/"20" cases | ❌ wrong | matrix actually has **25 rows** (separate from Unity tests) |
| On-target "0 failures" | ✅ TRUE after fix | all 62 pass (1 ignored placeholder); the 25 formerly-dropped tests — incl. the frozen-GATT encode lock — now run green |

---

## 🔴 TEST-COLLISION (top finding) — 40% of the test suite silently never runs

**Symptom:** On-target `idf.py flash monitor` of `test_app` runs only the **37 display tests** (`test_display_logic` 36 PASS + `test_display_pending` 1 IGNORE) → `37 Tests 0 Failures 1 Ignored / OK`. Looks healthy; isn't.

**Root cause (confirmed):** In ESP-IDF a component's name is its **directory basename**, which must be unique. All four test dirs were named `test`, so they collide. `EXTRA_COMPONENT_DIRS` listed them ending with `display/test`, so the build **silently kept only the last** and dropped the other three. No build error.

**Proof:** `firmware/test_app/build/project_description.json` had exactly one component named `test` → `/components/display/test`; `app_core/test`, `ble_env/test`, `env_sensor/test` appeared nowhere in the build graph.

**Impact — 25 of 62 written tests never compiled/ran**, including the locks on the *frozen contracts the project cares most about*:

| Component | Dropped tests | Guards |
|---|---|---|
| `ble_env` | `test_ble_env_encode` (3) | **Frozen GATT byte layouts** (telemetry 16-byte / status 6-byte encoders) |
| `env_sensor` | `test_sensor_provider` (2), `test_sensor_override` (6) | DD-017 ±2 °C override/drift contract |
| `app_core` | `test_app_state` (6), `test_storage_config` (3), `test_power_mode` (5) | State machine, NVS parse, sequence counter |

**Fix:** unique dir basenames (being applied — see top). After fix, expect 62 tests to register; on-target pass result still to be confirmed by the user's `run_tests.py` run.

**Lesson:** ESP-IDF component name = dir basename, must be unique; collisions dedupe silently. A green "OK / 0 failures" covered only 60% of the suite. `compiles` ≠ `runs`.

> NOTE: my earlier claim that "all 8 test files link / 62 tests" was wrong — I inferred it from CMake `SRCS` lists. Only the on-target run exposed the truth.

---

## 🔴 TEST-ENCODE-BROKEN (surfaced by fixing TEST-COLLISION) — the GATT-encoder test never compiled

After the rename fix, the test build fails at link:
```
test_ble_env_encode.c:32: undefined reference to `encode_telemetry'
test_ble_env_encode.c:62: undefined reference to `encode_status'
```
`test_ble_env_encode.c` (3 tests — the regression lock on the **frozen** telemetry 16-byte / status 6-byte layouts) calls `encode_telemetry()`/`encode_status()`, which are **`static`** in `ble_env_service.c` (lines 76, 91).

The test's own header comment (lines 5-9) says this is intentional: a TDD **red** test whose encoder "promotion … will happen in Phase 3 with explicit user approval." **That promotion never happened**, and TEST-COLLISION silently dropped the test from the build, so the broken state was invisible and the docs' "ble_env encode tests pass" was never true. **These 3 tests have never executed.**

**Fix (needs approval — edits `ble_env_service.c`):** give `encode_telemetry`/`encode_status` external linkage + a shared declaration. Recommended: declare both in a small internal header (`ble_env_encode.h`, or in `ble_env_service.h`), `#include` from both `ble_env_service.c` and the test, drop `static`. Minimal: just remove `static` (the test already forward-declares them, lines 19-20). Then rebuild → expect link success and the 3 encode tests to run and pass (they assert the exact frozen bytes, e.g. 2456 → `0x0998` LE at offset 8). This completes a real red→green TDD cycle and turns on the only automated check of the frozen GATT contract.

**STATUS (2026-05-29): FIXED at build level** — `static` removed (approved); test app links green; the 3 encode tests are registered in the ELF. On-target *pass* still to be confirmed via `run_tests.py`.

---

## A. Correctness / code issues

- **A1 [High] Blocking flash write inside a BLE callback.** `ble_env_service.c:151` calls `storage_config_save()` (NVS flash write, tens of ms) synchronously inside `gatt_access_cb` (NimBLE host task). Violates AGENT_BRIEF #7, NFR-003, DD-006, and `app_main.c:30` docstring. **Fix:** set a "config dirty" flag in `app_state`; do the NVS write in `telemetry_task` (mirror `force_sample`).

---

## B. ML pipeline (highest learning value)

- **B1 [High] Deployed model can't be regenerated — `ml/extract_weights.py` does not exist.** The firmware runs float32 weights in `ml_weights.h`, whose header + `tinyml_inference.c:38` + `DD-018:199` + `architecture.md` + `tinyml_guide.md` (lines 345/492/494) all reference `extract_weights.py` to (re)generate them. The file is absent. `train_classifier.py:27` wrongly says `quantize.py` makes it (it only emits the unused int8 `model_data.cc`). **Fix:** add the real `extract_weights.py` (read `models/saved_model`, write the C float arrays) and fix the cross-refs. *Lesson: a model you can't regenerate from committed code, you don't really have.*
- **B2 [High] Accuracy numbers disagree.** Deployed `ml_weights.h` header says **98.83%**; README:177 / `tinyml_inference.h:27` / `architecture.md:227` say **99.7%**. The flashed weights predate the cited dataset. **Fix:** reconcile to one number tied to the actual deployed weights.
- **B3 [High / key lesson] "99.7%" measures box-separability, not skill.** `collect_synthetic.py` draws each class from disjoint uniform boxes; train/test are the same generator, so ~99% is trivial. The "379 real device samples" are human slider entries in the same boxes — not independent data. `RELEASE_NOTES:52` states the caveat correctly; **propagate that honesty everywhere** and stop leading with 99.7%. No real-sensor validation exists.
- **B4 [Med] Dead ML artifacts presented as pipeline.** `firmware/components/tinyml_inference/model_data.cc` (int8) is **not compiled** (`CMakeLists.txt` lists only `tinyml_inference.c`). `ml/models/model.tflite` "for Android MlClassifier" is **unused** (no TFLite dep in the app — it just displays the device's `b7e00007` notification). `model_quantized.tflite`, `saved_model/` also dead. `verify_model.py` smoke-tests `model.tflite`, not the deployed `ml_weights.h` path. **Fix:** wire up or delete/label "reference only."

---

## C. Documentation consistency (dominant problem)

- **C1 [High] "Just Works" vs "MITM Passkey Display" split across the repo.** Code is authoritative: `ble_env_service.c:544-547` = `DISPLAY_ONLY` + `sm_mitm=1` + `sm_sc=1` = **MITM Passkey Display**.
  - **Correct:** `security_model.md`, `RELEASE_NOTES`, `DD-020`, matrix `TC-SEC-05/06`.
  - **Stale/wrong (say Just Works):** `SECURITY.md:8` (worst — public policy, also self-contradictory "device with no display" when the device's display shows the passkey), `gatt_profile.md:187-198`, `README` (roadmap lists passkey as "future" + FR-014), `requirements.md` FR-010/FR-014, `implementation_plan.md:208-217`, `test_plan.md:37` + matrix `TC-SEC-02`, `phase8_pairing_debug.md`, `ble_env_service.c:26` docstring (+ wrong "See DD-008" — DD-008 is OTA), `ble_env_service.h:25,49`, `BleRepository.kt:16`.
  - **Fix:** make everything defer to `security_model.md`; fix `SECURITY.md` first.
- **C2 [High] `phase8_pairing_debug.md` self-contradictory.** Declares attempt 19 "RESOLVED" with **Just Works**, yet trailing "What has NOT been tried yet" / "Next step: clear nRF Connect app data" reads as still-broken — and the whole thing was superseded by MITM passkey (DD-020). Reconcile or mark "historical."
- **C3 [Med] Every headline count disagrees.**
  - Unity tests: README "8 env_sensor + encode" / "19" / "20"; RELEASE_NOTES "37"; review report "19"; tree has **62 written / 37 actually run** (see TEST-COLLISION).
  - Manual matrix: README "19"/"20" vs **25 rows**. Also `TC-SEC-02` (Just Works) and `TC-SEC-05` (MITM) are **both marked Pass** — impossible on one build.
  - Binary size: `0x94f00` vs `0x99520` vs `0x98410` vs actual **`0x95b80`**.
- **C4 [Med] `RELEASE_NOTES_v1_0_0.md` errors.** Scrambled DD cross-refs (DD-001/002/003/004/015 all mislabeled); "Embedded weights: …/`model_data.h`" (wrong file — it's `ml_weights.h`); "245 weights fit in IRAM" (they're flash `.rodata`); "20-entry history" (code shows `take(50)` in `DataAlertsScreen`); "TC-001–TC-011" (matrix has 012).
---

## D. Repo hygiene

- **D1** Dead checked-in artifacts: `firmware/.../model_data.cc` (uncompiled), `ml/models/{model.tflite, model_quantized.tflite, saved_model/}` (none used by what ships). Small; more "implies a pipeline that isn't wired" than bloat. `.gitignore` `ml/models/` + a regenerate note, or keep only what's used.
- **D2** Hardcoded BLE static address `0xC2:01:EF:BE:AD:DE` (`ble_env_service.c:494`) → every flashed unit shares one MAC. Fine for one PoC; `SECURITY.md:9` already flags it.
- **D3** `.venv/`, `build/`, `.idea/`, `local.properties` correctly gitignored. (Good.)

---

## E. Gaps / suggested additions

- **E1 No CI.** `.github/workflows/` is only a placeholder README. Add a GitHub Action: build firmware (Espressif Docker), `./gradlew assembleDebug`, run `ml/verify_model.py`. Build-only CI alone would catch most drift found here.
- **E2 No host-runnable tests.** All Unity tests require on-target flashing → can't cheaply gate a PR. Formatters/encoders/`tinyml_infer` are pure C and could run on host in ms. (Missing leg of the TDD setup.)
- **E3 No real-sensor validation.** Entire ML headline untested on real data (on roadmap; mark accuracy provisional).
- **E4 Android robustness.** `onConnectionStateChange` ignores GATT `status` (133 errors); no Bluetooth-off UX; no connect timeout.
- **E5 Extend `issues_encountered.md`.** It stops at Phase 2; the Phase 8 pairing saga and Phase 9 ML pivot are the richest lessons and currently only live in the contradictory `phase8_pairing_debug.md`.

---

## Priority order (if only a few things)

1. **Finish TEST-COLLISION fix** (in progress) — restore real test coverage; then run on-target to confirm 62 pass.
2. **One pairing story** (MITM Passkey Display) across the repo, starting `SECURITY.md` + `gatt_profile.md` (C1).
3. **ML reproducibility** — add `extract_weights.py` (B1); reconcile 98.83% vs 99.7% (B2); propagate the honest accuracy caveat (B3).
4. **Defer NVS write out of BLE callback** (A1).
5. **Reconcile counts/sizes** in docs to verified values: binary `0x95b80`, 62 Unity tests, 25 manual TCs (C3).

---

## Strengths (keep doing)

- `docs/issues_encountered.md` — honest, specific root-cause writeups.
- `docs/power_budget.md` — correctly explains the ESP32-C3 `NO_LIGHT_SLEEP` PM-lock; honest bench estimates.
- README MCU honesty (ESP32-C3 vs nRF52840).
- TDD discipline on pure logic (display formatters) — *where it actually runs*.
- Android BLE correctness: sequential CCCD write queue, API-33 read/write split, bond-state receiver, GATT-cache refresh.
- `app_state.c` clean spinlock-guarded state with atomic snapshots.

---

## Scope of this review

Deep-read: all firmware C (`app_core`, `ble_env`, `env_sensor`, `display`, `tinyml_inference`, `main`), Android BLE/VM layer + manifest, full ML pipeline (`ml/*.py`), and every claim-bearing doc. Built main + test app; verified on-target test execution + binary size on the C3.

Not line-audited: `ssd1306.c`/`font_big.c` (hardware/bitmap, TDD-exempt), individual Compose screens beyond `DataAlertsScreen`, `CsvExporter.kt`, `build_and_flash.md`/`debug_guide.md`, learning guides (grepped not full-read), `docs/superpowers/` planning docs. On-target pass/fail of the 25 currently-non-running tests is **unknown** until the rename fix is verified.
