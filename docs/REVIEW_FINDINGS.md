# Independent Review Findings & Fix Log

**Date:** 2026-05-29 (created); last updated 2026-06-01 end-of-session-5.
**Branch:** main (HEAD = post-session-5; session-5 was a single on-target verification pass, no code changes; sessions 1-4 added 40 commits, session 5 adds this doc update).
**Reviewer:** principal-level pass (independent of `docs/principal_review_report.md`, retired to a pointer stub in session 3 — C7).
**Purpose:** Durable record of issues found, what's verified on hardware, and what needs fixing. Safe to resume from this file in a new session.

---

## Session 2026-06-01 (evening, session 5) — on-target verification pass

**No code changes this session.** Closed out the on-target verifications that had been carried over from sessions 2-4 (A1, A4+DD-021, D2). Board: ESP32-C3 on `/dev/ttyACM0`. Firmware `0x95d60` flashed from current `main` HEAD (no rebuild between gates).

| Verification | TC | Outcome |
|---|---|---|
| Unity re-run after D2's `ble_env_service.c` touch | — | `62 Tests / 0 Failures / 1 Ignored / OK` |
| D2 — per-device random-static address | TC-SEC-05 | Advertised MAC differs from old hardcoded `C2:01:EF:BE:AD:DE`; MITM passkey pair from a freshly-cleared phone bond succeeded; `Encryption established`. |
| D2 — bond reconnect across reboot | TC-SEC-06 | After hard reset, reconnect with no passkey prompt (eFuse MAC is stable across boots → bond key still resolves). |
| A1 — deferred NVS write behaviour-preserving | TC-010 | Wrote Configuration `01 00 E8 03` (interval 1000 ms); notification cadence changed to ~1 Hz without visible write stall. |
| A1 — config persistence across reboot | TC-012 | Hard-reset board, reconnected bonded, read Configuration → `01 00 E8 03` (interval persisted). Confirms the dirty-flag drain in `telemetry_task` reaches NVS before next boot. |
| A4 — checked `ble_gap_conn_find` rc + DD-021 shared-static access | TC-009 | Disconnect/reconnect/notify-subscribe cycle ×3-4 with OLED page rotation running concurrently. ADV resumed cleanly after every disconnect; no GATT 133/8/19 on any reconnect; page rotation stayed smooth (s_last_page coherent under churn). |

> Note: REVIEW_FINDINGS sessions 2-4 referred to "TC-006 (write report interval) + TC-011 (reboot persistence)" for A1, but the canonical IDs in `docs/test_plan.md` are **TC-010** (Config Write Valid) and **TC-012** (Persistence). TC-011 in the matrix is the *invalid* config write (negative test). Recorded here for future readers; no action needed.

All four blocking on-target verifications now closed. The only remaining "✅ Verified" gaps are TC-011 (negative — invalid config rejection) and the Phase 9 TC-ML-* class boundaries; both were already Pass in the matrix and were not affected by sessions 2-4 commits.

**Resume point unchanged from session 4:** the three deferred/optional items (T11 / T17 / E3) are the only remaining open work. See the session-4 wrap below for the breakdown.

---

## Session 2026-06-01 (afternoon, Phase D) — wrap & resume point

**Closed this session (5 punch-list items, 12 commits all pushed):** E5, E1, B4+D1, D2, E4 — the entire Phase D long tail from `docs/superpowers/plans/2026-05-30-review-findings-cleanup.md`. One Opus orchestrator + five Sonnet implementer subagents (one per task, fresh context each). Same per-task atomicity as session 3: each task = work commit + plan Status-tick commit, both pushed before the next task started. Plan extension commit (`56608a7`) added Phase D rows up front so any `/clear` mid-pass would resume cleanly.

| Item | Commit | Summary |
|---|---|---|
| E5 | `b0b503b` | `docs/issues_encountered.md` extended past Issue 9 (Phase 2): Issue 10 — Phase 8 pairing saga (root causes: `ble_store_config_init()` never called + NimBLE host task stack 4096→8192 B for SC ECDH; why `ble_gap_update_params()` was permanently removed for ENC_CHANGE race; why Security Request timer is permanently banned). Issue 11 — Phase 9 ML pivot (AE on comfortable-only was a category error → `max(softmax) < 0.5 → ANOMALY` per DD-019). |
| E1 | `0d9bcfa` | GitHub Actions CI added: `espressif/esp-idf-ci-action@v1` builds `firmware/` + `firmware/test_app/` (ESP-IDF v5.2.3, target esp32c3); Gradle `assembleDebug` for Android (JDK 17); ML TensorFlow smoke test (continue-on-error). Placeholder `workflows/README.md` deleted. |
| B4+D1 | `295295c` | Deleted dead ML artifacts (`model_data.cc`, `model.tflite`, `model_quantized.tflite`, `quantize.py`). Rewrote `verify_model.py` to test `saved_model` directly via `tf.saved_model.load`. Stripped TFLite output from `train_classifier.py`. Updated `architecture.md` ml/ block + `.gitignore` (covers `ml/models/*.tflite`). Canonical retrain loop is now `collect_synthetic.py → train_classifier.py → saved_model/ → extract_weights.py → ml_weights.h`. Firmware byte-identical (model_data.cc was never compiled). |
| D2 | `7f33400` | BLE random-static address now derived per-device from `esp_efuse_mac_get_default()` + top 2 bits set (BT spec); SECURITY.md updated. Bonds from prior firmware invalidated — one-time re-pair required. Firmware +16 B. |
| E4 | `ca1cffe` | Android: `onConnectionStateChange` now checks GATT status (133/8/19 → `DeviceState.Error`, not silent disconnect); `DeviceState` gains `Connecting` / `BluetoothOff` / `Error(gattStatus, msg)`; 10 s connect timeout via `Handler`; `BluetoothAdapter.ACTION_STATE_CHANGED` receiver. Minimal UI chip on `ScanScreen` + `DashboardScreen` for the new states. Gradle build green, lint flat vs baseline. |

Plus 5 per-task Status-tick commits on the plan file (`feec25a`, `61f1d75`, `4448320`, `9f2c187`, `6dda402`), one SHA-backfill (`dc11ffe`), and the Phase D plan extension (`56608a7`) — 12 session-4 commits total.

**Build state at end of session:** firmware `0x95d60` (was `0x95d50` at session-4 start; +16 B net, all from D2's eFuse MAC read). test_app `0x373c0` (unchanged, links green). Android `./gradlew assembleDebug` green in ~26 s. No on-target re-verify done this session.

**Pending on-target verifications carried over from sessions 2 + 3 + 4:** ✅ all closed in session 5 — see the session-5 wrap above. Original carry-over list (kept for traceability):
- ~~TC-006 (write report interval) + TC-011 (reboot persistence)~~ → done as TC-010 + TC-012 (correct IDs per test_plan.md).
- ~~TC-009 (notify subscribe/unsubscribe across reconnects)~~ → done, A4 + DD-021 hold under churn.
- ~~TC-SEC-05/06 (MITM Passkey Display) on a fresh phone~~ → done, MAC differs from old hardcoded; pair + bond reconnect both clean.
- ~~All 62 Unity tests~~ → done, `62 / 0 / 1 / OK` after D2's touch.

**Resume point for next session:** All medium/high punch-list items are now closed. Remaining open work is entirely optional or hardware-bound:
1. **T11 (B2 path-a)** — retrain classifier + redeploy weights to resolve the saved_model-vs-deployed mismatch surfaced in session 2's B1. OPTIONAL; needs Python TF venv + on-target TC-ML-* re-verify. Plan task at `docs/superpowers/plans/2026-05-30-review-findings-cleanup.md` § T11.
2. **T17 (E2)** — host-runnable tests for pure-C modules (encoders, validators, formatters). New infrastructure: mock ESP-IDF/FreeRTOS stubs, host CMake target, CI integration. DEFERRED — beyond a single-session budget; tracked in plan § T17.
3. **E3** — real-sensor validation on a physical BME280/BMP280. SKIPPED — requires hardware that isn't in this rig.

---

## Session 2026-06-01 (morning) — wrap (historical)

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

**Build state at end of session 3:** firmware `0x95d50` (was `0x95cb0` at session-3 start; +160 B net, all from A4's log string — A6 deleted preprocessor constants (zero bytes), B5's AE arrays were already linker-stripped). test_app `0x373c0` (unchanged, links green). No on-target re-verify done this session.

**New DD in session 3:** DD-021 in `docs/design_decisions.md` (shared-static access pattern on single-core ESP32-C3 — single-word atomic; check-then-use races inherent and bounded; multi-field locks belong to compound state, not single-word).

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

## Remaining open items (current state — 2026-06-01 end-of-session-5)

All original A/B/C/D/E findings and TEST-COLLISION / TEST-ENCODE-BROKEN are closed (see session 2-5 wraps above for per-item commit SHAs; per-item closure markers are inline in sections A-E below). Three items remain off the table:

- **T11 / B2 path-a** (OPTIONAL) — retrain classifier + redeploy weights to close the saved_model-vs-deployed mismatch surfaced in session 2's B1. Single-session feasible if Python TF venv works; changes deployed model behaviour → needs full TC-ML-* re-verify. Plan task at `docs/superpowers/plans/2026-05-30-review-findings-cleanup.md` § T11.
- **T17 / E2** (DEFERRED) — host-runnable tests for pure-C modules (encoders, validators, formatters). New infrastructure (mock ESP-IDF/FreeRTOS stubs, host CMake target, CI integration). Beyond a single-session budget; tracked in plan § T17.
- **E3** (SKIPPED) — real-sensor validation on a physical BME280/BMP280. Requires hardware not in the rig.

---

## ✅ Verified on hardware / by clean build (2026-05-29 — original snapshot)

> **Historical snapshot at the time of the original review.** Current verified state is in the session-5 wrap at the top: firmware **`0x95d60`** (after A1 +304 B, A4 +160 B, D2 +16 B; A6/B5 zero net), Unity **62/0/1** still, manual matrix **24/25 Pass + 1 Obsolete**. The C3 binary-size and TC-count mismatches in the table below were all fixed in session 2's C3 (`3b8acc2`).

Board: ESP32-C3 on `/dev/ttyACM0` (MAC 38:44:be:44:c0:a8). ESP-IDF v5.2.3.

| Claim in docs | Verdict | Evidence |
|---|---|---|
| Firmware builds green | ✅ TRUE | `idf.py build` of `firmware/` and `firmware/test_app/` both link cleanly from current `main` |
| Binary size `0x94f00`/`0x99520`/`0x98410` | ❌ all wrong (fixed in C3) | linker at review time: **`0x95b80` bytes** — 58% used / 42% free of the 0x100000 app partition |
| Unity suite "37 tests / all pass" | ✅ FIXED → 62 | Was 37 (display only) due to TEST-COLLISION; after the rename + encoder fix, on-target = **`62 Tests 0 Failures 1 Ignored / OK`** (2026-05-29) |
| Manual test matrix "19"/"20" cases | ❌ wrong (fixed in C3) | matrix actually has **25 rows** (separate from Unity tests) |
| On-target "0 failures" | ✅ TRUE after fix | all 62 pass (1 ignored placeholder); the 25 formerly-dropped tests — incl. the frozen-GATT encode lock — now run green |

---

## ✅ TEST-COLLISION (top finding — RESOLVED 2026-05-29, `e1ed479`) — historical writeup of the original symptom

**Symptom:** On-target `idf.py flash monitor` of `test_app` runs only the **37 display tests** (`test_display_logic` 36 PASS + `test_display_pending` 1 IGNORE) → `37 Tests 0 Failures 1 Ignored / OK`. Looks healthy; isn't.

**Root cause (confirmed):** In ESP-IDF a component's name is its **directory basename**, which must be unique. All four test dirs were named `test`, so they collide. `EXTRA_COMPONENT_DIRS` listed them ending with `display/test`, so the build **silently kept only the last** and dropped the other three. No build error.

**Proof:** `firmware/test_app/build/project_description.json` had exactly one component named `test` → `/components/display/test`; `app_core/test`, `ble_env/test`, `env_sensor/test` appeared nowhere in the build graph.

**Impact — 25 of 62 written tests never compiled/ran**, including the locks on the *frozen contracts the project cares most about*:

| Component | Dropped tests | Guards |
|---|---|---|
| `ble_env` | `test_ble_env_encode` (3) | **Frozen GATT byte layouts** (telemetry 16-byte / status 6-byte encoders) |
| `env_sensor` | `test_sensor_provider` (2), `test_sensor_override` (6) | DD-017 ±2 °C override/drift contract |
| `app_core` | `test_app_state` (6), `test_storage_config` (3), `test_power_mode` (5) | State machine, NVS parse, sequence counter |

**Fix shipped 2026-05-29 (`e1ed479`):** four dir renames (`test/` → `test_<name>/`) + `firmware/test_app/CMakeLists.txt` EXTRA_COMPONENT_DIRS update. On-target Unity then went 37 → **62 / 0 / 1 / OK**. Re-verified again session 5 after D2's `ble_env_service.c` touch: still 62 / 0 / 1 / OK.

**Lesson:** ESP-IDF component name = dir basename, must be unique; collisions dedupe silently. A green "OK / 0 failures" covered only 60% of the suite. `compiles` ≠ `runs`.

> NOTE: my earlier claim that "all 8 test files link / 62 tests" was wrong — I inferred it from CMake `SRCS` lists. Only the on-target run exposed the truth.

---

## ✅ TEST-ENCODE-BROKEN (RESOLVED 2026-05-29, same commit as TEST-COLLISION) — historical writeup

After the rename fix, the test build fails at link:
```
test_ble_env_encode.c:32: undefined reference to `encode_telemetry'
test_ble_env_encode.c:62: undefined reference to `encode_status'
```
`test_ble_env_encode.c` (3 tests — the regression lock on the **frozen** telemetry 16-byte / status 6-byte layouts) calls `encode_telemetry()`/`encode_status()`, which are **`static`** in `ble_env_service.c` (lines 76, 91).

The test's own header comment (lines 5-9) says this is intentional: a TDD **red** test whose encoder "promotion … will happen in Phase 3 with explicit user approval." **That promotion never happened**, and TEST-COLLISION silently dropped the test from the build, so the broken state was invisible and the docs' "ble_env encode tests pass" was never true. **These 3 tests have never executed.**

**Fix (needs approval — edits `ble_env_service.c`):** give `encode_telemetry`/`encode_status` external linkage + a shared declaration. Recommended: declare both in a small internal header (`ble_env_encode.h`, or in `ble_env_service.h`), `#include` from both `ble_env_service.c` and the test, drop `static`. Minimal: just remove `static` (the test already forward-declares them, lines 19-20). Then rebuild → expect link success and the 3 encode tests to run and pass (they assert the exact frozen bytes, e.g. 2456 → `0x0998` LE at offset 8). This completes a real red→green TDD cycle and turns on the only automated check of the frozen GATT contract.

**STATUS: FIXED 2026-05-29** — `static` removed (approved) in commit `e1ed479`; the 3 encode tests register, link, and pass on-target as part of the 62/0/1 run. Re-verified session 5.

---

## A. Correctness / code issues

> All A-series items are closed. A1 is preserved below in long form; A2–A7 were tracked only in the original punch-list summary and are all closed in session 3 (see session-3 wrap table for SHAs: A2 `baa6403`, A3 `2ac5825`, A4 `178f180`, A5 `3a119d9`, A6 `294bf6b`, A7 `449efdd`). A1, A4, A5 also have on-target re-verification per the session-5 wrap.

- **A1 [High] Blocking flash write inside a BLE callback.** ✅ **CLOSED session 2** (`a3ef354`); on-target re-verified session 5 (TC-010 + TC-012). *Original symptom:* `ble_env_service.c:151` called `storage_config_save()` (NVS flash write, tens of ms) synchronously inside `gatt_access_cb`, violating AGENT_BRIEF #7, NFR-003, DD-006. *Fix shipped:* dirty-flag mirror of `force_sample` — NVS write deferred to `telemetry_task`.

---

## B. ML pipeline (highest learning value)

> B1, B2 (doc layer), B3, B4, B5 all closed. The only B-item still open is **B2 path-a** (optional retrain — see "Remaining open items" above).

- **B1 [High] Deployed model can't be regenerated.** ✅ **CLOSED session 2** (`53c27fd`) — added `ml/extract_weights.py` (loads `models/saved_model`, writes the six `ML_W*` / `ML_b*` arrays). Smoke test surfaced a real saved_model-vs-deployed mismatch (tracked separately as B2 path-a). Cross-refs in `train_classifier.py` and `architecture.md` fixed in the same commit.
- **B2 [High] Accuracy numbers disagree.** ✅ **CLOSED at doc layer session 2** (`0f5adf8`) — reconciled to **98.83%** (deployed-model truth) everywhere. *Path-a retrain remains optional* — would close the B1-surfaced saved_model-vs-deployed mismatch but needs HW re-verify of TC-ML-*.
- **B3 [High / key lesson] "99.7%" measures box-separability, not skill.** ✅ **CLOSED session 2** (`0f5adf8`) — box-separability caveat propagated to every site that cites the accuracy (`tinyml_inference.h`, `README.md`, `RELEASE_NOTES_v1_0_0.md`).
- **B4 [Med] Dead ML artifacts presented as pipeline.** ✅ **CLOSED session 4** (`295295c`) — deleted `model_data.cc`, `model.tflite`, `model_quantized.tflite`, `quantize.py`; rewrote `verify_model.py` to test the `saved_model` path; canonical pipeline now `collect_synthetic → train_classifier → saved_model → extract_weights → ml_weights.h`. Firmware byte-identical (the deleted files were never compiled).
- **B5 [Med] Dead autoencoder arrays in `ml_weights.h`.** ✅ **CLOSED session 3** (`85571ca`) — `ML_AE_*` arrays + `ML_AE_HIDDEN_SIZE` + `ML_ANOMALY_THRESHOLD` removed (DD-019 made them dead). Binary size unchanged — linker `--gc-sections` had already stripped them.

---

## C. Documentation consistency (dominant problem)

> All C-series items closed across sessions 2 and 3.

- **C1 [High] "Just Works" vs "MITM Passkey Display" split.** ✅ **CLOSED session 2** (docs: `08d6420`, source comments: `fa0e802`) — every site now defers to `docs/security_model.md`; grep for "Just Works" in firmware/Android source is clean.
- **C2 [High] `phase8_pairing_debug.md` self-contradictory.** ✅ **CLOSED session 2** (`08d6420`) — historical banner added; superseded by MITM passkey (DD-020).
- **C3 [Med] Every headline count disagrees.** ✅ **CLOSED session 2** (`3b8acc2`) — at the time: binary `0x95cb0`, Unity 62/0/1, manual 24/25 + 1 Obsolete. Post-session-4 binary is `0x95d60` (drift recorded in the session-5 wrap snapshot banner above).
- **C4 [Med] `RELEASE_NOTES_v1_0_0.md` errors.** ✅ **CLOSED session 2** (`aa79796`) — DD cross-refs fixed, `model_data.h` → `ml_weights.h`, "IRAM" → flash `.rodata`, "20-entry history" → `take(50)`, TC range `TC-001–TC-012`.
- **C5 [Low] OLED page spec stale in requirements.md FR-011.** ✅ **CLOSED session 3** (`d4463cc`) — FR-011 now lists `{temperature, humidity, pressure} @ 2000 ms` + persistent BLE-state badge to match `display.c`.
- **C6 [Low] `architecture.md` half-stale.** ✅ **CLOSED session 3** (`49d0f0f`) — single canonical 5-component Module Layout at the top; Phase 9 Extensions subsection collapsed to a pointer up; stale "display TBD — Phase 1.5" removed.
- **C7 [Low] `principal_review_report.md` is itself stale.** ✅ **CLOSED session 3** (`6a55cb7`) — body replaced with a 4-line pointer stub to this doc.

---

## D. Repo hygiene

- **D1 [Low] gitignore / regenerate note for dead `ml/models/*`.** ✅ **CLOSED session 4** (`295295c`, bundled with B4) — `.gitignore` extended to cover `ml/models/*.tflite`; canonical regenerate path documented in `architecture.md`.
- **D2 [Low] Hardcoded BLE static address.** ✅ **CLOSED session 4** (`7f33400`); on-target re-verified session 5 (TC-SEC-05 + TC-SEC-06). Address now derived per-device from `esp_efuse_mac_get_default()`.
- **D3** `.venv/`, `build/`, `.idea/`, `local.properties` correctly gitignored. (No change needed; was already good at original review.)

---

## E. Gaps / suggested additions

- **E1 [Med] No CI.** ✅ **CLOSED session 4** (`0d9bcfa`) — GitHub Actions workflow: ESP-IDF builds (firmware + test_app), Gradle `assembleDebug` for Android, TF smoke for ML (continue-on-error).
- **E2 [Med] No host-runnable tests.** ⏸️ **DEFERRED** (plan task T17) — beyond a single-session budget; needs mock ESP-IDF/FreeRTOS stubs, host CMake target, CI integration.
- **E3 [Low] No real-sensor validation.** ⏸️ **SKIPPED** — requires a physical BME280/BMP280 not in the rig.
- **E4 [Med] Android BLE robustness gaps.** ✅ **CLOSED session 4** (`ca1cffe`) — `onConnectionStateChange` checks GATT status (133/8/19 → `DeviceState.Error`); new `Connecting` / `BluetoothOff` / `Error` states; 10 s connect timeout; `ACTION_STATE_CHANGED` receiver.
- **E5 [Low] Extend `issues_encountered.md`.** ✅ **CLOSED session 4** (`b0b503b`) — Issue 10 (Phase 8 pairing saga) + Issue 11 (Phase 9 ML pivot).

---

## Priority order (historical 2026-05-29 ranking — all closed)

Original "if only a few things" list, preserved for audit. Every item is closed; current open items are in "Remaining open items" above.

1. ~~Finish TEST-COLLISION fix~~ — done 2026-05-29 (`e1ed479`); 62 Unity tests on-target re-verified session 5.
2. ~~One pairing story (MITM Passkey Display) across the repo~~ — done session 2 (C1: `08d6420` + `fa0e802`).
3. ~~ML reproducibility — `extract_weights.py` (B1), accuracy reconciliation (B2), box-separability caveat (B3)~~ — done session 2 (B1: `53c27fd`, B2+B3: `0f5adf8`). B2 path-a retrain remains optional.
4. ~~Defer NVS write out of BLE callback (A1)~~ — done session 2 (`a3ef354`); on-target re-verified session 5 (TC-010 + TC-012).
5. ~~Reconcile counts/sizes (C3)~~ — done session 2 (`3b8acc2`); binary then `0x95cb0`, now `0x95d60`.

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

Not line-audited at original review time (2026-05-29): `ssd1306.c`/`font_big.c` (hardware/bitmap, TDD-exempt), individual Compose screens beyond `DataAlertsScreen`, `CsvExporter.kt`, `build_and_flash.md`/`debug_guide.md`, learning guides (grepped not full-read), `docs/superpowers/` planning docs. The 25 then-non-running Unity tests were verified pass on-target the same day after the TEST-COLLISION rename fix (and re-verified session 5).
