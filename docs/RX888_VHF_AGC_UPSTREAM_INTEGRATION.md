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
