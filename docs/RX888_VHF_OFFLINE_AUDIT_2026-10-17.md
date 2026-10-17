# RX888 MkII VHF offline integration audit — 2026-10-17

Branch: `feature/rx888-vhf-agc-upstream-integration`.

## Baseline evidence

DEVPC001 clean integration compile after header correction: `make -C src -j4 radiod rx888.so NATIVE=0 ENABLE_RX888=1` with all other SDR frontends explicitly zero completed with exit status 0, no reported warnings. `radiod` was already up-to-date on the incremental rebuild, and `rx888.so` recompiled. Standalone C tests passed: 29 RF and 16 IF gain table values/limits, and 256 register input values for the AGC mode-bit helper. This does **not** verify I²C transport or radio receive behaviour.

## Observed findings and required offline tests

| Severity | Function and lines in audited rx888.c | Evidence and risk | Offline acceptance test |
| --- | --- | --- | --- |
| Critical | `r828_status` (182-189) and `r828_read` (191-199) | Assert checks are compiled out under `-DNDEBUG`; a negative read count or short read is not reported as fatal, and the current register status/shadow can be left partial or invalid | Stub read transport to return -1, 0, 1, 4, 5 and verify only exact-length read commits state; no uninitialised byte use |
| Critical | `r828_write_byte` (205-211), `r828_write_byte_mask` (213-219) | Shadow value changes **before** the I²C write succeeds; caller ignores errors | Inject failed writes and verify no shadow commit or continued configuration; verify masked unrelated bits preserved |
| Critical | `rx888_set_vhf_mode` (1447 onward) | Reference-clock result, tuner status and dozens of register-write results are ignored | Fault-inject clock failure and each tuner-init write; assert startup aborts, never publishes valid VHF state |
| High | `rx888_set_tuner_ref` (1346 onward) | Clock-lock timeout currently only logs warning; caller does not enforce lock | Fake clock readback and PLLB lock timeout; verify failed startup and state cleanup |
| High | `rx888_set_tuner_frequency` (1589-1697) | PLL VCO-lock failure only logs, then sets `frontend->frequency` to calculated nominal value | Force PLL-unlocked response; verify frequency remains uncommitted and error reaches setup |
| High | `rx888_set_hf_mode` (1428-1435) | Only switches antenna GPIO; R828D powerdown and Si5351 tuner reference shutdown are absent | Trace VHF→HF register/clock/GPIO writes, verify no spurious tuner power and restore HF IF/gain semantics |
| High | `rx888_setup` (487-520) | Explicit VHF setup commits lock and gain/status without checking I²C writes, and fallback only checks zero frequency | Simulate VHF init error, gain write error, invalid frequency, omitted frequency and undersample>1; confirm no false ready state |
| Medium | `rx888_set_att` and `rx888_set_gain` (1310-1344) | Status and calibration state updated even after failed R828D writes | Inject RF LNA, mixer, IF VGA errors and ensure no false success report |

## Proposed test harness

Before hardware testing, extract small transport-independent R828D register/control operations with a callback-driven fake I²C backend. Capture ordered register operations, inject a failure at each transaction position, and check committed state, rollback/failure propagation, and register masks. Add a separate tuner/clock startup state-machine test, followed by PLL calculation cases for representative marine frequencies and 20 MS/s IF bounds. Run both default and `-DNDEBUG=1` test builds so assertion-based assumptions cannot mask errors.

## Change constraints

Do not overwrite Phil's upstream receive/sample-processing, FM, filter, timing or command infrastructure. Keep hardware tuner-auto AGC disabled, manual VHF configuration explicit, and existing HF default behaviour unaffected. No local receiver start/install, firmware load, or production-devlap200 changes before scheduled maintenance.

## Status

Audit is based on current committed source and supplied DEVPC001 build/test output. No hardware emulation/injected fault test has been executed, and no runtime error-handling fix has yet been implemented.

## First fault-injection implementation

Added `src/r828d_io.h` with callback-driven read/write, successful-write-only register-shadow commits, strict short-read failure handling, bit-reversed R828D readback, and masked writes preserving unrelated bits. Added `tests/test_r828d_io.c`, including failing reads/writes and exhaustive 256x256 value/mask combinations, using `CHECK` rather than `assert` so test conditions remain active with `-DNDEBUG=1`. The RX888-only CI workflow now compiles and invokes that test.

**Integration boundary:** these new functions and fault-injection tests are **not yet called by `rx888.c`**. The existing production-path R828D I²C wrappers retain the issues listed above until they are deliberately adapted and failure-propagated through the VHF tuner setup. CI and DEVPC001 test success for the new harness remain unverified. No hardware/production deployment occurred.

## 2026-10-17 FX3 adapter integration

Committed the first adapter from actual `rx888.c` to `src/r828d_io.h`: register status reads, single-register reads, single-byte writes and masked writes now use callback-backed transactions with strict transfer-length checking and commit their existing R828D shadow *only after a successful operation*. The successful historical return conventions are maintained: status read 5, register read 1, writes 0. Errors are logged and returned negative. The FX3 read/write USB endpoints and the successful register values/order are unchanged.

**Not yet validated by an on-host rebuild.** Tuner callers still generally ignore the returned errors, so this is *not* end-to-end failure propagation or a safe-startup guarantee. The global shadow model is preserved for compatibility but needs scrutiny if more than one RX888 instance is supported. `r828_write` bulk writes continue to use their existing direct FX3 path. Next: rebuild and run existing tests on DEVPC001, then propagate error returns through reference-clock, tuner startup/PLL lock and manual RF/IF gain operations with failure-injection coverage. No production installation or receiver control.

## 2026-10-17 VHF initialisation fail-fast checkpoint

Changed the active `rx888_set_vhf_mode` to return `int`. It now aborts after a failed R828D status read and at the first failed register write within the 0x05..0x1f active initialisation table, leaving the successful register order and byte values unchanged. `rx888_set_tuner_frequency` checks the startup result before proceeding with RF tracking/PLL programming and returns zero on initialisation failure, so `rx888_setup` does not accept a calculated tuned frequency from this path.

**Not yet build/test verified.** This checkpoint is deliberately partial: Si5351 clock setup, GPIO/antenna switching, AD8340 writes, RF-tracking writes, PLL write/lock operations and manual RF/IF gain application still need end-to-end error propagation, failure cleanup and fault-injected integration tests. The inactive alternative `#else` init table retains unchecked operations. A partially performed startup may leave hardware side effects, and the setup fallback is not a transactional rollback. Treat hardware tests as blocked until these are addressed.

## 2026-10-17 clock and PLL failure propagation checkpoint

Si5351 CLK2/PLLB configuration in `rx888_set_tuner_ref` now treats negative/unsuccessful writes, short register reads and PLLB lock timeout as failure (return zero). The R828D VHF initialisation caller now aborts if the reference-clock setup fails. The active `rx888_set_tuner_frequency` PLL and tracking register writes now check error results and abandon frequency publication on failure. PLL status reads are checked, and both the initial and fallback VCO lock attempts must succeed before `frontend->frequency` is committed. PLL divider bounds are runtime checked rather than being solely an assertion.

**Not yet compiled or fault-injected as a whole.** This is a code-review checkpoint only. Remaining important risks: failed startup can leave CLK2, antenna selection and partial tuner state active; manual RF/IF gain failures remain unhandled by their callers; the success path has not been hardware-verified. No production devices were changed.

## 2026-10-17 fault-injected R828D initialization sequence

Factored the original 27-byte active R828D startup table (registers 0x05–0x1f) into `src/r828d_init.h`, accessed through the already tested `r828d_io` callback layer. `rx888_set_vhf_mode` now invokes this helper. `tests/test_r828d_init.c` checks the successful write order and abort behaviour for each of 27 individually injected write failures, under `-DNDEBUG=1`. The CI workflow includes this test.

**Validation is pending on DEVPC001.** This specifically tests the active register-init loop, not Si5351 clock handling, PLL/VCO locking, gain application, or entire receiver setup. In the real driver the global shadow is copied back only after all 27 writes succeed. On partial failure, actual hardware may have accepted earlier writes while the global shadow remains at the pre-init state. Safe cleanup/reset after failures is still required before hardware testing.

## 2026-10-17 manual gain / readiness gating

Added `vhf_ready` to the RX888 device state, initially false via calloc. A dedicated `rx888_apply_vhf_manual_gains` performs the five existing manual LNA/mixer/IF VGA masked writes, checks every return code, then publishes nominal RF attenuation/IF gain/scale only after *all five* succeed. Explicit `frequency` startup sets `vhf_ready` and `frontend->lock` true only when PLL tuning and gain application have succeeded; a gain failure clears the readiness flag, frequency and lock, and suppresses success logging. A tuner frequency failure clears readiness and lock. HF/default startup remains the default path. Hardware tuner AGC remains disabled.

**Outstanding and blocking hardware tests:** this is not a transactional hardware rollback; earlier writes can succeed before a later one fails, and CLK2/antenna/tuner state may remain active. The generic `frontend->lock` field's downstream meaning should be checked before any live receiver test. Other R828D gain call sites and dynamic VHF↔HF transitions still need audit. This change has not been compiled or fault-injection-tested on DEVPC001 yet.

## 2026-10-17 VHF fallback recovery (pending build verification)

Added callback-driven `rx888_vhf_recover` state helper and `tests/test_rx888_vhf_recovery.c` for four CLK2/antenna failure combinations. The `rx888.c` integration attempts to power down Si5351 CLK2 and switch the antenna GPIO back to HF whenever VHF setup ends with zero frequency. The routine always clears VHF readiness, frontend lock and frequency, reports incomplete cleanup, and attempts antenna routing even if clock shutdown fails. The CI offline test now includes this helper.

**Caveats:** This is best-effort cleanup, *not* confirmed full R828D tuner power-down, not restore of HF gain/IF state and not proof of physically safe state after I²C/USB failure. It also runs for the normal no-frequency HF setup, so HF regression needs explicit testing. The caller currently logs incomplete recovery rather than making the full device setup fail. The new code and tests are **not yet compiled or run on DEVPC001**. Do not deploy or start receivers; preserve production AIS.

## 2026-10-17 default HF startup regression guard

An explicit local `vhf_attempted` flag now distinguishes the normal HF startup path from a failed attempted VHF tune. `rx888_choose_startup_route` selects ordinary `rx888_set_hf_mode` for no VHF attempt, recovery for failed attempted VHF startup, and no cleanup for successful VHF startup. Standalone routing tests cover all input combinations and are included in CI. This avoids the previous unconditional Si5351 CLK2 shutdown on default HF initialisation. **Offline build/test still pending.** This routing test covers the policy helper, not actual FX3 calls or physical HF reception; the runtime transition and recovery-after-failure cases still require deeper test isolation.

## 2026-10-17 refuse setup after failed VHF hardware cleanup

The failed-VHF branch in `rx888_setup` now returns -1 if `rx888_recover_vhf` cannot complete both CLK2 shutdown and HF GPIO routing, rather than continuing with an uncertain hardware state. The startup routing test now verifies the shared recovery-result policy for normal HF, ready VHF and both successful/failed recovery results. These new changes are committed but **not yet build-tested**. Return -1 at this late stage does not itself guarantee all previously allocated resources are released; inspect setup teardown and failure handling prior to hardware tests. Successful best-effort recovery is still not a guarantee of complete R828D hardware reset or coherent HF gain state.

## 2026-10-17 teardown / failed-VHF fallback audit

Audit: rx888_setup allocates an RX888 context, publishes it via frontend->context, and initialises USB resources. Previously a late recovery error could return -1 without closing these resources. Successful hardware cleanup after VHF tuning failure could also silently continue as HF, with altered AGC/gain/IF state. Now every attempted but failed VHF startup attempts recovery, calls rx888_close(sdr), clears frontend->context, frees the context and returns -1, whether cleanup itself succeeds or fails. The routing unit test now rejects failed VHF startup despite successful cleanup.

Build/test pending on DEVPC001. USB-init early failure and normal teardown remain separate audit topics. This is not proof of full R828D hardware reset or safe live deployment.

## 2026-10-17 USB resource and control transfer audit

Found and patched USB initialization failures in rx888_usb_init: unsuccessful startup previously nulled dev_handle before trying to close it; SuperSpeed-rejected open handles were not closed; queuedepth was assigned after the allocation loop so partially allocated buffers were not freed; USB configuration descriptor and transfer allocation failures were not handled adequately. The branch now closes outstanding handles, sets cleanup capacity before allocating, and tests those allocation results. FX3 OUT control routines command_send, argument_send, control_send and control_send_byte now reject short (nonnegative) libusb control transfers rather than treating them as successes; inbound control_recv already returns the actual byte count, checked by the R828D/Si5351 code.

Scope and limitations: These are source-level repairs, NOT yet compiled or tested on DEVPC001. The shared ezusb.c transfer change touches more than the VHF path; a full offline build/regression is warranted. This is not a complete audit of USB interface-claim ownership, all early libusb init failures, or every unchecked return elsewhere in RX888. No production host changes.

## FX3 exact OUT-transfer test checkpoint

Extracted exact-length OUT transfer success logic to src/fx3_transfer_result.h and applied it to four ezusb.c helpers. New tests/test_fx3_transfer_result.c tests expected lengths of 0, 1, 4, 8, 255 and 65535 bytes with exact-length success, short transfer, negative USB error and overlength results. CI includes the test. Changes are committed but not yet executed or build-validated on DEVPC001. This directly tests the result-classification helper, not actual libusb mocking or USB allocation/teardown. Remaining resource-fault test requires a dedicated mocked libusb harness.
