# RX888 MkII VHF AGC upstream integration

**Status (2026-10-17):** integration staging only; not deployed, not a functional VHF AGC port.

## Baseline and source of truth

Created this branch at upstream `ka9q/ka9q-radio:main` commit `484ce92e0f9e60ae4a7321b22171327cfdedd1cb`. At creation it compared **identical** to that upstream main commit (0 ahead / 0 behind). The source research and experimental AGC implementation remain preserved in `feature/rx888-vhf-hardware-agc-lab`. No merge, rebase or force-push was performed.

## Ported in this checkpoint

- `src/rx888_vhf_gain.h`: experimental R828D RF/IF gain tables and index helpers
- `src/rx888_vhf_agc_bits.h`: pure LNA/mixer gain-mode bit helpers
- `tests/test_rx888_vhf_gain.c` and `tests/test_rx888_vhf_agc_bits.c`: standalone C gain-model and mode-bit checks

These are **independent helper/test files only**. The upstream `src/rx888.c` remains **unchanged** and does not yet call them. No changes were made to upstream `fm.c`, `filter.c`, `radio_status.c`, or build files.

## Integration concerns before porting rx888.c

Both upstream and the lab changed `src/rx888.c` substantially; the lab is 1838 lines versus upstream 1504. Upstream also changed `fm.c`, `radio_status.c`, `filter.c`, `misc.c`, and build interfaces. Avoid wholesale replacement of upstream files with lab versions.

Next: review functions and dependencies side-by-side, port minimal VHF-mode/gain changes into upstream's current code, preserve upstream changes to timing, DSP and interfaces, adapt control/status so tuner-auto gain is not misreported as fixed, then validate an RX888-only build and offline runtime tests. Follow documented build flags including `ENABLE_SOAPY=0`.

Production RX888 and both AIS receive pipelines must remain uninterrupted. Do not start `radiod`, flash FX3 firmware, change services, or run `make install` during software validation.

## 2026-10-17 VHF tuner reconciliation findings

Source comparison between upstream `src/rx888.c` and the existing `feature/rx888-vhf-gain-controls` branch establishes an important dependency: upstream currently has `rx888_set_tuner_frequency` inside a disabled `#if 0` block, relies on `R82XX_ATTENUATOR`/`R82XX_VGA` firmware commands in its VHF gain paths, and does not have the verified host-side R828D tuning flow. The experimental branch implements host-driven tuner reference clock, R828D I²C register writes, full tuner initialisation, RF tracking and PLL configuration, HF/VHF routing and explicit manual RF/IF gain settings. Therefore importing only gain routines would **not** yield working VHF receive functionality.

Staged `src/r828d.h` from the existing VHF branch as *definitions only*, alongside the already staged gain helpers and tests. Upstream `src/rx888.c`, `src/fm.c`, `src/filter.c`, `src/radio.c`, `src/radio_status.c` remain unchanged. The VHF tuning implementation is **not yet enabled or compiled** on the integration branch.

Next required integration: isolate and adapt the I²C and Si5351 support functions, tuner startup and shutdown, PLL/tracking methods, setup/command flow, and validation of gain/state semantics against upstream's updated sample processing, timing and device lifecycle. Keep tuner-auto AGC disabled until the manual baseline builds and is validated. Avoid copying the older RX888 file wholesale.

## 2026-10-17 low-level R828D I²C helper stage

Added the original VHF lab branch's `r828_status`, `r828_read`, `r828_write`, `r828_write_byte`, `r828_write_byte_mask`, and `bitrev` helpers plus tuner shadow/status buffers into upstream-based `src/rx888.c`; included `r828d.h`. These functions use existing FX3 I²C command endpoints. **The VHF tuner startup, PLL and frequency tuning remain disabled upstream, and hardware AGC is not enabled.** No existing sample processing, filter, FM or radio/status implementation was replaced.

**Validation:** GitHub source changes committed, but **no full C compilation or executable I²C transport tests have been run**. This stage preserves source dependencies only. Before compiling, inspect static helper placement, compiler warnings, and error paths: current lab functions use `assert` for transport results, and release builds may disable assertions. These checks are needed before treating hardware I²C operations as reliable.
