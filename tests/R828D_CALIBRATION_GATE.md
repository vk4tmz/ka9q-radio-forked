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
