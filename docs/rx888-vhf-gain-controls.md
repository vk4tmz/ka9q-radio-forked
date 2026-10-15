# RX888 MkII VHF manual gain controls

**Status:** Development branch only. KA9Q firmware/hardware performance not yet tested.

## Reason

The existing KA9Q code passes truncated dB values as R82XX gain command arguments, but the independent SDDC firmware uses **indices**: combined RF LNA/mixer indices 0–28 and IF VGA indices 0–15. Different RX888 firmware implementations need not share those command semantics.

This branch uses KA9Q's existing R828D I2C passthrough to update hardware registers directly, paralleling SDDC's `set_all_gains()` and `set_vga_gain()`. For RF, R5 bit 4 enables manual LNA, R7 bit 4 selects manual mixer, and their low nibbles select corresponding indices. For IF, R0C low nibble selects the VGA index.

## Example config in a VHF RX888 frontend

```ini
vhf-rf-gain = 43.4
vhf-if-gain = 26.5
```

Nearest supported steps are selected from tables of 29 nominal RF and 16 nominal IF gain levels. Defaults RF 29.7 dB / IF 16.3 dB. With VHF frequency configured, VHF AGC is disabled. Existing HF gain/attenuation behaviour is unchanged.

**Interim runtime API:** Generic KA9Q gain controls the VHF IF stage in nominal dB. Generic attenuation controls VHF RF loss relative to maximum 49.6 dB; e.g. for RF 43.4 dB request attenuation 6.2 dB. The generic metadata consequently reports IF-stage gain under rf_gain and relative RF loss under rf_atten; these aren't yet equivalent to absolute RF power calibration.

## Standalone tests without RX888 hardware

```sh
cc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_rx888_vhf_gain.c -lm -o /tmp/test_rx888_vhf_gain
/tmp/test_rx888_vhf_gain
```

## Hardware test preconditions

1. Compile the KA9Q radio driver on devlap200. This environment lacked libusb headers and cannot certify a full build.
2. Preserve the still-running independent SDDC/AIS system. Do not switch firmware or disturb the device until an agreed test window.
3. On switchover, verify gain changes against receiver level and DAB quality; command acceptance is not sufficient.
4. Verify the HF path, runtime controls, metadata, retuning, clipping and VHF AGC behaviour.
5. Restore the temporarily disabled KA9Q udev firmware rule only under a controlled firmware switch.

## Reference implementations

- renardspark/SDDC_Driver, `SDDC_FX3/driver/tuner_r82xx.c`, `set_all_gains()` and `set_vga_gain()`.
- renardspark/SDDC_Driver, `Core/radio/RX888R2Radio.cpp`.
