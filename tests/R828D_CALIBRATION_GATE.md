# R828D calibrated analogue path: preflight gate

Status: **OFFLINE DESIGN ONLY — not wired to rx888.c**. Updated 2026-10-22.

The working KA9Q RX888 MkII capture path must remain unmodified. The experimental 8 MHz option still has the earlier unsafe full-register writes; DO NOT pass `--analog-bandwidth 8M`.

Files added:
- `src/r828d_cal_plan.h`: pure state decisions for status nibble 0/15 retry, good value, error abort, and final masked 0x0A.
- `tests/test_r828d_cal_plan.c`: standalone assertions; no USB/I2C/SDR dependencies.

Compile and run, from this repository:
```bash
cc -std=c11 -Wall -Wextra -Werror tests/test_r828d_cal_plan.c -o /tmp/r828d-cal-test
/tmp/r828d-cal-test
```

Verified from current working baseline trace: initial `0A=DB,0B=6B,0F=68,1E=0E`; tune and gain do not alter those filter registers. 60 s baseline producer/decoder exit 0, USB failures 0. Shadow is NOT a readback.

**Missing before any live opt-in feature:**
1. Isolation of the PLL tuning routine so it can park at e.g. 100.1 MHz for calibration **without recursively re-running VHF init**.
2. Verified PLL lock and mandatory restore of exact original LO, tracking table, reference, and all modified register shadow upon success or failure.
3. Proper handling if an I2C write/read, PLL lock, or calibration clock-enable/disable fails; do not claim a safe rollback just from a software snapshot.
4. Calibration clock gating; 0x0B trigger timing, status low-nibble acquisition, bounded retries.
5. Bandwidth selection matching actual +1.570 MHz digital IF extraction. Reference 8 MHz preset assumes 4.570 MHz IF; copying its values is not a controlled comparison.
6. Hardware-independent fault injection at each IO/PLL stage, plus a verified rollback test on the physical device only with consent.

No live hardware test or capture is prescribed by this commit. After the offline gate passes, design a single baseline/variant/baseline test using an RF reference, not traffic counts alone.

## Offline transactional rollback exercise (2026-10-22)

`src/r828d_cal_transaction.h` and `tests/test_r828d_cal_transaction.c` introduce a **mock-only** execution harness, never called by the RX888 capture path. It exercises calibration clock/trigger writes, injected status nibble, and best-effort restoration of filter registers `0A/0B/0F/1E` plus a backend PLL-restore callback. Tests inject faults during forward and cleanup writes and assert that restoration failures are reported as an **uncertain** state.

```bash
cd ~/tools/ka9q-radio-vhf-standalone
git pull --ff-only
cc -std=c11 -Wall -Wextra -Werror tests/test_r828d_cal_plan.c -o /tmp/r828d-cal-plan-test
cc -std=c11 -Wall -Wextra -Werror tests/test_r828d_cal_transaction.c -o /tmp/r828d-cal-tx-test
/tmp/r828d-cal-plan-test && /tmp/r828d-cal-tx-test && echo "R828D offline C tests PASS"
```

**Limitations:** the harness uses a mock PLL `park/restore`; it does not model actual PLL register transitions, hardware lock verification, antenna input switching, complete register restoration, power loss or process death. Its `status` callback injects a pre-decoded nibble. It deliberately restores even after success and never selects an analogue variant. A logged successful rollback in this harness must not be called a verified **hardware** rollback. The live tuner and working application remain unchanged.

## Consolidated state-model checkpoint (2026-10-22)

The previous standalone ZIP checkpoint has been incorporated directly into **this repository** as `src/r828d_state.h` and `tests/test_r828d_state.c`. No download or manual file transfer is required. No changes were made to production `src/rx888.c`.

The model attempts to restore the entire software register image (0x05–0x1F), original LO through an injected retune callback, and lock verification, including injected write/status/lock errors. This is intentionally hardware-disconnected: **a mock retune callback does not implement the real R828D synthesizer or guarantee physical recovery**.

Locally compiled and executed the supplied model test with `cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer`; it printed `R828D offline complete-state rollback tests PASS`. The new repo paths have matching content and should be tested as part of the one consolidated validation with the full driver when hardware integration is complete.

Outstanding high-priority gate: actual production PLL tune/park/restore, confirmed frequency orientation and matching +4.57 MHz digital extraction if 8 MHz IF variant is selected, and failure-safe hardware readback/lock checks. No live `--analog-bandwidth 8M` testing is authorized by this commit.

## Production-PLL integration checkpoint (2026-10-22)

A separately gated `vhf-calibrated-8mhz` flag was added in `src/rx888.c` (default **false**). When set through a configuration dictionary, it invokes masked 8 MHz register controls, the **existing production PLL-lock-checked tuner routine** at 100.1 MHz, calibration trigger/status/retries, and restores the original LO through an explicit force-retune helper. The ordinary VHF/AIS default path remains unchanged.

**Do not enable this flag yet.** The capture-host CLI deliberately does not expose it, because the full integrated `rx888.c` module has not been compiled/linked and tested locally in this session. Source review identified and fixed one failure concern: cached frontend frequency must not suppress the restoration retune after a partial park failure. This addresses only that specific condition, not overall hardware recovery.

**Remaining release gates before live opt-in:** compilation with project build; RX888-specific mock USB/I2C integration tests for calibration and restore errors; measured/grounded IF placement and correct +4.57 MHz extraction; verification of the restore path under actual R828D lock outcomes. In particular, the current source does not prove that every transient clock and PLL register will restore correctly after every failed I2C transaction. Never represent the implementation as proven or authorize a hardware A/B/A based only on mock unit-test success.

The old `--analog-bandwidth 8M` path remains unchanged and unsafe for comparative conclusions. The new mode is separate, not a retrofit of that argument.
