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
