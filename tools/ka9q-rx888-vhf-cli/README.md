# KA9Q RX888 MkII VHF command-line tool

## Architecture decision (2026-10-20)

**Use the existing KA9Q `src/rx888.c` receiver implementation.** Do **not**
reimplement its Si5351 clock, R828D tuner, FX3 USB, or transfer queue logic.
The previous exploratory diagnostics in `tools/rx888-vhf-standalone/` and
`src/rx888_vhf_*.h` remain preserved for possible future investigations.
They are **not** the production CLI's hardware backend.

## Requested outcome

A tool with capability comparable to or better than `sddc_vhf_test`:

- Select RF center frequency, especially **92.7 MHz Mix FM** for first on-air test.
- Configure ADC sample rate, reference/PPM calibration, R828D RF/IF gain,
  and FX3 USB queue/transfer sizes.
- Select firmware intentionally; never silently reset/reload a running unit.
- Bound capture duration or stream until interrupted; output documented
  IQ sample format to a file/stdout with sample and USB dropout accounting.
- Reuse KA9Q's existing RX888 setup/tune/start/stop routines.
- Later, support AIS 161.975/162.025 MHz and KA9Q channelization.

## Phase 1 (this commit) — CLI syntax only

The new `ka9q-rx888-vhf-cli` executable parses options and prints a dry-run
configuration. **It intentionally refuses real hardware operation**, since the
adapter to KA9Q's existing `rx888.c`/frontend streaming path has not yet
been implemented. A successful dry-run is **not** a received RF signal.

```bash
make -C tools/ka9q-rx888-vhf-cli
./build/ka9q-rx888-vhf-cli -f 92.7M -s 20M \
  --rf-gain 29.7 --if-gain 16.3 --seconds 10 \
  --output /tmp/mix927.iq --dry-run

gcc -std=gnu11 -O2 -Wall -Wextra -Werror \
  -Itools/ka9q-rx888-vhf-cli \
  tools/ka9q-rx888-vhf-cli/test_cli.c -lm \
  -o /tmp/test_ka9q_rx888_cli
/tmp/test_ka9q_rx888_cli
```

## Immediate next development tasks

- [ ] Inventory the **actual** `sddc_vhf_test --help` output and match compatible argument names and semantics; do not guess unsupported flags.
- [ ] Refactor the minimum KA9Q `rx888.c` static lifecycle functions behind an internal reusable interface, preserving existing `radiod` behavior and avoiding duplicate hardware implementations.
- [ ] Design frontend sample callback/file sink using the same code path as `rx888_start_rx`, `proc_rx888`, and `rx_callback`; carefully check dependencies on KA9Q DSP globals.
- [ ] Support accurate IQ capture output format, byte order, timestamps, sample count and USB error statistics.
- [ ] First dry-run and unit-test argument compatibility; then inspect USB ownership/session status before **any** hardware test.
- [ ] First controlled 92.7 MHz Mix FM capture and validate FM carrier/spectrum/pilot/audio (depending on output stage).
- [ ] Compare output to established SDDC baseline; expand to AIS.
- [ ] Update `PROJECT_STATUS.md` to reflect this corrected architecture.

## Safety

Current RX888 FX3 firmware was reported as `sddc_vhf_test`; KA9Q's
default W1EUJ FX3 firmware may differ in USB streaming control.
Firmware `TESTFX3` response version 2.2 alone does not prove full
protocol compatibility. Keep independent Nooelec AIS session untouched.

## Verified symbol audit and capture integration checkpoint (2026-10-20)

The CLI parser, raw sink regression, and modified `src/rx888.so` compile all PASS
on devlap200 at commit `746f21be`. The user captured the full
`nm -D -u src/rx888.so` output, confirming that `rx888.so` is a KA9Q
plugin rather than a self-contained executable:

- Link actual `src/config.c` routines (`config_getstring/int/float/double/boolean`,
  `config_validate_section`) and `parse_frequency`.
- Resolve KA9Q utility functions `gps_time_ns`, `dist_path`, `stick_core`.
- For normal radiod, preserve `scale_AD`, `scale_ADpower2FS` and
  `write_rfilter`. Standalone `rx888_capture_real_s16` uses an alternate
  callback and **does not invoke the FFT input writer**.
- Export the compatible KA9Q globals `Verbose`, `Description`, `Serial`,
  `USB_busnum`, `USB_devnum`.
- Do not change library symbol contracts or introduce stubs that masquerade
  as working DSP services. Resolve all dynamic dependencies before hardware use.

**Live capture remains disabled in the CLI**. Remaining integration: compile
a small host executable that supplies the KA9Q service symbols via reuse of
existing source objects and calls `rx888_setup` with an in-memory ini dictionary;
then call the capture API with file ownership and cleanup carefully handled.
Ensure the initial RX888 hardware setup is compatible with the currently loaded
FX3 firmware, and check USB exclusivity before any hardware action.

## USB failure-path hardening (2026-10-20)

- `rx888_start_rx` now returns USB submit / STARTFX3 failures rather than asserting.
- Standalone `rx888_capture_real_s16` now transitions to STOPPING, cancels outstanding transfers and drains their callbacks before detaching the stack-owned capture sink or releasing the device. This avoids the earlier path that could return with callbacks still referencing freed memory. On an unrecoverable libusb event error during draining, it exits the process rather than freeing potentially active transfer state.
- **Hardware/firmware compatibility remains unverified.** The USB firmware currently on the RX888 originated from the SDDC experiment. The KA9Q driver’s GET/SET, clocks and streaming assumptions must be reviewed against that firmware before executing live capture.
- The `capture-host` binary can initiate hardware access without `--dry-run`, so do **not** run it live yet. Current validation is compile, `--help`, dry-run and offline regression only.
- Retain earlier exploratory diagnostics; they may be valuable later but are not imported as production hardware code.

## Live capture safety gate (2026-10-20)

The `ka9q-rx888-capture-host` requires the exact environment variable
`KA9Q_RX888_ALLOW_LIVE=YES` before it will call KA9Q setup.
`--dry-run` and `--help` remain USB-safe. **Do not set this opt-in yet**:
hardware firmware compatibility has NOT been demonstrated; the current
FX3 firmware originated with SDDC, not necessarily KA9Q.

USB startup failure now reports transfer submission failures rather than
asserting. On capture termination the dedicated callback queue is cancelled
and drained before returning; NOT_FOUND cancellation of never-submitted
buffers is handled as harmless. No hardware test has verified these paths.

Next verification:
```sh
make -C src ENABLE_RX888=1 rx888.so
make -C tools/ka9q-rx888-vhf-cli test
make -C tools/ka9q-rx888-vhf-cli capture-host
./build/ka9q-rx888-capture-host -f 92.7M -s 20M --seconds 2 --output /tmp/mix927.s16
# Expected: REFUSED... no USB hardware access (without explicit opt-in)
```
Confirm the actual SDDC FX3 command compatibility before attempting a live capture.
