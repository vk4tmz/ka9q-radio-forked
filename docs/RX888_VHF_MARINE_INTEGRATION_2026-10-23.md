# RX888 MkII VHF Marine integration — initial checkpoint (23 October 2026)

Target branch: `feature/rx888-vhf-sync-main` (the original `feature/rx888-vhf-marine` name is not present on GitHub). Reference: `feature/rx888-vhf-standalone-test`, whose KA9Q/W1EUJ 2.3 + manually fixed R828D path decoded AIS reliably after genuine USB cold boot.

## Scope / explicit exclusions

Only integrate general KA9Q RX888 VHF driver support into normal `radiod`:
- Correct 1-bit R828D IF VGA_MODE mismatch; use actual LNA/mixer/IF gain tables.
- Make analogue IF bandwidth configurable with a VHF-only 8 MHz default.
- Preserve explicit tuner LO semantics and document necessary IF offset.
- Improve fail-fast I2C, clock and PLL checks.
- No standalone CLI, CF32 sink, FIFO, ADC capture diagnostics or AIS-Catcher orchestration.
- No experimental firmware-owned tuner, hardware RF AGC, auto-calibration or untested RF-input switching.
- No changes to production receivers, udev rules, services or `ka9q-ais`.

## Changed files and behaviour

`src/rx888.c`:
- Validates `vhf-bandwidth-mhz` against the 21 exact `r828d_bw_profiles` and applies the chosen masked R0A/R0B/R1E filter profile. **Default 8.0 MHz only when entering VHF**.
- The `frequency` option continues to mean R828D tuner **LO**, not RF channel centre. Prints actual nominal RF centre = measured tuner LO - selected IF and the nominal RF window. No silent retune when bandwidth changes.
- Validated RF LNA/mixer `vhf-rf-gain` nominal 37.2 dB default and fixed IF `vhf-if-gain` nominal 16.3 dB default. Nearest of 29 RF / 16 IF legal steps is selected; IF write uses `0x1f` mask, which clears `R0C[4]` VGA_MODE.
- Initial tuner value at R0C corrected to `EB`, and 16.3 dB IF index 8 yields `E8`. Both initial and runtime IF writes apply this correction.
- `vhf-auto-gain=false` accepted; true deliberately rejected until hardware AGC tested. HF automatic gain behaviour is not intentionally modified.
- R828D I2C status/read/write now handles failures without relying on `assert`; register shadow updates occur only after successful writes. Initial tuner reference/27 register writes, PLL register operations and lock checks are gated to prevent false ready state.
- The existing VHF error path still needs *full cleanup/lifecycle validation*; a failed setup may leave CLK2/antenna/tuner partially configured. Do **not** operate the integrated receiver until the error-recovery path and build are validated.

Portable non-CLI files:
- `src/rx888_vhf_gain.h`: validated nominal gain tables (29 RF/16 IF).
- `src/r828d_bandwidth_profiles.h`: 21 width/IF and masked register profiles.
- `src/rx888_vhf_vga_mode.h`: fixed VGA initial byte and five-bit mask.
- `tests/test_rx888_vhf_gain.c`, `tests/test_r828d_bandwidth_profiles.c` and `tests/test_rx888_vhf_marine_config.c`: pure, hardware-free checks.

Configuration fragment: `config/examples/radiod@ka9q-vhf-marine.conf.d/01-rx888.conf`. It documents every supported gain and analogue bandwidth option with IF centre and the LO necessary to keep a 159.000 MHz RF centre. **Not a complete working marine radiod configuration**: global/status multicast and separate CH12/CH16/AIS channel sections still need integration.

### Nominal 8 MHz marine RF range

With `frequency=163570000` LO and `vhf-bandwidth-mhz=8.0` (nominal IF +4.570 MHz):
- Centre 159.000 MHz; nominal RF span ~155–163 MHz, subject to real analogue-filter response.
- CH12 156.600 MHz and CH16 156.800 MHz; AIS-A 161.975 MHz and AIS-B 162.025 MHz are nominally inside.
- The 20 MS/s real-sample path provides a 10 MHz theoretical Nyquist half-span; 8 MHz is the selected R828D analogue IF filter profile, not a promise of a flat 10 MHz RF passband.
- This channel mapping is a design check; actual `radiod` real/complex IF orientation, spectral purity and four simultaneous channels have **not** been verified on hardware.

## Offline compile/tests

These must pass **before a receiver switchover**. The GitHub Actions offline workflow is `.github/workflows/rx888-vhf-marine-offline.yml`. It does not install anything or access USB. Local equivalent:

```bash
cd ~/tools/ka9q-radio-vhf
git fetch origin
git switch feature/rx888-vhf-sync-main
git pull --ff-only

cc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_rx888_vhf_gain.c -lm -o /tmp/vhf-gain-test
/tmp/vhf-gain-test
cc -std=c11 -Wall -Wextra -Werror tests/test_r828d_bandwidth_profiles.c -o /tmp/vhf-bandwidth-test
/tmp/vhf-bandwidth-test
cc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_rx888_vhf_marine_config.c -lm -o /tmp/vhf-marine-test
/tmp/vhf-marine-test

make -C src -j2 radiod rx888.so NATIVE=0 \
  ENABLE_RX888=1 ENABLE_AIRSPY=0 ENABLE_AIRSPYHF=0 \
  ENABLE_BLADERF=0 ENABLE_FOBOS=0 ENABLE_FUNCUBE=0 \
  ENABLE_HACKRF=0 ENABLE_HYDRASDR=0 ENABLE_RTLSDR=0 \
  ENABLE_SDRPLAY=0 ENABLE_SIG_GEN=0 ENABLE_SOAPY=0
```

These commands are for an offline checkout and are not required tonight. They have NOT been executed on `devlap200`.

## Next work / gates

1. Verify RX888-only compile and offline unit tests; resolve any compiler warnings, complete config-validation checks, and verify the resulting code has no standalone-CLI dependency.
2. Add fault-injection regression tests for VHF setup failures, especially short/failed R828D reads, clock timeout, PLL lock failures and recovery; keep HF regression tests.
3. Prove `radiod` frequency sign and actual sampled marine passband mapping with simulation/replay before USB.
4. Build a complete isolated marine `radiod` config with multicast status/data and CH12, CH16, AIS-A/B routing; verify no SSRC collisions with existing `ka9q-ais`.
5. Only during an explicitly agreed maintenance window: stop the standalone overnight receiver, snapshot working binaries/config/USB firmware provenance, test the integrated marine `radiod` and roll back cleanly if needed. Leave NooElec on 8101 unaffected.

**Status:** Source implementation and offline validation workflow committed; full CI build status, real `radiod` channel mapping and live hardware acceptance not verified. This is a development checkpoint, not permission to redeploy the active receiver.
