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
