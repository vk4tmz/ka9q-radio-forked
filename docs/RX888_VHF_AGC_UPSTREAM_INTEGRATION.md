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

## 2026-10-17 tuner-reference staging

Added the R828D I²C address and Si5351 CLK2/PLLB register constants to `src/rx888.h`, and ported `rx888_set_tuner_ref` to the upstream-derived `src/rx888.c`. This helper calculates PLLB/multisynth settings, programs CLK2, checks clock-enable/PLLB-lock and returns the configured reference. `SI5351_VALUE_LOL_B` was identified as a missing macro during source inspection and added in a follow-up commit.

**Important:** The helper is staged but not invoked: existing upstream VHF frequency setup remains disabled with `#if 0`, and the R828D PLL and full VHF lifecycle have not been integrated. No full C compile or tuner-hardware validation has occurred. This staged code is therefore **not** a working VHF receiver or validated feature. Retain Phil's current processing/filter/FM/timing implementations unchanged.

## 2026-10-17 R828D tuner/PLL staging

Staged the **existing VHF branch** R828D tuner init table, `Freq_ranges` RF tracking map (local `static const`), `rx888_set_vhf_mode` startup and `rx888_set_tuner_frequency` PLL/tracking routine into upstream-based `src/rx888.c`. The older, disabled upstream `#if 0` tuner stub remains for comparison. The integrated code is **not connected to setup or to runtime command handling**. This is source staging only, not validated reception. Neither manual-gain integration nor hardware AGC was enabled, and Phil's receive callback, FFT/filter, FM, timing, and status paths remain untouched.

**Required before build/activation:** check declaration order and dependencies against the current upstream definitions, remove or isolate obsolete disabled stub safely, implement the upstream-compatible configuration and VHF/HF lifecycle, explicitly keep manual gain default and host HF AGC excluded from the tuner, then run a complete RX888-only compile and offline tests. The staged tuner code retains original research-stage I²C `assert` assumptions and needs error-path review. No receiver was restarted or retuned.

## 2026-10-17 initial manual VHF configuration wiring

Ported the independent R828D manual LNA/mixer and IF VGA register writes into upstream `rx888_set_att` and `rx888_set_gain`; preserved HF paths. Registered `vhf-rf-gain` (29.7 dB default) and `vhf-if-gain` (16.3 dB default) config keys. Enabled the host-side frequency-configuration path when a valid VHF frequency is explicitly provided, preserving HF when none is supplied. VHF startup explicitly disables the **HF** software gain feedback loop. Hardware tuner-auto AGC is deliberately **not** incorporated.

Crucial compatibility fix: upstream's undersampling/Nyquist initial frontend frequency may be nonzero at `undersample=1`, but the VHF tuner lifecycle requires zero as the uninitialised state. Reset this sentinel before conditional VHF tuning so the first tune enters `rx888_set_vhf_mode`. This needs tests across HF default, invalid VHF, valid VHF and undersample settings.

**Status: code committed, not compiled or hardware validated.** Further review needed for config validity and cleanup on tuner failure, Si5351/R828D I²C return checking, HF↔VHF transitions, RF gain status calibration, and sample-format/frequency placement. No production changes.

## 2026-10-17 integrated RX888-only CI build gate

Added `.github/workflows/rx888-vhf-upstream-integration.yml` on the isolated integration branch. It builds the full `src/radiod` and `src/rx888.so` targets with RX888-only `ENABLE_RX888=1`, every other `ENABLE_*` explicitly zero, `NATIVE=0`, and Ubuntu 24.04 dependencies. It also compiles and runs isolated manual RF/IF gain table tests and R828D mode-bit tests. No installation, receiver access, FX3 firmware changes, or running radiod.

**GitHub workflow status not yet verified.** Code integration remains compile-unverified and has not been exercised with an R828D. Inspect actual job logs for build failures, then fix on the integration branch only. The tests exercise standalone gain helpers, not the entire tuner command lifecycle. Hardware tuner AGC is not enabled here.

## 2026-10-17 opt-in R828D hardware AUTO gain calibration trial

An experimental `vhf-auto-gain = true` boolean is now recognized by RX888 VHF setup (default false). After VHF tune, AUTO sets LNA register 0x05 bit 4 clear and mixer register 0x07 bit 4 set; R828D IF VGA register 0x0c remains at nearest fixed `vhf-if-gain` (default 16.3 dB). The existing manual RF gain and fixed IF route is unchanged when AUTO is absent/false. Hardware AUTO does **not** enable the unrelated HF software AGC. This is an initial tuner mode, not yet a verified runtime mode switch; calls to generic manual gain/attenuation control may override it. The software has no live readback of instantaneous AUTO RF gain, so reported/calibrated dBm values must not be treated as trustworthy during AUTO. Compare relative demod metrics, error rates, and repeated fixed-signal conditions instead.

**Hardware trial not yet authorised/executed; compile verification pending.** Safely coordinate stopping the specifically identified AIS receiver, preserve complete current configuration and executable, and verify rollback before trial. Do not touch the other AIS receiver or run any RX888 auto-firmware loader without explicit plan.
