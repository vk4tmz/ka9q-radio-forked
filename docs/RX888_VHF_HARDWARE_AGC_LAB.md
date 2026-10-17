# RX888 MkII VHF hardware AGC experiment

**Branch:** `feature/rx888-vhf-hardware-agc-lab`, based on `feature/rx888-vhf-gain-controls`.

**Status:** source implementation for offline review; **not hardware tested, not approved for production, not deployed**.

## Motivation

The upstream `rx888-vhf` AGC feedback loop manages HF AD8340 gain, not the R828D's distinct LNA/mixer and IF/VGA. Our preceding gain-controls branch already exposes two manual VHF gain settings through R828D masked I²C writes. The original ExtIO_sddc driver and Osmocom's R82XX tuner driver establish LNA and mixer automatic-mode bit semantics.

## Added configuration (experimental branch only)

```ini
vhf-gain-mode = manual
vhf-rf-gain = 29.7
vhf-if-gain = 16.3
```

`manual` is default, retaining the preceding branch's RF/IF manual setup and log. Unknown mode text also falls back to manual with a warning. Optional `vhf-gain-mode = tuner-auto` enables **R828D hardware LNA and mixer AGC** after applying the manual RF gain reference and fixed IF gain.

- LNA: register `0x05`, mask `0x10`, set bit to **0** (automatic).
- Mixer: register `0x07`, mask `0x10`, set bit to **1** (automatic).
- IF/VGA: register `0x0c`, remains at the configured `vhf-if-gain` manual value.
- HF host AGC remains disabled for the VHF configuration. The source does **not** substitute an HF software feedback loop for R828D tuner AGC.
- `vhf-rf-gain` in automatic mode is a configuration/reference value used during setup; it is **not** the instantaneous hardware-AGC RF gain. Existing `frontend->rf_atten` / frontend scaling and status do not yet model variable hardware-AGC gain, so their RF-level calibration may be unreliable in tuner-auto mode. Do not interpret status RF-gain readout as actual hardware AGC gain.

## Initialization/retune review

`rx888_set_vhf_mode` writes the full R828D initialisation table when switching from HF/unknown to VHF. Thereafter `rx888_set_tuner_frequency` adjusts RF tracking, PLL and divider registers; the source review identified **no normal VHF-to-VHF retune writes to registers 0x05/0x07**, but an HF-to-VHF transition will reinitialize the tuner and the gain mode requires reapplication. Current mode selection runs on the initial configured frequency setup; dynamic runtime VHF/HF mode switching is **not covered or qualified** for AGC.

Manual RF gain operations still force manual LNA/mixer mode. Therefore subsequent explicit analog attenuation/gain controls can override tuner-auto; persistence across runtime control commands is **not yet guaranteed**. This limitation must be addressed before considering an upstream-ready feature.

## Required offline checks before a live test

1. Compile the branch **without installation**, observing `vk4tmz/ka9q-radio` `AGENTS.md` and `docs/RADIOD_BUILD.md` RX888-only build rules.
2. Verify parser/default/manual/unknown-mode behavior by unit tests or source-isolated harness.
3. Validate masked writes and correct order: configure manual RF+IF, then toggle R5/R7 mode bits.
4. Test retuning and runtime manual control interactions (via mocks/stubs or code review).
5. Verify no changes to HF paths, USB firmware loading, or production multicast configuration.
6. Confirm currently loaded devlap200 firmware and I²C write behaviour separately before hardware experiments.

## Future RF tests (maintenance window only)

Use known-good manual baseline, then explicit tuner-auto at fixed IF gain; monitor DAB+ decoding/errors, ADC overrange events, AIS valid messages/CRC and repeated distant MMSI observations. Do not draw conclusions from different vessel-traffic windows alone. Preserve rollback branch and original receiver configuration.

## References

- [RX888 VHF experimental gain branch](https://github.com/vk4tmz/ka9q-radio-forked/tree/feature/rx888-vhf-gain-controls)
- [Original ExtIO R82XX tuner gain modes](https://github.com/ik1xpv/ExtIO_sddc/blob/master/SDDC_FX3/driver/tuner_r82xx.c)
- [Osmocom RTL-SDR tuner implementation](https://github.com/osmocom/rtl-sdr/blob/master/src/tuner_r82xx.c)
- [VK4TMZ research notes](https://github.com/vk4tmz/ka9q-radio-misc/blob/main/docs/rx888-vhf-agc-research-2026-10-17.md)

## 2026-10-17 source validation checkpoint

- Eight source-level predicates were checked against the GitHub branch content: **8/8 matched**. They cover manual default, parser key, R5/R7 AGC mode masks, order of RF/IF configuration relative to AGC enable, suppression of the HF software AGC loop, and VHF retune inspection.
- Added two offline source-contract tests (`tests/test_rx888_vhf_agc_source.py`, now **six** methods): the normal retune function must not directly write R5/R7 gain-mode bits; and the explicit manual RF control path is recognised as a mode override requiring further design.
- **These are static/source assertions, not proof of C compilation, running Python unittest, hardware AGC functionality or production compatibility.** The automated Python suite and C build still require execution in an isolated checkout.
- **Known issue:** runtime manual RF gain commands currently override auto mode, and switching HF→VHF reinitializes the tuner. For upstream-quality runtime transitions, mode needs explicit stored state/reapplication and honest status fields. Do not paper over this with a startup-only test.
- Continue to leave the live RX888 and Nooelec AIS-catchers running uninterrupted.

## Runtime state refinement

The experimental branch now tracks `sdr->vhf_tuner_auto` separately from the legacy `frontend->rf_agc` software-feedback flag. It sets the hardware-mode marker only after writing both R828D automatic mode bits. An explicit runtime RF attenuation command still forces manual LNA/mixer operation and now logs this change rather than silently leaving a stale auto-mode marker. HF/VHF tuner reinitialisation invalidates the marker; initial VHF configuration reapplies the selected mode.

**Still not solved:** this marker records the host's intended register mode, not measured tuner gain or hardware readback. The existing generic frontend status values do not distinguish AGC-adjusted LNA/mixer gain from fixed manual RF gain. Runtime receiver control of a *new* AGC policy (manual -> tuner-auto without restarting) has not been designed. A further design review of the control/status API is required before proposing an upstream-quality feature.

The Python source-contract suite now has **seven** tests. They are structural regression guards, not a substitute for executable tuner/mocked register tests, a C build, or hardware RF qualification.

## Offline CI and gain telemetry review

Added isolated GitHub Actions source-contract execution in `.github/workflows/rx888-vhf-agc-offline.yml` on the experimental branch. This is strictly a Python source-contract suite; it **does not compile radiod**, emulate an R828D, or attach to a receiver. The workflow's actual success/failure must be checked in GitHub Actions before claiming a test pass.

For `tuner-auto`, the existing frontend RF attenuation field remains a **manual reference**, not an observed instantaneous LNA/mixer gain. The startup diagnostic explicitly warns of this. **Do not feed or present it as calibrated actual RF gain.** The correct follow-up requires reading tuner gain state (if supported by the chip/driver) or introducing an explicit "AGC gain unknown" state in status/telemetry without silently changing existing protocol meanings. This status/API work is outstanding and blocks hardware qualification.

## 2026-10-17 executable register test checkpoint

- Added `src/rx888_vhf_agc_bits.h` with pure, side-effect-free LNA/mixer mode-bit calculations. The radiod path now calls these same helpers before masked R828D writes; this avoids divergence between model and production calculation.
- Added `tests/test_rx888_vhf_agc_bits.c`, exhaustive across all 256 old-register values, checking automatic/manual selection, preservation of non-mode bits, idempotence and auto-to-manual transitions.
- The standalone C test **passed locally** using `cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=undefined`. It was tested as a pure register model only, **not** linked to radiod, I²C or the receiver.
- Updated the experimental GitHub Actions workflow to run the source-contract suite and the standalone C test. Workflow status is **unconfirmed**; available workflow lookup only covers PR runs and returned no entries.
- Updated source-contract tests for the refactor. Still pending: executing the entire GitHub branch's Python tests and compiling the full RX888-only radiod binary without installation.
- No status protocol change has been made: instantaneous tuner AGC gain remains **unknown** and the legacy attenuation field remains a manual reference.
