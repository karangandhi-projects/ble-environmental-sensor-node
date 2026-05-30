# Review Findings Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. **Each task names the model it should be dispatched to and is fully self-contained — a fresh subagent with no prior context can execute it from the plan alone.**

**Goal:** Close the remaining open items on `docs/REVIEW_FINDINGS.md` ("Remaining punch-list") in a single coordinated pass, with each task sized so a Haiku, Sonnet, or Opus agent can complete it efficiently and the plan stays resumable across `/clear` and session boundaries.

**Architecture:** Tasks are partitioned into three phases by cognitive load:
- **Phase A (Haiku)** — purely mechanical edits; exact before/after text in the task.
- **Phase B (Sonnet)** — small source/doc edits that need correct context handling and a build/grep verification.
- **Phase C (Opus)** — concurrency analysis and ML retraining; multi-file reasoning + on-target re-verify.

**Tech Stack:** ESP-IDF v5.2.3 / NimBLE / C / Kotlin (untouched here) / Python TensorFlow (T11 only).

---

## Execution model (READ FIRST)

This plan is built so each task is one **atomic, resumable, push-checkpointed unit**. The orchestrator (or you) dispatches one task at a time as a **fresh subagent** with a clean context window. When the subagent finishes, it has:

1. Committed its changes locally.
2. Pushed to `origin/main`.
3. Ticked the matching row in the **Status table below** (the plan file is the resume marker — it gets pushed too).

Then the subagent's context is discarded. The next task starts fresh from this plan file, reads the Status table, and picks the first un-ticked task.

**If the orchestrator session itself runs out of context mid-plan:** the user runs `/clear`, re-opens, and says "resume the plan at `docs/superpowers/plans/2026-05-30-review-findings-cleanup.md`." The Status table tells the new orchestrator session which task to dispatch next. Zero progress lost.

**Dispatch instruction template** (paste into `Agent` tool when dispatching a task):

> You are completing **Task T<N>** of the plan at `/home/karan-gandhi/ble_skill_project_package_reviewed/docs/superpowers/plans/2026-05-30-review-findings-cleanup.md`. Read the plan file, find the task by ID, execute every step exactly as written, including the trailing "commit → push → tick Status" steps. Do not start a new task; do not skip the push; do not skip the Status update. If you hit an approval gate (🔒 flag), surface the diff to the user and wait. Report back the commit SHA when done.

**Approval gate (project rule from `CLAUDE.md`):** Any edit to an existing **source** file (`.c`, `.h`, `.py` under `ml/`, Android `.kt`) requires explicit user approval before the change is made. Doc-only edits (`docs/**`, `*.md` at repo root) are free. Each task below flags **🔒 SOURCE EDIT — needs approval** when it applies; the subagent must stop and request approval with a one-line diff summary.

**Common verification pattern** (run from `firmware/` after any C edit):
```bash
source ~/esp/esp-idf/export.sh
idf.py build                               # main app must stay green
( cd test_app && idf.py build )            # test app must stay green
```
Baseline at plan start: firmware `0x95cb0`, test_app `0x373c0`.

**Tick-status template** — after `git push`, every task ends with this exact step:

```bash
# Open the plan file and change `- [ ]` to `- [x]` for the row matching this task ID,
# add the commit SHA to the "Commit" column, and append a one-line note to "Done".
# Then:
git add docs/superpowers/plans/2026-05-30-review-findings-cleanup.md
git commit -m "plan: tick T<N> ($BRIEF_NOTE) — <commit-sha>"
git push
```

---

## Status (single source of truth — update at the end of every task)

| ID  | Phase | Model  | Item       | Files                                                         | Status | Commit | Notes |
|-----|-------|--------|------------|---------------------------------------------------------------|--------|--------|-------|
| T1  | A     | Haiku  | A7         | `firmware/components/env_sensor/sensor_provider.c`            | `- [x]` | `449efdd` | A7 closed |
| T2  | A     | Haiku  | C5         | `docs/requirements.md`                                        | `- [x]` | `d4463cc` | C5 closed |
| T3  | B     | Sonnet | A2         | `README.md`                                                   | `- [x]` | `baa6403` | A2 closed |
| T4  | B     | Sonnet | A3         | `README.md`                                                   | `- [x]` | `2ac5825` | A3 closed |
| T5  | B     | Sonnet | C6         | `docs/architecture.md`                                        | `- [x]` | `49d0f0f` | C6 closed |
| T6  | B     | Sonnet | A4         | `firmware/components/ble_env/ble_env_service.c`               | `- [x]` | `178f180` | A4 closed |
| T7  | B     | Sonnet | A6         | `firmware/components/app_core/include/app_config.h`, DD-015, power_budget.md | `- [x]` | `294bf6b` | A6 closed |
| T8  | B     | Sonnet | B5         | `firmware/components/tinyml_inference/include/ml_weights.h`, `ml/extract_weights.py` | `- [x]` | `85571ca` | B5 closed |
| T9  | B     | Sonnet | C7 (full)  | `docs/principal_review_report.md`                             | `- [x]` | `6a55cb7` | C7 closed |
| T10 | C     | Opus   | A5         | `ble_env_service.c`, `display.c`, `design_decisions.md`       | `- [x]` | `3a119d9` | A5 closed via DD-021 (Option 1) |
| T11 | C     | Opus   | B2 path-a (OPTIONAL) | `ml/`, `ml_weights.h`, README, RELEASE_NOTES         | `- [ ]` | —      |       |

**Recommended order:** T1 → T2 in parallel (both Haiku, no file overlap), then T3 → T4 → T5 → T6 → T7 → T8 → T9 (serial; T3 and T4 share README.md so they cannot parallelize), then T10, then T11 if pursued.

**Resume rule:** start at the first row whose Status is `- [ ]`. If all of Phase A/B are done and only T10/T11 remain, the user may choose to stop — the punch-list will be substantively cleared.

---

## Phase A — Haiku tasks

Mechanical edits. Each takes under 5 minutes including build + commit + push.

### Task T1: A7 — fix self-contradictory drift comment in sensor_provider.c

**Model:** Haiku
**Files:**
- Modify: `firmware/components/env_sensor/sensor_provider.c` (header doc-comment line 13)

**🔒 SOURCE EDIT — needs approval.**

**Context:** Header @par block (lines 11-15) says "±2 hPa". Inline comment at line 61 says "±4 hPa (±400 Pa)" and the code (`press_drift = ((t % 5) - 2) * 200;` line 66) produces ±400 Pa. Header is wrong.

- [ ] **Step 1: Request approval**

Print this diff to the user and wait for "yes approve":

```diff
- * Returns the set values plus a ±2°C / ±2% / ±2 hPa drift that cycles
+ * Returns the set values plus a ±2°C / ±2% / ±4 hPa drift that cycles
```

- [ ] **Step 2: Apply the edit**

Edit `firmware/components/env_sensor/sensor_provider.c`. Find:
```
 * Returns the set values plus a ±2°C / ±2% / ±2 hPa drift that cycles
```
Replace with:
```
 * Returns the set values plus a ±2°C / ±2% / ±4 hPa drift that cycles
```

- [ ] **Step 3: Verify build**

```bash
source ~/esp/esp-idf/export.sh
cd firmware && idf.py build
```
Expected: green; binary size unchanged at `0x95cb0` (comment-only).

- [ ] **Step 4: Update REVIEW_FINDINGS.md**

In `docs/REVIEW_FINDINGS.md`:
1. Add a row to the "Closed this session" table at the top: `| A7 | <commit-sha> | sensor_provider.c header "±2 hPa" → "±4 hPa" to match code (±400 Pa) and the inline comment. |`
2. Delete the `**A7 [Low] Self-contradictory drift comment.**` bullet from section A.

- [ ] **Step 5: Commit + push**

```bash
git add firmware/components/env_sensor/sensor_provider.c docs/REVIEW_FINDINGS.md
git commit -m "fix(env_sensor): drift comment ±2 hPa → ±4 hPa (A7)

Header @par block claimed ±2 hPa; inline comment and code both produce
±4 hPa (press_drift = ((t % 5) - 2) * 200 → ±400 Pa). Aligns docstring
with reality."
git push
```

- [ ] **Step 6: Tick Status table in this plan**

Open this file, change the T1 row Status from `- [ ]` to `- [x]`, fill the Commit column with the SHA from Step 5, add `A7 closed` to Notes. Then:

```bash
git add docs/superpowers/plans/2026-05-30-review-findings-cleanup.md
git commit -m "plan: tick T1 (A7 done) — <commit-sha>"
git push
```

---

### Task T2: C5 — fix OLED page spec in requirements.md FR-011

**Model:** Haiku
**Files:**
- Modify: `docs/requirements.md` (FR-011 Acceptance block, lines 86-91)

**Doc-only — no approval needed.**

**Context:** FR-011 lists pages `{BLE runtime state, latest temperature, latest humidity}`. Implemented design (per `docs/architecture.md:150` and `firmware/components/display/display.c`) is three data pages `{temperature, humidity, pressure}` at 2000/2000/2000 ms with BLE-state as a persistent badge on every page.

- [ ] **Step 1: Apply the edit**

Edit `docs/requirements.md`. Find the FR-011 Acceptance block:

```
Acceptance:
- The display cycles three pages: BLE runtime state, latest temperature, latest humidity.
- The BLE state page renders one of `BOOT`, `ADV`, `CONN`, `NOTIFY` matching the current runtime state.
- The temperature and humidity pages show the latest values reported by the telemetry source.
- A `SIM` indicator is visible on the temperature and humidity pages while the telemetry's simulated-data flag (`BLE_ENV_FLAG_SIMULATED_DATA`) is set.
- The 72×40 visible region inside the 128×64 controller frame is honoured (X-offset 28).
```

Replace with:

```
Acceptance:
- The display cycles three data pages: temperature, humidity, pressure (2000 ms each).
- A persistent BLE-state badge on every page renders one of `BOOT`, `ADV`, `CONN`, `NOTIFY` matching the current runtime state.
- Each data page shows the latest value reported by the telemetry source.
- A `SIM` indicator is visible on every page while the telemetry's simulated-data flag (`BLE_ENV_FLAG_SIMULATED_DATA`) is set.
- The 72×40 visible region inside the 128×64 controller frame is honoured (X-offset 28).
```

- [ ] **Step 2: Verify**

```bash
grep -n "three pages" docs/requirements.md
```
Expected: only match is the new "three data pages: temperature, humidity, pressure" line. No "BLE runtime state, latest temperature, latest humidity" wording survives.

- [ ] **Step 3: Update REVIEW_FINDINGS.md**

1. Add to "Closed this session" table: `| C5 | <commit-sha> | requirements.md FR-011 page spec aligned with implementation: {temp, humidity, pressure} @ 2000 ms + persistent state badge. |`
2. Delete the `**C5 [Med] OLED page spec stale.**` bullet from section C.

- [ ] **Step 4: Commit + push**

```bash
git add docs/requirements.md docs/REVIEW_FINDINGS.md
git commit -m "docs(requirements): FR-011 page spec matches implementation (C5)

Listed three data pages {temperature, humidity, pressure} @ 2000 ms
with the BLE-state badge as persistent overlay (not its own page).
Matches display.c and architecture.md:150."
git push
```

- [ ] **Step 5: Tick Status table**

Change T2 row to `- [x]`, fill Commit + Notes, then:

```bash
git add docs/superpowers/plans/2026-05-30-review-findings-cleanup.md
git commit -m "plan: tick T2 (C5 done) — <commit-sha>"
git push
```

---

## Phase B — Sonnet tasks

Single-file edits, some judgement. Each takes 10–20 minutes including build and commit + push.

### Task T3: A2 — fix README SIM-badge claim in override mode

**Model:** Sonnet
**Files:**
- Modify: `README.md` (the "Using Sensor Override as a real-sensor stand-in" paragraph at line 52)

**Doc-only — no approval needed.**

**Context:** `sensor_provider.c:72` sets `.simulated = true` in **both** the default-sim and the override paths. The OLED `SIM` badge therefore stays on while override is active. `README.md:52` currently claims override "clears the `SIM` badge" — wrong. The agreed semantics: only a real on-board BME280/BMP280 driver will clear the badge.

- [ ] **Step 1: Apply the edit**

In `README.md`, find:
```
**Using Sensor Override as a real-sensor stand-in:** If you have a real temperature/humidity/pressure sensor but want to inject readings without wiring it to the ESP32, use the Android companion app's Sensor Override screen. Write actual sensor readings to characteristic `b7e00006` (Sensor Override) via the sliders — the firmware accepts them immediately, clears the `SIM` badge, and uses those values for TinyML classification and telemetry. Writing all-zeros restores the built-in simulation. This means the full firmware stack (TinyML, GATT, OLED, Android app) can be validated with real environmental data without any additional hardware.
```

Replace with:
```
**Using Sensor Override as a real-sensor stand-in:** If you have a real temperature/humidity/pressure sensor but want to inject readings without wiring it to the ESP32, use the Android companion app's Sensor Override screen. Write actual sensor readings to characteristic `b7e00006` (Sensor Override) via the sliders — the firmware accepts them immediately and uses those values for TinyML classification and telemetry. The `SIM` badge stays on because the firmware still flags samples as simulated (`BLE_ENV_FLAG_SIMULATED_DATA = 1`); only a real on-board BME280/BMP280 driver will clear it. Writing all-zeros restores the built-in simulation. This means the full firmware stack (TinyML, GATT, OLED, Android app) can be validated with real environmental data without any additional hardware.
```

- [ ] **Step 2: Verify**

```bash
grep -n "clears the .SIM. badge" README.md
```
Expected: zero matches. `grep -n "SIM badge stays on" README.md` expected: 1 match.

- [ ] **Step 3: Update REVIEW_FINDINGS.md**

1. Add to "Closed this session" table: `| A2 | <commit-sha> | README override paragraph corrected — SIM badge stays on (matches sensor_provider.c setting simulated=true for override). |`
2. Delete the `**A2 [Med] SIM badge never clears in override mode.**` bullet from section A.

- [ ] **Step 4: Commit + push**

```bash
git add README.md docs/REVIEW_FINDINGS.md
git commit -m "docs(readme): correct SIM-badge behaviour in override mode (A2)

Override path in sensor_provider.c sets simulated=true, so the OLED SIM
badge stays on for override readings — only a real BME280 driver will
clear it. README claim that override 'clears the SIM badge' was wrong."
git push
```

- [ ] **Step 5: Tick Status table**

Change T3 row to `- [x]`, fill Commit + Notes. Commit + push the plan file:
```bash
git add docs/superpowers/plans/2026-05-30-review-findings-cleanup.md
git commit -m "plan: tick T3 (A2 done) — <commit-sha>"
git push
```

---

### Task T4: A3 — fix README "realistic drift" claim for default sim

**Model:** Sonnet
**Files:**
- Modify: `README.md` (the "Sensor status" paragraph at line 50)

**Doc-only — no approval needed.**

**Context:** Default simulation in `sensor_provider.c:80-82` is near-constant — `temp = 2450 + (t % 20)` produces 24.50–24.69 °C, etc. The ±2 °C drift only exists in **override mode**. README:50's "with realistic drift" claim is wrong for the default-sim path.

- [ ] **Step 1: Apply the edit**

In `README.md`, find:
```
**Sensor status:** The firmware uses a simulated sensor (with realistic drift) because the physical BME280 was not available during development. The architecture is fully ready for a real sensor — the `env_sensor` component has a swap point, and the `SIM` badge on the OLED and in telemetry flags clears automatically once real data is present.
```

Replace with:
```
**Sensor status:** The firmware uses a simulated sensor because the physical BME280 was not available during development. The default simulation is near-constant (≈24.5 °C / 52 % RH / 1013 hPa with sub-degree variation) — a placeholder, not a synthetic environment. Realistic ±2 °C / ±2 % / ±4 hPa drift is produced only when an external central (e.g. the Android app) writes the Sensor Override characteristic. The architecture is fully ready for a real sensor — the `env_sensor` component has a swap point, and the `SIM` badge on the OLED and in telemetry flags will clear once a real on-board BME280/BMP280 driver lands.
```

- [ ] **Step 2: Verify**

```bash
grep -n "realistic drift" README.md
```
Expected: zero matches.

- [ ] **Step 3: Update REVIEW_FINDINGS.md**

1. Add to "Closed this session" table: `| A3 | <commit-sha> | README sensor-status paragraph: default sim is near-constant; realistic drift exists only under override. |`
2. Delete the `**A3 [Med] Default sim telemetry is near-constant.**` bullet from section A.

- [ ] **Step 4: Commit + push**

```bash
git add README.md docs/REVIEW_FINDINGS.md
git commit -m "docs(readme): default sim is near-constant, not 'realistic drift' (A3)

Default returns temp=2450+(t%20) — that's 0.19 C of variation, not the
±2 C drift the README claimed. Drift exists only under override. README
updated to say so."
git push
```

- [ ] **Step 5: Tick Status table**

Change T4 row to `- [x]`, fill Commit + Notes. Commit + push the plan file:
```bash
git add docs/superpowers/plans/2026-05-30-review-findings-cleanup.md
git commit -m "plan: tick T4 (A3 done) — <commit-sha>"
git push
```

---

### Task T5: C6 — merge architecture.md duplicate component listings

**Model:** Sonnet
**Files:**
- Modify: `docs/architecture.md` (the "## Module Layout" block ≈ lines 38-74 AND the "### Component Map — Updated" subsection inside "## Phase 9 Extensions" ≈ lines 197-212)

**Doc-only — no approval needed.**

**Context:** Doc has two competing component layouts: the top "Module Layout" shows 4 components with `display/` still "TBD — Phase 1.5"; the bottom "Phase 9 Extensions" shows 5 with `tinyml_inference`. The dependency-graph sentence on line 74 lists 4. Decision: keep ONE canonical "Module Layout" at the top showing the 5-component reality (inline the `tinyml_inference` include/ tree there); replace the bottom subsection with a one-line pointer up.

- [ ] **Step 1: Rewrite the canonical "Module Layout" section (≈ lines 38-74)**

Replace the entire "## Module Layout (multi-component, ESP-IDF)" block (the heading, the tree, and the dependency-graph sentence that follows — everything up to but not including "## Runtime State Machine") with:

````markdown
## Module Layout (multi-component, ESP-IDF)

```text
firmware/
├── CMakeLists.txt                  (top-level IDF project)
├── sdkconfig.defaults
├── main/
│   ├── CMakeLists.txt              (REQUIRES app_core ble_env env_sensor display tinyml_inference)
│   └── app_main.c                  (only file in main/)
├── components/
│   ├── app_core/                   (state, storage, app_config.h)
│   │   ├── include/{app_config.h, app_state.h, storage_config.h}
│   │   ├── app_state.c
│   │   ├── storage_config.c
│   │   ├── test_app_core/{test_app_state.c, test_storage_config.c, test_power_mode.c}
│   │   └── CMakeLists.txt          (REQUIRES nvs_flash)
│   ├── ble_env/                    (NimBLE GATT v2 server — 6 characteristics)
│   │   ├── include/ble_env_service.h
│   │   ├── ble_env_service.c
│   │   ├── test_ble_env/test_ble_env_encode.c
│   │   └── CMakeLists.txt          (REQUIRES bt app_core env_sensor)
│   ├── env_sensor/                 (sensor provider — simulated + override + ±2°C drift)
│   │   ├── include/sensor_provider.h
│   │   ├── sensor_provider.c
│   │   ├── test_env_sensor/{test_sensor_provider.c, test_sensor_override.c}
│   │   └── CMakeLists.txt          (REQUIRES esp_timer)
│   ├── display/                    (SSD1306 driver + page rotator)
│   │   ├── include/{display.h, ssd1306.h, font_big.h}
│   │   ├── display.c, ssd1306.c, font_big.c
│   │   ├── test_display/{test_display_logic.c, test_display_pending.c}
│   │   └── CMakeLists.txt
│   └── tinyml_inference/           (pure-C MLP classifier — Phase 9C)
│       ├── include/
│       │   ├── tinyml_inference.h  (public API: ml_class_t, ml_result_t, tinyml_infer)
│       │   └── ml_weights.h        (245 floats: W1/b1/W2/b2/W3/b3, embedded at compile time)
│       └── tinyml_inference.c      (dense + ReLU + softmax + anomaly threshold)
└── test_app/                       (unit-test-app project for on-target Unity)
    ├── CMakeLists.txt
    └── main/{test_main.c, CMakeLists.txt}
```

Component dependency graph: `main → app_core, ble_env, env_sensor, display, tinyml_inference`; `ble_env → app_core, env_sensor`; `display → app_core, env_sensor`; `tinyml_inference` has no internal deps (pure C math on flash-resident constants). Cross-component coupling is explicit via `REQUIRES`.
````

- [ ] **Step 2: Collapse the "Phase 9 Extensions — Component Map" subsection**

Find the "### Component Map — Updated" subsection (still inside "## Phase 9 Extensions (2026-05-28)"). Replace its entire body (the `firmware/components/` tree + intro sentence) with:

```
### Component Map — Updated

See the canonical "Module Layout" section above; `tinyml_inference` is included there. The Phase 9 work added one new component and did not change the dependencies of the existing four.
```

- [ ] **Step 3: Verify no duplicate component listings**

```bash
grep -c "tinyml_inference/" docs/architecture.md
```
Expected: **1**. Any other count → a duplicate slipped through.

```bash
grep -n "TBD — Phase 1.5\|TBD - Phase 1.5" docs/architecture.md
```
Expected: zero matches (was 1).

- [ ] **Step 4: Update REVIEW_FINDINGS.md**

1. Add to "Closed this session" table: `| C6 | <commit-sha> | architecture.md: single canonical 5-component Module Layout; Phase 9 Extensions subsection collapsed to a pointer up. Stale "display TBD" removed. |`
2. Delete the `**C6 [Low] `architecture.md` half-stale.**` bullet from section C.

- [ ] **Step 5: Commit + push**

```bash
git add docs/architecture.md docs/REVIEW_FINDINGS.md
git commit -m "docs(architecture): merge dual component listings (C6)

Top 'Module Layout' tree was 4 components with display still TBD;
bottom 'Phase 9 Extensions' tree had the real 5. Promoted the
5-component view to the canonical block, collapsed the duplicate to a
pointer up, dropped the 'display TBD — Phase 1.5' relic."
git push
```

- [ ] **Step 6: Tick Status table**

Change T5 row to `- [x]`, fill Commit + Notes:
```bash
git add docs/superpowers/plans/2026-05-30-review-findings-cleanup.md
git commit -m "plan: tick T5 (C6 done) — <commit-sha>"
git push
```

---

### Task T6: A4 — handle unchecked ble_gap_conn_find return in gap_event_cb

**Model:** Sonnet
**Files:**
- Modify: `firmware/components/ble_env/ble_env_service.c` around lines 374-380 (inside `case BLE_GAP_EVENT_CONNECT`)

**🔒 SOURCE EDIT — needs approval.**

**Context:** Current code path:
```c
struct ble_gap_conn_desc _desc;
ble_gap_conn_find(event->connect.conn_handle, &_desc);
struct ble_store_key_sec _key = { .peer_addr = _desc.peer_id_addr };
struct ble_store_value_sec _val;
if (ble_store_read_peer_sec(&_key, &_val) != 0) {
    ble_gap_security_initiate(event->connect.conn_handle);
}
```
If `ble_gap_conn_find` fails (non-zero return), `_desc` is uninitialized stack memory; `.peer_addr` reads garbage. Fix: check the return, log + fall through to "initiate pairing" (safe default — same path as truly unbonded).

- [ ] **Step 1: Request approval**

Print this diff and wait for "yes approve":

```diff
                 struct ble_gap_conn_desc _desc;
-                ble_gap_conn_find(event->connect.conn_handle, &_desc);
-                struct ble_store_key_sec _key = { .peer_addr = _desc.peer_id_addr };
-                struct ble_store_value_sec _val;
-                if (ble_store_read_peer_sec(&_key, &_val) != 0) {
+                int _rc = ble_gap_conn_find(event->connect.conn_handle, &_desc);
+                bool _is_bonded = false;
+                if (_rc == 0) {
+                    struct ble_store_key_sec _key = { .peer_addr = _desc.peer_id_addr };
+                    struct ble_store_value_sec _val;
+                    _is_bonded = (ble_store_read_peer_sec(&_key, &_val) == 0);
+                } else {
+                    ESP_LOGW(TAG, "ble_gap_conn_find rc=%d on fresh CONNECT — assuming unbonded peer", _rc);
+                }
+                if (!_is_bonded) {
                     ble_gap_security_initiate(event->connect.conn_handle);
                 }
```

- [ ] **Step 2: Apply the edit**

Use `Edit` on `firmware/components/ble_env/ble_env_service.c`. Match the existing 5-line block exactly and replace with the new 10-line block. Indentation matches the surrounding `case` arm (match what the file already uses — do not invent).

- [ ] **Step 3: Verify build**

```bash
source ~/esp/esp-idf/export.sh
cd firmware && idf.py build
( cd test_app && idf.py build )
```
Expected: both green; firmware size may grow ≤ 64 bytes (one log string + branch).

- [ ] **Step 4: Update REVIEW_FINDINGS.md**

1. Add to "Closed this session" table: `| A4 | <commit-sha> | ble_gap_conn_find rc checked; on failure log + fall through to "assume unbonded → initiate pairing". |`
2. Delete the `**A4 [Low] Unchecked return.**` bullet from section A.

- [ ] **Step 5: Commit + push**

```bash
git add firmware/components/ble_env/ble_env_service.c docs/REVIEW_FINDINGS.md
git commit -m "fix(ble_env): check ble_gap_conn_find return in gap_event_cb (A4)

Previously _desc was used uninitialized on the rc != 0 path, making the
bond-store lookup meaningless. On failure we now log and fall through
to the safe default (initiate pairing as if unbonded)."
git push
```

- [ ] **Step 6: Tick Status table**

Change T6 row to `- [x]`:
```bash
git add docs/superpowers/plans/2026-05-30-review-findings-cleanup.md
git commit -m "plan: tick T6 (A4 done) — <commit-sha>"
git push
```

---

### Task T7: A6 — delete dead BLE_ENV_CONN_* constants

**Model:** Sonnet
**Files:**
- Modify: `firmware/components/app_core/include/app_config.h` (Connection Intervals subgroup, lines 59-69)
- Modify: `docs/design_decisions.md` (DD-015 paragraph that claims the firmware requests 500–1000 ms intervals)
- Modify: `docs/power_budget.md` (≈ line 116, same claim)

**🔒 SOURCE EDIT — needs approval (touches app_config.h).**

**Context:** `BLE_ENV_CONN_ITVL_MIN_UNITS`, `BLE_ENV_CONN_ITVL_MAX_UNITS`, `BLE_ENV_CONN_LATENCY`, `BLE_ENV_CONN_SUPERVISION_UNITS` are defined but unreferenced. Intended use was a `ble_gap_update_params()` call removed during Phase 8 pairing debug. Decision: delete the constants; update DD-015 and power_budget.md to reflect that we no longer negotiate intervals.

- [ ] **Step 1: Confirm no live references**

```bash
grep -rn "BLE_ENV_CONN_ITVL\|BLE_ENV_CONN_LATENCY\|BLE_ENV_CONN_SUPERVISION\|ble_gap_update_params" firmware/ android/
```
Expected: only the four `#define` lines in `app_config.h`. Anything else → stop and re-scope.

- [ ] **Step 2: Request approval**

Print this diff for `app_config.h` and wait for "yes approve":

```diff
-/** @defgroup adv_config Advertising / Connection Intervals (Phase 7)
- *  NimBLE uses 0.625 ms units for advertising and 1.25 ms for connection.
+/** @defgroup adv_config Advertising Interval (Phase 7)
+ *  NimBLE uses 0.625 ms units for advertising.
  *  @{
  */
 #define BLE_ENV_ADV_ITVL_MS          250
 #define BLE_ENV_ADV_ITVL_UNITS       ((BLE_ENV_ADV_ITVL_MS * 8) / 5)
-#define BLE_ENV_CONN_ITVL_MIN_UNITS  400
-#define BLE_ENV_CONN_ITVL_MAX_UNITS  800
-#define BLE_ENV_CONN_LATENCY         0
-#define BLE_ENV_CONN_SUPERVISION_UNITS 400
 /** @} */
```

- [ ] **Step 3: Apply the edit to app_config.h**

`Edit` the file: delete only the four `BLE_ENV_CONN_*` lines and rename the @defgroup heading from "Advertising / Connection Intervals" to "Advertising Interval" (keep `BLE_ENV_ADV_ITVL_*` intact).

- [ ] **Step 4: Update DD-015 in `docs/design_decisions.md`**

Locate DD-015 (power tuning). Find the paragraph claiming the firmware requests 500–1000 ms connection intervals. Replace it with a paragraph stating: the firmware does **not** call `ble_gap_update_params()`; the call was removed during Phase 8 pairing debug to avoid an ENC_CHANGE race, and the unreferenced `BLE_ENV_CONN_*` constants have been deleted. Power tuning is now reduced to peripheral-side advertising interval (250 ms) + the central's chosen connection interval. Re-introducing the negotiation would require ordering it after `BLE_GAP_EVENT_ENC_CHANGE`.

Keep the rest of DD-015 intact. Exact wording is the agent's call — one factual paragraph.

- [ ] **Step 5: Update `docs/power_budget.md` (≈ line 116)**

Find the sentence claiming a 500–1000 ms connection-interval request. Replace with one sentence stating the firmware does not currently negotiate connection intervals; the peer central picks them. Add a one-line forward-pointer to DD-015.

- [ ] **Step 6: Verify build + dead-ref grep**

```bash
source ~/esp/esp-idf/export.sh
cd firmware && idf.py build
grep -rn "BLE_ENV_CONN_ITVL\|BLE_ENV_CONN_LATENCY\|BLE_ENV_CONN_SUPERVISION" firmware/
```
Expected: build green; zero grep matches.

- [ ] **Step 7: Update REVIEW_FINDINGS.md**

1. Add to "Closed this session" table: `| A6 | <commit-sha> | Deleted BLE_ENV_CONN_* dead constants from app_config.h; DD-015 + power_budget.md updated — we no longer call ble_gap_update_params. |`
2. Delete the `**A6 [Low] Documented BLE conn-param tuning not in code.**` bullet from section A.

- [ ] **Step 8: Commit + push**

```bash
git add firmware/components/app_core/include/app_config.h docs/design_decisions.md docs/power_budget.md docs/REVIEW_FINDINGS.md
git commit -m "refactor(app_core): drop dead BLE_ENV_CONN_* constants (A6)

ble_gap_update_params() was removed during Phase 8 pairing debug
(ENC_CHANGE race) and never re-added; the four BLE_ENV_CONN_* defines
had no users. DD-015 and power_budget.md updated."
git push
```

- [ ] **Step 9: Tick Status table**

```bash
git add docs/superpowers/plans/2026-05-30-review-findings-cleanup.md
git commit -m "plan: tick T7 (A6 done) — <commit-sha>"
git push
```

---

### Task T8: B5 — delete dead autoencoder arrays from ml_weights.h + extract_weights.py

**Model:** Sonnet
**Files:**
- Modify: `firmware/components/tinyml_inference/include/ml_weights.h` (delete the `/* --- Autoencoder weights (encoder then decoder) --- */` block + `ML_AE_HIDDEN_SIZE` + `ML_ANOMALY_THRESHOLD` + the `Autoencoder: 3-8-3, …` header comment)
- Modify: `ml/extract_weights.py` (drop the AE-preservation logic)

**🔒 SOURCE EDIT — needs approval (touches ml_weights.h + the Python regenerator).**

**Context:** DD-019 replaced AE-based anomaly with `max(softmax) < 0.5 → ANOMALY`. The AE arrays (~59 floats) and the two `#define`s are unread by `tinyml_inference.c`. `extract_weights.py` preserves them across retrainings; once deleted, the script can drop that regex too.

- [ ] **Step 1: Verify symbols are truly dead**

```bash
grep -rn "ML_AE_\|ML_ANOMALY_THRESHOLD\|ML_AE_HIDDEN_SIZE" firmware/ ml/
```
Expected: matches only in `ml_weights.h` (definitions), `ml/extract_weights.py` (the regex preservation logic), possibly `ml/train_classifier.py` (docstring), and `docs/`. If `tinyml_inference.c` or anything else in `firmware/` actively reads them → stop, re-scope.

- [ ] **Step 2: Request approval**

Print one-line summary + the headers being removed; wait for "yes approve":

```diff
-/* Classifier: 3-16-8-5 MLP, accuracy 0.9883 */
-/* Autoencoder: 3-8-3, comfortable-only, p95 threshold 0.00474350 */
+/* Classifier: 3-16-8-5 MLP, accuracy 0.9883 */
 /* Normalization: temp (-10,60), hum (0,100), press (900,1100) */

 #define ML_INPUT_SIZE        3
 #define ML_LAYER1_SIZE       16
 #define ML_LAYER2_SIZE       8
 #define ML_OUTPUT_SIZE       5
-#define ML_AE_HIDDEN_SIZE    8
-#define ML_ANOMALY_THRESHOLD 0.00474350f

 …classifier W1/b1/W2/b2/W3/b3 unchanged…

-/* --- Autoencoder weights (encoder then decoder) --- */
-static const float ML_AE_We[24] = { … };
-static const float ML_AE_be[8]  = { … };
-static const float ML_AE_Wd[24] = { … };
-static const float ML_AE_bd[3]  = { … };
```

- [ ] **Step 3: Apply the ml_weights.h deletion**

Edit `firmware/components/tinyml_inference/include/ml_weights.h`:
1. Delete the `/* Autoencoder: 3-8-3, …` header comment line.
2. Delete the two `#define` lines `ML_AE_HIDDEN_SIZE` and `ML_ANOMALY_THRESHOLD`.
3. Delete the entire trailing AE block: the `/* --- Autoencoder weights (encoder then decoder) --- */` marker and the four `static const float ML_AE_*` arrays.

End the file after `ML_b3`'s closing `};`.

- [ ] **Step 4: Update extract_weights.py**

Edit `ml/extract_weights.py`:
1. Docstring lines ≈ 14-22 — replace the "PRESERVES verbatim … autoencoder block" bullet with one stating "The autoencoder block was removed in REVIEW_FINDINGS.md B5 (DD-019 replaced AE-based anomaly with confidence thresholding); the file now ends after ML_b3."
2. `CLASSIFIER_BLOCK_RE` (≈ line 66-69) — replace the lookahead-terminated regex with end-of-file:

   ```python
   CLASSIFIER_BLOCK_RE = re.compile(
       r"/\* --- Classifier weights --- \*/\n.*",
       re.DOTALL,
   )
   ```
3. Adjust `render_classifier_block()` to not append a trailing blank line that would create double-blanks at EOF.
4. Delete the trailing `print("  Autoencoder block preserved …")` line.

- [ ] **Step 5: Smoke-test extract_weights.py (TF-dependent — skippable)**

```bash
cd ml
source .venv/bin/activate 2>/dev/null && python3 extract_weights.py --saved-model models/saved_model
```
Expected (if TF installed): "Wrote .../ml_weights.h (245 classifier weights regenerated)". This is a smoke test only — the saved_model classifier numbers differ from the deployed ones (per B1). **Revert** the file change before committing:

```bash
cd /home/karan-gandhi/ble_skill_project_package_reviewed
git diff firmware/components/tinyml_inference/include/ml_weights.h | head -40
# If the classifier numbers changed: discard the regen and re-apply the AE-block deletion by hand:
git checkout firmware/components/tinyml_inference/include/ml_weights.h
# then use Edit to re-delete the AE block + ML_AE_HIDDEN_SIZE / ML_ANOMALY_THRESHOLD / accuracy header comment line.
# Goal: only the AE deletion is committed; deployed classifier weights are byte-identical.
```

If TF is not installed → skip this step. Step 6 build will catch any structural breakage.

- [ ] **Step 6: Verify firmware build + size shrink**

```bash
source ~/esp/esp-idf/export.sh
cd firmware && idf.py build
( cd test_app && idf.py build )
```
Expected: both green; firmware binary shrinks ~200–250 bytes (was `0x95cb0`).

- [ ] **Step 7: Update REVIEW_FINDINGS.md**

1. Add to "Closed this session" table: `| B5 | <commit-sha> | Deleted ML_AE_* arrays + ML_AE_HIDDEN_SIZE + ML_ANOMALY_THRESHOLD from ml_weights.h (DD-019 made them dead). extract_weights.py simplified. |`
2. Delete the `**B5 [Med] Dead autoencoder arrays in `ml_weights.h`.**` bullet from section B.

- [ ] **Step 8: Commit + push**

```bash
git add firmware/components/tinyml_inference/include/ml_weights.h ml/extract_weights.py docs/REVIEW_FINDINGS.md
git commit -m "refactor(tinyml): delete dead autoencoder arrays (B5)

DD-019 replaced AE-based anomaly with max(softmax) < 0.5 -> ANOMALY.
ML_AE_We/be/Wd/bd, ML_AE_HIDDEN_SIZE, ML_ANOMALY_THRESHOLD were unread.
Removed; extract_weights.py simplified to no longer preserve them."
git push
```

- [ ] **Step 9: Tick Status table**

```bash
git add docs/superpowers/plans/2026-05-30-review-findings-cleanup.md
git commit -m "plan: tick T8 (B5 done) — <commit-sha>"
git push
```

---

### Task T9: C7 (full) — retire docs/principal_review_report.md to a pointer stub

**Model:** Sonnet
**Files:**
- Modify (rewrite): `docs/principal_review_report.md`

**Doc-only — no approval needed.**

**Context:** The file currently has a SUPERSEDED banner but the body still narrates stale facts (Just Works, 0x94f00, 19 TC rows). Banner alone isn't enough — anyone reading past it gets misled. Decision: replace the body with a one-paragraph pointer stub. Preserves the file path (no 404 from external bookmarks).

- [ ] **Step 1: Replace the file contents**

Overwrite `docs/principal_review_report.md` with:

```markdown
# Principal Manager Review Report — Retired

This file was the initial "review of the review package" produced during the planning phase. It is retired as of 2026-05-30.

The current independent review and remaining punch-list live in [`docs/REVIEW_FINDINGS.md`](REVIEW_FINDINGS.md). That file supersedes everything formerly in this report; nothing here is load-bearing for understanding the project's current state.

If you arrived from a stale link or bookmark, follow the pointer above.
```

- [ ] **Step 2: Verify no other doc still treats the body as authoritative**

```bash
grep -rn "principal_review_report" --include="*.md"
```
Expected: matches are the retired file itself + the existing C7 reference in `REVIEW_FINDINGS.md` + the top-line "independent of …" reference in REVIEW_FINDINGS:5. None should imply the body is current. If any other doc implies authority → fix that doc inline.

- [ ] **Step 3: Update REVIEW_FINDINGS.md**

1. Add to "Closed this session" table: `| C7 (full) | <commit-sha> | docs/principal_review_report.md body replaced with a 4-line pointer stub. SUPERSEDED banner no longer needed — the body is gone. |`
2. Delete the `**C7 [Low] `principal_review_report.md` is itself stale**` bullet from section C.
3. (Optional polish) — the top-line self-reference in REVIEW_FINDINGS:5 can stay as-is or be trimmed to "(now retired to a pointer stub)". Agent's call.

- [ ] **Step 4: Commit + push**

```bash
git add docs/principal_review_report.md docs/REVIEW_FINDINGS.md
git commit -m "docs: retire principal_review_report.md to a pointer stub (C7)

SUPERSEDED banner added in 08d6420 was not enough — the body still
narrated stale facts. Replaced with a 4-line pointer at REVIEW_FINDINGS
so external bookmarks still resolve."
git push
```

- [ ] **Step 5: Tick Status table**

```bash
git add docs/superpowers/plans/2026-05-30-review-findings-cleanup.md
git commit -m "plan: tick T9 (C7 done) — <commit-sha>"
git push
```

---

## Phase C — Opus tasks

Reasoning across multiple files + on-target re-verify. Reserved for Opus.

### Task T10: A5 — resolve inconsistent locking on s_conn_handle / s_ml_alert_subscribed / s_last_page

**Model:** Opus
**Files (read first; edit only if Step 2 says to):**
- `firmware/components/ble_env/ble_env_service.c` — `s_conn_handle`, `s_ml_alert_subscribed`
- `firmware/components/display/display.c` (line 129) — `s_last_page`
- `firmware/components/app_core/app_state.c` — existing `portMUX_TYPE` pattern for reference

**🔒 SOURCE EDIT — needs approval (if Step 2 concludes a code change is needed).**

**Context:** Single-core ESP32-C3 (RV32IMC) with FreeRTOS preemption. Single-word loads/stores are atomic on RV32; no torn-read risk. The question is memory ordering / compiler visibility across preemption. Three positions:

1. **Document why it's safe** — one-line comment per shared static, no code change. Lowest blast radius.
2. **Add `volatile`** — stops compiler from hoisting reads out of loops. Cheap; consistent with embedded idiom.
3. **Add `portMUX` everywhere** — most defensive; matches `app_state.c`. Adds tens of cycles per access and serializes interrupts.

- [ ] **Step 1: Read the call sites**

```bash
grep -n "s_conn_handle\|s_ml_alert_subscribed" firmware/components/ble_env/ble_env_service.c
grep -n "s_last_page\|s_mux" firmware/components/display/display.c
grep -n "portMUX\|portENTER\|portEXIT" firmware/components/app_core/app_state.c
```

For each access site, note which task touches it and whether the read is followed by a check that the value would change.

- [ ] **Step 2: Decide on a position + write DD-021**

Pick option 1, 2, or 3 above. Confirm the next free DD number by reading `docs/design_decisions.md` (likely DD-021 — verify). Add DD-021 with: context, decision, alternatives considered, consequences. 6-10 lines.

- [ ] **Step 3: Request approval for any source change**

If Step 2 chose option 2 (`volatile`) or option 3 (`portMUX`): print the per-site diff and wait for "yes approve". If option 1: skip to Step 5.

- [ ] **Step 4: Apply the source edit (if applicable)**

Apply at every access site, not just declarations. For option 3 use the same `portMUX_TYPE` pattern as `app_state.c`; the mux is a file-static in the relevant `.c`, not a header export.

- [ ] **Step 5: Verify build**

```bash
source ~/esp/esp-idf/export.sh
cd firmware && idf.py build
( cd test_app && idf.py build )
```
Expected: both green; binary may grow 0–200 bytes depending on the option chosen.

- [ ] **Step 6: On-target re-verify**

Reflash and run manual TC-009 (notify subscribe/unsubscribe across reconnects) + TC-006 (config write) from `tests/manual_test_matrix.md`. Confirm no regressions vs. the pre-edit run.

- [ ] **Step 7: Update REVIEW_FINDINGS.md + cross-refs**

1. Add to "Closed this session" table: `| A5 | <commit-sha> | Inconsistent locking resolved via DD-021 (option N). [If source edit: s_conn_handle / s_ml_alert_subscribed / s_last_page now <volatile|portMUX-guarded>]. |`
2. Delete the `**A5 [Low] Inconsistent locking.**` bullet from section A.
3. If README cites a DD upper bound (e.g. "DD-001 to DD-020"), bump it to DD-021.

- [ ] **Step 8: Commit + push**

```bash
git add firmware/components/ble_env/ble_env_service.c firmware/components/display/display.c docs/design_decisions.md docs/REVIEW_FINDINGS.md README.md
git commit -m "fix(concurrency): resolve shared-static locking (A5)

<2-3 sentences describing the chosen option. e.g.: 'Single-core
ESP32-C3 with FreeRTOS preemption — single-word access is atomic on
RV32, but volatile is added to prevent compiler hoisting. portMUX
would over-serialize for no benefit on a uniprocessor. See DD-021.'>"
git push
```

- [ ] **Step 9: Tick Status table**

```bash
git add docs/superpowers/plans/2026-05-30-review-findings-cleanup.md
git commit -m "plan: tick T10 (A5 done) — <commit-sha>"
git push
```

---

### Task T11 (OPTIONAL): B2 path-a — retrain classifier and redeploy weights

**Model:** Opus
**Files:**
- Run: `ml/train_classifier.py`
- Run: `ml/extract_weights.py --accuracy <new>`
- Modify: `firmware/components/tinyml_inference/include/tinyml_inference.h` (@par Training comment if accuracy changes)
- Modify: `README.md` status row + `docs/RELEASE_NOTES_v1_0_0.md` if they cite the deployed accuracy
- On-target: reflash + TC-ML-* from `tests/manual_test_matrix.md`

**🔒 SOURCE EDIT — needs approval (`ml_weights.h` is generated source; deployed model behaviour changes).**

**Status:** OPTIONAL. Skip unless the saved_model-vs-deployed mismatch surfaced in B1 needs closing. If skipped, the existing 98.83% deployed accuracy stays; B2/B3 already captioned it honestly.

**Context:** `ml/train_classifier.py` is reproducible (seeded RNGs per its docstring). One pipeline run (~30 s on CPU) → fresh SavedModel; `extract_weights.py` transposes kernels to row-major and rewrites the 245 floats. ML behaviour shifts → every ML class-boundary TC must be re-verified.

- [ ] **Step 1: Confirm TF is available**

```bash
cd ml
source .venv/bin/activate 2>/dev/null || python3 -m venv .venv && source .venv/bin/activate && pip install -r requirements.txt
python3 -c "import tensorflow as tf; print(tf.__version__)"
```
Expected: TF version printed (any 2.13+).

- [ ] **Step 2: Retrain**

```bash
cd ml
python3 train_classifier.py 2>&1 | tee /tmp/train.log
```
Capture final accuracy as `<new-accuracy>`.

- [ ] **Step 3: Extract weights**

```bash
cd ml
python3 extract_weights.py --accuracy <new-accuracy>
```
Expected: "Wrote .../ml_weights.h (245 classifier weights regenerated)".

- [ ] **Step 4: Diff + request approval**

```bash
cd /home/karan-gandhi/ble_skill_project_package_reviewed
git diff --stat firmware/components/tinyml_inference/include/ml_weights.h
git diff firmware/components/tinyml_inference/include/ml_weights.h | head -50
```
Print: 245 numeric changes + the accuracy header. One-line summary: "retrain shifted deployed accuracy from 0.9883 to <new>". Wait for "yes approve".

- [ ] **Step 5: Build + sanity check**

```bash
source ~/esp/esp-idf/export.sh
cd firmware && idf.py build
( cd test_app && idf.py build )
```
Both green; binary size unchanged (still 245 floats).

- [ ] **Step 6: On-target re-verify**

Reflash. Run every TC-ML-* from `tests/manual_test_matrix.md`. If a class boundary shifted (plausible after retraining on a fresh split), update the TC's expected class + add an explanatory note. If a TC genuinely regresses, **stop**, revert (`git checkout ml/models/saved_model firmware/components/tinyml_inference/include/ml_weights.h`), and add a B2 note in REVIEW_FINDINGS.md.

- [ ] **Step 7: Propagate the new number**

If the new accuracy differs from 0.9883:

```bash
grep -rn "98\.83\|0\.9883" --include="*.md" --include="*.h" --include="*.c"
```
Hand-edit each match to the new value. Keep the box-separability caveat from B3 verbatim.

- [ ] **Step 8: Update REVIEW_FINDINGS.md**

1. Add to "Closed this session" table: `| B2 path-a | <commit-sha> | Retrained classifier; deployed accuracy <old>→<new>. saved_model-vs-deployed mismatch (B1) resolved. On-target TCs re-verified. |`
2. Delete the "B2 path-a follow-up" bullet from "Remaining punch-list".

- [ ] **Step 9: Commit + push**

```bash
git add ml/models/saved_model firmware/components/tinyml_inference/include/ml_weights.h firmware/components/tinyml_inference/include/tinyml_inference.h README.md docs/RELEASE_NOTES_v1_0_0.md docs/REVIEW_FINDINGS.md tests/manual_test_matrix.md
git commit -m "ml: retrain classifier + redeploy weights (B2 path-a)

Closes the saved_model-vs-deployed mismatch surfaced by B1. New
deployed accuracy: <new>. All previously-cited 0.9883 figures updated.
Box-separability caveat (B3) unchanged: this still measures the
synthetic-box split, not real-sensor skill."
git push
```

- [ ] **Step 10: Tick Status table**

```bash
git add docs/superpowers/plans/2026-05-30-review-findings-cleanup.md
git commit -m "plan: tick T11 (B2 path-a done) — <commit-sha>"
git push
```

---

## Self-Review

**Spec coverage** (vs. `docs/REVIEW_FINDINGS.md` "Remaining punch-list"):

| Punch-list item | Task | Model  |
|-----------------|------|--------|
| A2              | T3   | Sonnet |
| A3              | T4   | Sonnet |
| A4              | T6   | Sonnet |
| A5              | T10  | Opus   |
| A6              | T7   | Sonnet |
| A7              | T1   | Haiku  |
| B2 path-a       | T11  | Opus (OPTIONAL) |
| B5              | T8   | Sonnet |
| C5              | T2   | Haiku  |
| C6              | T5   | Sonnet |
| C7 (full)       | T9   | Sonnet |

**Items NOT covered (deferred to a follow-up plan):**
- **B4** (med) — wire up or delete `model_data.cc` / `model.tflite` / `model_quantized.tflite` / `saved_model/`. Overlaps with T11; better as its own pass after.
- **D1** (low) — gitignore / regenerate note for dead `ml/models/*`. Coupled with B4.
- **D2** (low) — hardcoded BLE static address; SECURITY.md already flags it.
- **E1–E5** (low-med) — CI, host-runnable tests, real-sensor validation, Android robustness, extending `issues_encountered.md`. Each is its own plan.

**Placeholder scan:** searched for "TBD", "TODO", "implement later", "fill in details", "similar to Task N", "add appropriate" — none in task bodies. The two spots that say "agent's call" (T7 Step 4 paragraph wording, T9 Step 3 optional polish) are intentional small-judgement calls inside Sonnet tasks.

**Type/symbol consistency:** task references match the codebase as of HEAD `aa79796` (`s_conn_handle`, `s_ml_alert_subscribed`, `s_last_page`, `ble_gap_conn_find`, `BLE_ENV_CONN_*`, `ML_AE_*`, `ML_ANOMALY_THRESHOLD`, `ML_AE_HIDDEN_SIZE`, `app_state_request_config_save`). Line numbers are anchors only; subagents should grep-confirm before editing in case of drift.

---

## Execution Handoff

Plan saved to `docs/superpowers/plans/2026-05-30-review-findings-cleanup.md`. Three execution modes:

1. **Per-task model dispatch with fresh subagent context (recommended)** — orchestrator dispatches each task as a fresh `Agent` call with the task ID + plan path. Subagent reads the plan, executes only its task (including commit + push + Status tick), and yields. Cheapest path: Haiku tasks T1/T2 can run in parallel; Sonnet tasks roughly serial; Opus runs last. Approval prompts surface back to the user. **Best for cost + session-safety: each subagent's context is automatically clean.**

2. **Inline execution with `/clear` between tasks** — user runs each task inline in this session, then `/clear`s, then re-opens with "resume the plan at `docs/superpowers/plans/2026-05-30-review-findings-cleanup.md`". The Status table is the resume marker. Same per-task atomicity, but the model is whatever the user has loaded — no per-task model tiering.

3. **Single-session sequential** — work through every task in this session via `superpowers:executing-plans`. Simplest; no per-task context reset; risks orchestrator context overflow on a long plan.

**Which mode?**
