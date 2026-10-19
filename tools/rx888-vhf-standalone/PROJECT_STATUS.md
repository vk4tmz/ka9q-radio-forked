# RX888 MkII VHF standalone driver — project status

**Last updated:** 2026-10-19 (Brisbane, AEST)  
**Repository:** `vk4tmz/ka9q-radio-forked`  
**Branch:** `feature/rx888-vhf-standalone-test`  
**Latest validated checkpoint:** `7b9234f5` (integrated Si5351 mock regression PASS)  
**Host:** `devlap200`  
**Worktree:** `~/tools/ka9q-radio-vhf-standalone`

> This document records observed results separately from assumptions and planned tests.
> Do not run `clock-test` until its live hardware risks and rollback procedure have been reviewed.

## Goals and success criteria

1. Implement an RX888 MkII VHF receive path that operates under KA9Q without relying on a continuously running SDDC receiver process.
2. Control the Si5351 ADC/tuner clocks and R828D tuner using the FX3 firmware USB/I²C protocol.
3. Receive a continuous, correctly timed VHF IQ stream and feed KA9Q's DSP/channelization pipeline.
4. Receive and decode **AIS-A 161.975 MHz** and **AIS-B 162.025 MHz**; subsequently validate voice channels such as Brisbane VTS CH12 and marine CH16.
5. Preserve recoverability, deterministic startup/shutdown, clear failure reporting, sample-continuity checks, and working operational documentation.
6. Protect the independent Nooelec AIS receiver and keep existing, proven RX888/SDDC captures as a comparison baseline.

**Near-term definition of success:** One reproducible, independently captured RX888 IQ sample stream that yields real AIS decodes.  
**Broader definition of completion:** An integrated KA9Q RX888 VHF driver that starts, tunes, streams, decodes, and recovers reliably.

## Current setup / constraints

- Receiver: RX888 MkII. Last verified USB identity `04b4:00f1`, **bus 2 / address 9** (re-enumerate before each live experiment; these numbers may change), USB speed code `4` (SuperSpeed).
- Firmware in FX3 RAM: user reports **`sddc_vhf_test`**, retained because the receiver has not been unplugged/power-cycled. `TESTFX3` reports hardware `0x04`, version `2.2`; this response alone does **not** uniquely prove firmware build identity.
- Last observed AIS sessions: RX888 **STOPPED**, independent Nooelec **RUNNING**. Verify with `python3 ~/tools/fm-measure/scripts/ais/ais-session.py status` before any hardware test.
- Last observed `sudo fuser -v /dev/bus/usb/002/009` listed no process owning RX888's USB device. USB interface exclusivity is checked again by libusb claim.
- Reference input assumed nominal `27,000,000 Hz`; use an explicitly justified PPM correction rather than treating a nominal frequency as a measurement.
- Do **not** upload firmware, issue `STOPFX3`, reset USB, power-cycle, or change the antenna/GPIO setup as part of the clock tests.
- Independent Nooelec capture must not be stopped or modified by these tests.

## Confirmed results

### USB and read-only communication

- `list`, `probe --exclusive --read-i2c`, and `inspect-clocks --exclusive` worked on physical RX888.
- FX3 `TESTFX3` firmware response seen consistently; most recent sequence ended at request count **46** during `compare-clocks`.
- Si5351 I²C status reads and R828D I²C status reads worked.
- Si5351 status was **`0x01`**. Correct decoding: `SYS_INIT=0`, `LOL_A=0`, `LOL_B=0`, `LOS=0`. An earlier version incorrectly treated bit 0 as `SYS_INIT`; corrected at `40328650`.
- Several sequential inspections returned identical clock register contents.

### Captured Si5351 baseline — read only

```text
status reg0             01  (SYS_INIT=0, LOL_A=0, LOL_B=0, LOS=0)
interrupt status reg1   e8
output enable reg3      00
output mask reg9        00
PLL source reg15        00
CLK0 control reg16      4f  (enabled, not powered down)
CLK2 control reg18      6c  (enabled, not powered down)
PLLA reg26..33          ff ff 00 0e 4b fd a0 cb
PLLB reg34..41          ff ff 00 0e 97 fb 42 17
MS0  reg42..49          00 01 00 14 00 00 00 00
MS2  reg58..65          00 01 00 1a 00 00 00 00
```

Using the full Si5351 fractional register fields and **nominal** 27 MHz crystal reference, decoded output clocks are approximately **19,999,999.545 Hz (CLK0)** and **15,999,999.745 Hz (CLK2)**. These are calculated register-derived frequencies, **not physical frequency measurements**. The KA9Q clock planner produces exactly 20 MHz and 16 MHz at nominal reference. Byte-for-byte comparison: **18 of 32 PLL/MS register bytes differ**, consistent with different divider solutions; see `compare-clocks --ppm 0`.

### First physical write-path verification

On `afa4c193` the guarded `echo-reg3` command succeeded:

```text
probe firmware hw=0x04 version=2.2 requests=44
echo-reg3 rc=0 before=0x00 after=0x00
Exit status: 0
```

A subsequent read-only `inspect-clocks` returned all the same captured Si5351 configuration bytes and zero PLL loss-of-lock flags. **What this proves:** the FX3 accepted a Si5351 I²C write of the register's *existing* value and readback matched. **What it does not prove:** changed-value writes, physical output frequency, R828D initialization, ADC streaming, or full PLL reprogramming.

### Offline build and regression tests

User compiled using `gcc -std=gnu11 -D_GNU_SOURCE=1 -O2 -Wall -Wextra -Werror -iquote src` and reported:

| Test | Last observed result |
|---|---|
| `test_rx888_vhf_si5351_sequence` | PASS — mocked dual-clock writes, lock polling, timeout and recovery |
| `test_rx888_vhf_clock_snapshot` | PASS — snapshot, readback verification and restoration-error handling |
| `test_rx888_vhf_fx3_transport` | PASS — request/address mapping, read bit order, short I/O errors |
| `test_rx888_vhf_i2c_echo` | PASS — same-value register write, readback and injected failures |
| `test_rx888_vhf_si5351_decode` | PASS — live/planned clock-register decoding |
| `test_rx888_vhf_clock_health` | PASS — restoration/register status and failures |
| `test_rx888_vhf_clock_integrated` | **PASS at `7b9234f5`** — snapshot → program → PLL poll → restore → verify, including injected errors |

Earlier offline tests also covered calibration, manual R828D control sequences, R828D PLL maths and sequencing, Si5351 register packing and FX3 transport. Their current branch regression status should be reconfirmed when changing underlying routines.

**Note:** `make -B -C tools/rx888-vhf-standalone` currently forces rebuilding with recently changed headers. Fix incomplete Makefile header dependencies so ordinary `make` rebuilds when required. Use `-iquote ../../src` rather than `-I../../src` to avoid collisions between KA9Q `src/sched.h` and system `sched.h`.

## Remaining work / TODO

### Phase A — guarded clock reconfiguration (hardware-control milestone)

- [x] Enumerate/claim RX888 without disturbing another running application.
- [x] Read current Si5351 and R828D state.
- [x] Verify a single same-value Si5351 register write and readback.
- [x] Decode both live and generated PLL/MultiSynth plans.
- [x] Unit/integration-test snapshot, programming, restore, lock polling, and failure injection.
- [ ] Harden `clock-test` live preconditions: compare against known-good baseline, check initial Si5351 status and output-enable state, and report all steps distinctly.
- [ ] Review I²C write semantics with the currently loaded `sddc_vhf_test` firmware; a version `2.2` response alone is not sufficient evidence of compatibility with every control operation.
- [ ] Review rollback limitations, including loss of USB/I²C communication, unexpectedly re-enabled outputs, and the effect of changing PLLs on other outputs; do not imply the simulated tests guarantee physical recovery.
- [ ] Conduct one controlled live reconfiguration/restore experiment only after preflight review. Compare restored registers and status with captured baseline; separately establish whether output frequency needs external measurement.
- [ ] Improve Makefile dependencies and add a reliable command to run all standalone tests.

### Phase B — R828D tuning

- [ ] Integrate existing mocked initialization and gain-control routines into the gated FX3/I²C hardware path.
- [ ] Validate firmware-specific R828D register write/read conventions and tuner-PLL lock polling with physical hardware.
- [ ] Tune 161.975 MHz AIS-A, then 162.025 MHz AIS-B with a repeatable RF/IF gain baseline.
- [ ] Log frequency settings, lock state, USB/I²C errors, and output metadata.

### Phase C — ADC / FX3 USB IQ acquisition (major unknown)

- [ ] Reuse or port the proven SDDC FX3 streaming/control path; establish appropriate ADC/FX3 init without destabilizing the Nooelec session.
- [ ] Verify continuous USB transfers and unpack the exact sample format, polarity, rate and gain settings.
- [ ] Capture short standalone IQ test files; measure sample continuity, dropped blocks, effective sample clock, spectral content and signal strength.
- [ ] Validate first AIS decode from IQ acquired by the standalone RX888 process.

### Phase D — KA9Q integration and VHF validation

- [ ] Feed RX888 samples into KA9Q's real-time DSP/channelizer and multicast outputs.
- [ ] Verify simultaneous AIS-A/B decoding and marine voice CH12/CH16 reception.
- [ ] Validate channel frequencies, oscillator offset, dynamic tuning, gain control, sample-drop metrics and startup/shutdown/restart behaviour.
- [ ] Compare against independent Nooelec and previously captured SDDC/AIS data using similar conditions; do not interpret decoder-count changes alone as RF improvement.
- [ ] Add configuration examples, operations guide, hardware regression procedures, failure recovery, and relevant tests.

## Repeatable commands

```bash
cd ~/tools/ka9q-radio-vhf-standalone
git status --short
git log -1 --oneline
make -B -C tools/rx888-vhf-standalone

# Check competing applications and current USB assignment first
python3 ~/tools/fm-measure/scripts/ais/ais-session.py status
./build/rx888-vhf-standalone list

# Read-only commands (substitute actual current bus/address)
./build/rx888-vhf-standalone probe --bus 2 --address 9 --exclusive --read-i2c
./build/rx888-vhf-standalone inspect-clocks --bus 2 --address 9 --exclusive
./build/rx888-vhf-standalone compare-clocks --bus 2 --address 9 --exclusive --ppm 0

# Offline integrated clock test
gcc -std=gnu11 -D_GNU_SOURCE=1 -O2 -Wall -Wextra -Werror \
  -iquote src tests/test_rx888_vhf_clock_integrated.c src/si5351.c -lm \
  -o build/standalone-tests/test_rx888_vhf_clock_integrated
./build/standalone-tests/test_rx888_vhf_clock_integrated
```

**Not cleared for unattended/live use:** `clock-test --write-clocks`. The `echo-reg3 --write-reg3` experiment has already succeeded once, but unnecessary repeated live writes are discouraged.

## Resumption protocol / next decision

1. Check branch revision, dirty files, AIS session state, actual USB bus/address, firmware response, read-only Si5351 snapshot.
2. Fix pre-programming guard and error-reporting gaps, and assess whether the current FX3 firmware supports all the intended write commands.
3. Run offline regressions and targeted failure injection before any disruptive live reconfiguration.
4. Perform a deliberate and supervised clock reconfiguration only if recovery steps and acceptance criteria are clear.
5. Move promptly toward **first standalone RX888 IQ capture and real AIS decode**, rather than indefinitely adding isolated clock tests.

**Safety / interpretation:** A PASS in mocked tests proves only the software scenarios covered. A register readback or healthy PLL status is not an independent physical-clock measurement. A failed USB/I²C restoration can leave hardware in an unknown state; stop testing and assess recovery rather than repeatedly retrying.
