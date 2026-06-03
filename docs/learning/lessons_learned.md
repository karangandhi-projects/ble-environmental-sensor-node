# Lessons Learned — BLE Environmental Sensor Node

**Project:** BLE Environmental Sensor Node — ESP32-C3 / ESP-IDF v5.2.3 / NimBLE
**Audience:** Any embedded/firmware developer. No prior BLE knowledge required.

Most of the *code* in this project is unremarkable — a GATT service, an OLED driver, a tiny MLP. The genuinely transferable skill is everything *around* the code: how the hard bugs were actually solved, and what an independent review found once the thing "worked."

There are two families of lessons here, and they are different in kind:

- **Part I — Systematic Debugging.** How the runtime bugs in `../issues_encountered.md` were solved. Almost none yielded to staring at code; they yielded to a repeatable loop.
- **Part II — Integrity Lessons from the Independent Review.** What `../REVIEW_FINDINGS.md` exposed *after* the firmware ran: a repo that quietly disagreed with itself — stale docs, dead code posing as the pipeline, numbers that were never true, a metric that didn't measure what it claimed.

Part I is about making broken things work. Part II is about not fooling yourself once they do. The second is the subtler and, arguably, the more valuable skill.

Where `../issues_encountered.md` is a *chronological log* and `../debug_guide.md` is a *symptom lookup*, this file is the *methodology and the meta-lessons* — what to carry to the next project.

---

# Part I — Systematic Debugging

## 1. The Core Loop

When something doesn't work and the cause isn't obvious, do **not** start editing code and re-flashing. Run this loop instead:

```
        ┌─────────────────────────────────────────────────┐
        │ 1. REPRODUCE      Make the failure happen on     │
        │                   demand. Deterministically.     │
        └───────────────────────┬─────────────────────────┘
                                ▼
        ┌─────────────────────────────────────────────────┐
        │ 2. ISOLATE        Which layer? Narrow it to one  │
        │                   suspect. Change nothing else.  │
        └───────────────────────┬─────────────────────────┘
                                ▼
        ┌─────────────────────────────────────────────────┐
        │ 3. TEST THE       Does the vendor / reference    │
        │    REFERENCE      example work on THIS hardware?  │
        │                   (bleprph, blehr, nvs_rw…)       │
        └───────────────────────┬─────────────────────────┘
                                ▼
        ┌─────────────────────────────────────────────────┐
        │ 4. DIFF           Compare the working example to │
        │                   our code, line by line. The    │
        │                   bug lives in the difference.    │
        └───────────────────────┬─────────────────────────┘
                                ▼
        ┌─────────────────────────────────────────────────┐
        │ 5. CHANGE ONE     Apply the smallest single fix. │
        │    THING          Re-test. Confirm with evidence.│
        └───────────────────────┬─────────────────────────┘
                                ▼
                  fixed? ──no──► back to step 2
                     │ yes
                     ▼
        ┌─────────────────────────────────────────────────┐
        │ 6. WRITE IT DOWN  Root cause + fix + lesson into │
        │                   issues_encountered.md.         │
        └─────────────────────────────────────────────────┘
```

The order matters. Each step exists because skipping it is exactly how this project wasted time — and below, each is justified by a real failure that proves the point.

## 2. Step 1 — Reproduce Deterministically (you can't fix what you can't trigger)

A bug you can only sometimes reproduce is a bug you can't confirm you've fixed. The first job is to make the failure **reliable**.

**Evidence — Issue 10 (pairing):** pairing failed across *18 consecutive attempts*, but the logs differed between runs — "zero SM events on most attempts." The intermittency was the trap: it kept attention on whichever variable changed last instead of the constant underlying cause. Progress came only once the failure was pinned to a deterministic sequence (connect → first encrypted write → silent abort) that happened *every* time.

**Evidence — Issue 8 (no serial output):** the deeper version — you couldn't even *observe* the system. The Bash tool backgrounded any command over ~20–25 s; our serial scripts ran ~25–35 s, so they were killed mid-capture and held the port hostage in a loop. Until that was fixed (`run_tests.py`, ≤15 s, inline), no downstream debugging was trustworthy.

> **Principle:** Before theorising about cause, make the failure happen the same way every time, and make sure you can actually *see* the output. A flaky repro or a broken observation harness corrupts every conclusion that follows.

## 3. Step 2 — Isolate One Layer (change one thing, test, log, proceed)

BLE failures love to masquerade. A bug in advertising data shows up as "the app won't connect." A missing store callback shows up as "wrong PIN." `../debug_guide.md` says it directly: *"Debug one layer at a time"* and *"Never change five things at once."*

**Evidence — Issue 10 again:** the single biggest time sink in the project happened *because* this rule was broken. The symptom ("Incorrect PIN") pointed at the SM configuration, so attention churned across IO-capability, the MITM flag, the SC flag, and Security-Request timing — four variables at once. The real fault was in a *different layer entirely* (the bond store), invisible while everything was being changed. The fix landed only after the search collapsed to one layer at a time.

**The corollary — bisect to where the failure *actually* is, not where the symptom appears.** In Issue 10 the abort happened *after* SC key exchange, at the LTK-save step, with nothing at INFO log level to show it. Raising log verbosity and walking the SMP exchange PDU-by-PDU is what finally located the true failure point.

> **Principle:** One variable per test. When a symptom in layer A is really caused by layer B, the only way to find B is to hold everything else still and move the boundary one step at a time.

## 4. Step 3 — Test the Reference Example (is it us, or the platform?)

This is the step the project leaned on hardest, and the one most developers skip. Before assuming your code is wrong, **prove the vendor's known-good example works on your exact hardware and toolchain.** ESP-IDF ships them for precisely this: `bluetooth/nimble/bleprph` (peripheral + security), `bluetooth/nimble/blehr` (notifications), `storage/nvs_rw_value`, `system/unit_test` (see `resources.md`).

Two outcomes, both valuable:

- **The example works** → the platform and hardware are fine. The bug is in *your* difference from the example. Proceed to Step 4.
- **The example also fails** → it's your environment (toolchain, target, wiring, sdkconfig), not your application logic. Stop reading your own code; fix the environment.

**Evidence — Issue 10:** the resolution came *directly* from this step. `bleprph` pairs and bonds successfully; our code did not. That single fact reframed the whole investigation from "what's wrong with my SM config" to "what does bleprph do that I don't" — which is Step 4.

> **Principle:** A working reference example is a *control* in the scientific sense. It tells you which half of the universe — platform or your code — the bug lives in, before you've read a single line.

## 5. Step 4 — Diff Against the Working Example (the bug lives in the difference)

Once a reference example works and yours doesn't, the bug is, by definition, somewhere in what's *different*. Compare deliberately: init order, the exact set of init calls, `CMakeLists.txt` `REQUIRES`, `sdkconfig` options, callback registration. Don't skim for what looks important — the bug is usually the boring call you didn't know mattered.

**Evidence — Issue 10 (the headline catch):** the diff between `bleprph` and our firmware revealed a single missing line — `ble_store_config_init()`. The example calls it right after `nimble_port_init()`; our code never did. Without it, `store_write_cb` is `NULL` and the Security Manager **silently aborts** at the LTK-save step. `CONFIG_BT_NIMBLE_NVS_PERSIST=y` was set, which *looks* sufficient, but the NVS callbacks still have to be registered explicitly. No amount of staring at our SM config would have found this — only the diff against a working example did.

**Evidence — Issues 2 & 6 (`host/ble_hs.h` not found):** the working examples all declare `REQUIRES bt` in `CMakeLists.txt`; ours didn't (Issue 2). Then in `test_app`, the examples carry an `sdkconfig.defaults` with `CONFIG_BT_ENABLED=y`; ours was missing, so the `bt` component was excluded entirely and the include path vanished (Issue 6). Both were *config diffs* from the reference, not code bugs.

**Evidence — Issue 5 (`idf_performance.h`):** the inverse — we had pulled in a component (`test_utils`) the standard examples *don't* use. The diff said "remove what the example doesn't have," and the build went green.

> **Principle:** "What does the working example do that mine doesn't?" is the single highest-yield question in this entire project. The fix to the hardest bug was one line a diff surfaced in minutes and direct reasoning missed for eighteen attempts.

## 6. Step 5 — Change One Thing, Then Verify With Evidence

Apply the smallest possible fix, re-run the deterministic repro from Step 1, and **confirm with concrete evidence** — a log line, a count, a packet — not a vibe.

**Evidence — Issue 3 (the silent test dedupe):** the cautionary tale for "verify with evidence." The build was green and the runner printed `37 Tests 0 Failures 1 Ignored / OK`. It *looked* perfect. But 25 of 62 written tests were silently dropped because four `test/` directories shared a basename and ESP-IDF deduped them, keeping only the last. The "OK" was a lie; the **number** was the truth. Renaming the dirs to unique basenames jumped the count to `62 Tests / OK`. Verify against an *expected quantity*, because "no error" and "passed" are not the same as "did what I intended."

> **Principle:** A fix isn't done when the error disappears — it's done when positive evidence shows the intended behaviour. Prefer a number you can predict (test count, byte count, sequence number) over a green checkmark you can't.

## 7. The Cross-Cutting Debugging Meta-Lessons

The loop is the process. These are the *patterns* that recurred across the 11 issues — what to be suspicious of by default.

### Lesson A — Assume failures are silent until proven loud

The most expensive bugs produced **no error output**:
- Issue 9: `ble_gap_adv_set_fields()` returned an error for an over-31-byte payload, but the return value was unchecked, so advertising "started" with an empty payload. *Fix: check every return code; on error, log and abort.*
- Issue 10: a `NULL` store callback aborted bonding with nothing at INFO level.
- Issue 3: a component-name collision dropped 25 tests with a green build.

> Treat "it ran without complaint" as **unverified**, not "correct." Check return codes; raise log levels; count what you expect.

### Lesson B — The reference example is ground truth

Issues 2, 5, 6, and 10 were all resolved by comparing to ESP-IDF's NimBLE/NVS/unit-test examples. When the platform ships a working sample of the thing you're attempting, it is the cheapest oracle you have. Keep `resources.md`'s example list open while debugging.

### Lesson C — Suspect the environment, not just the code

Several "bugs" were never in the application at all:
- Issue 4: a stale `build/` dir cached the wrong (xtensa) toolchain after a target switch. *`set-target` doesn't invalidate `build/` — delete it.*
- Issue 7: the Task Watchdog reset-looped because `unity_run_menu()` busy-waits on UART and starves IDLE — a *harness* behaviour, not our logic. *Fix: `CONFIG_ESP_TASK_WDT_EN=n` for the test app only.*
- Issue 8: the Bash tool's auto-backgrounding broke serial capture.

> Know your toolchain, build cache, watchdog, and tooling as well as your code. A surprising fraction of "code bugs" are environment bugs in costume.

### Lesson D — Read the logs for what's *absent*

Issue 9's tell was that *no advertising-start log appeared*. Issue 10's was *zero SM events*. The missing line is often a louder signal than any present one — it says the code died *before* reaching the point you expected.

### Lesson E — Distinguish a wrong *implementation* from a wrong *approach*

Issue 11 (the autoencoder anomaly detector) was not a coding bug — the code worked exactly as written. The *approach* was a category error: an autoencoder trained on one class is a "novelty detector for that class," so it flagged the legitimate `danger` reading as an anomaly. No amount of debugging the implementation would fix it; the design had to change (→ classifier-confidence thresholding, DD-019).

> When fix after fix fails to converge, stop debugging the code and question whether you're solving the right problem the right way. Step up an altitude.

---

# Part II — Integrity Lessons from the Independent Review

Part I was about a thing that *crashed*. This part is about a thing that *ran fine* — and was reviewed anyway. The independent pass in `../REVIEW_FINDINGS.md` found almost no crashes. It found something more insidious: **a repository that had quietly drifted out of agreement with itself.** Docs claimed things the code didn't do. Numbers cited everywhere were never true. Dead code was presented as the live pipeline. The headline accuracy measured something other than what everyone assumed.

These are the bugs that ship, because nothing turns red. They are caught only by deliberately checking claims against reality — which is the whole skill of Part II.

## 8. Lesson G — Documentation rots, and it rots *silently*

The single dominant category in the review (the entire "C" series) was documentation that no longer matched the code.

- **One story, told many ways (C1/C2).** The security model had moved from "Just Works" to "MITM Passkey Display," but the old phrase survived in ~10 docs *and* in source comments — and `phase8_pairing_debug.md` even contradicted itself. The fix wasn't just find-and-replace; it was nominating **one source of truth** (`../security_model.md`) and making every other mention *defer* to it instead of restating it. Restated facts drift independently; pointers don't.
- **Stale specs (C5/C6).** `requirements.md` still described an OLED page layout the firmware had long since changed; `architecture.md` still said "display TBD — Phase 1.5" for a module that had shipped. Specs written once and never revisited become fiction.

> **Principle:** Every fact should live in exactly one place; everywhere else *links* to it. Duplicated prose is duplicated maintenance, and the copies silently diverge. When you change behaviour, grep the whole repo — docs *and* comments — for the old story.

## 9. Lesson H — Reconcile every number to verified reality

The review found that **every headline count in the repo disagreed with the hardware** (C3): the cited binary size, the Unity test count, and the manual-test-case count were all wrong across README, RELEASE_NOTES, and several docs. They had been written once, then drifted as the code grew, and nobody re-measured.

The deeper trap (TEST-COLLISION): the claim "all 62 tests pass" had been *inferred from the CMake `SRCS` lists* — it looked authoritative but was never observed. Only running the suite **on the target** exposed that just 37 actually ran. `compiles ≠ runs`; a list in a build file is not evidence of execution.

> **Principle:** A number in your docs is a claim, and a claim needs evidence. Re-measure binary size, test counts, accuracy, and timings against the *actual artifact on the actual target* before publishing them — and re-measure when the code changes. Never transcribe a number you didn't observe.

## 10. Lesson I — Delete dead code; don't let it pose as the pipeline

Two findings (B4, B5) were dead code masquerading as live:
- `model_data.cc`, `model.tflite`, `model_quantized.tflite`, and `quantize.py` sat in `ml/` looking like the deployment path — but the firmware never compiled any of them. They were deleted; the binary was **byte-identical** afterward, proving they were never part of the build.
- The autoencoder weight arrays (`ML_AE_*`) lingered in `ml_weights.h` after DD-019 retired the approach. The linker's `--gc-sections` had already stripped them, so they cost zero bytes — but they cost *comprehension*: a reader would reasonably assume they were used.

> **Principle:** Dead code has no runtime cost and a large cognitive cost — it lies about how the system works. If it isn't in the build, delete it. "It's harmless, the linker strips it" misses the point: the harm is to the next person trying to understand the pipeline.

## 11. Lesson J — Generated artifacts must be regenerable — and prove it by regenerating

The deployed model weights (`ml_weights.h`) had been committed with **no script that could reproduce them** from the trained model (B1). The fix added `extract_weights.py` — and the moment it ran, it surfaced a *real latent bug*: the regenerated weights **differed from the deployed ones** (`saved_model dense/kernel[0,0] = 1.4600533` vs `ML_W1[0] = 1.45603526`). The committed weights and the committed model had silently fallen out of sync; nobody could have known without a regeneration path.

> **Principle:** Anything generated (weights, code-gen, lockfiles, fixtures) must have a committed, runnable path from source to artifact — and you only know it works when you *run it and diff the output against what's checked in*. A reproducibility script you never execute is itself an unverified claim.

## 12. Lesson K — Know what your metric actually measures

The model's "accuracy" was cited as `99.7%` in some places and `98.83%` in others (B2). Reconciling the number was the easy half. The important half (B3): the 98.83% **measures box-separability of synthetic data, not real-sensor skill.** The train and test sets were both drawn from the same disjoint, axis-aligned class boxes in `collect_synthetic.py`; even the "real device" samples were human slider entries *inside those same boxes*. A high score on cleanly separated boxes says little about messy real sensors. The fix propagated an honest caveat to every site that cited the number.

> **Principle:** A metric is only as meaningful as the data it's computed on. Before you quote a number, ask what it would take for it to be *high but useless* — and state that caveat next to the number. Overclaiming a benchmark is a correctness bug in the documentation.

## 13. Lesson L — Review-and-fix is itself a disciplined loop

How the ~40 findings were actually cleared is a lesson in its own right (see the session wraps in `../REVIEW_FINDINGS.md`):
- **A durable findings doc** that any future session could resume from — the review's value survives the session that produced it.
- **Per-task atomicity:** each fix = one work commit + one plan-Status-tick commit, both pushed *before the next task started*, so a `/clear` or crash mid-pass resumes cleanly.
- **One source of truth for status,** with stale predecessors retired to pointer stubs (`principal_review_report.md` → 4-line pointer) rather than left to contradict the live doc.
- A real correctness fix hid in there too: **A1** — a blocking NVS flash write (~tens of ms) inside the BLE callback `gatt_access_cb`, violating "never block in a callback." Review catches the rule violations that work *anyway* until they don't.

> **Principle:** Treat review findings like code: track them in a resumable artifact, fix them atomically, and keep exactly one authoritative status. A review whose conclusions aren't written down to survive the session was half-wasted.

---

# Part III — Quick Reference

## The debugging loop, condensed

| Step | Question | Anti-pattern it prevents |
|---|---|---|
| 1. Reproduce | Can I trigger this on demand and *see* it? | Chasing intermittent ghosts (Issue 10, 8) |
| 2. Isolate | Which single layer? | Changing five things at once (Issue 10) |
| 3. Reference | Does the vendor example work here? | Assuming the bug is in your code (Issue 10, 6) |
| 4. Diff | What do I do that the example doesn't? | Missing the one boring init call (Issue 10, 2, 5) |
| 5. One fix + verify | Did a *number/log* confirm it? | Trusting a green "OK" (Issue 3) |
| 6. Record | What's the lesson + guard? | Re-hitting the same bug next session |

## Failure modes to suspect first

| If you see… | Suspect… | Seen in |
|---|---|---|
| Build green but behaviour wrong | Silent dedupe / unchecked return / NULL callback | Issues 3, 9, 10 |
| Header / symbol not found | Missing `REQUIRES` or `sdkconfig` option | Issues 2, 6 |
| Linker / toolchain errors after a target change | Stale `build/` cache | Issue 4 |
| Reset loop, watchdog timeout | Harness busy-loop starving IDLE | Issue 7 |
| No output at all | Observation harness (backgrounding, DTR reset) | Issue 8 |
| "Works in the example but not for me" | The diff between example and your code | Issues 2, 5, 6, 10 |
| Repeated fixes never converge | Wrong *approach*, not wrong code | Issue 11 |

## Pre-release integrity self-audit (the Part II checklist)

Run this before calling something "done" — it's the review, internalised:

- [ ] **One source of truth?** Does every doc/comment about a behaviour *defer* to one canonical place, or do copies restate (and risk contradicting) it? (G)
- [ ] **Numbers re-measured?** Are binary size, test counts, accuracy, timings observed on the real target *now*, not transcribed from memory? (H)
- [ ] **Compiles ≠ runs?** Is "it passes" backed by an on-target run and an *expected count*, not inferred from a build file? (H)
- [ ] **Dead code gone?** Does anything in the tree look like the live path but isn't built? Delete it and confirm the artifact is unchanged. (I)
- [ ] **Artifacts regenerable?** Is every generated file reproducible from committed source — and did you *run* the path and diff the result? (J)
- [ ] **Metric honest?** Do you know what your headline number actually measures, and is the caveat written next to it? (K)
- [ ] **Findings durable?** Are open issues in a resumable doc with one authoritative status? (L)

## Related docs

- `../issues_encountered.md` — the chronological evidence base Part I cites.
- `../REVIEW_FINDINGS.md` — the independent-review punch-list and fix log Part II distills.
- `../debug_guide.md` — symptom → diagnosis lookup for when you already know the symptom.
- `../phase8_pairing_debug.md` — the full forensic write-up of Issue 10 (the canonical case study for the whole method).
- `resources.md` — the ESP-IDF reference examples used as "ground truth" in Step 3.

> **The two sentences to remember.** When something is broken: don't out-think the bug — *reproduce it, get the reference example working, diff the two, and change one thing at a time.* When something works: don't trust it — *re-measure every number, delete every dead path, regenerate every artifact, and make every doc defer to one source of truth.* The hardest bug in this project was one missing line that the first habit found in minutes; the most pervasive problem was a repo that quietly disagreed with itself, which only the second habit catches.
