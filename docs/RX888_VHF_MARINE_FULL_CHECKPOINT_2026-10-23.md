# RX888 MkII / KA9Q VHF Marine — comprehensive project checkpoint

**Date:** Friday 23 October 2026 (Australia/Brisbane, AEST UTC+10)  
**Operator:** VK4TMZ  
**Host with RX888 hardware:** `devlap200`; workstation/listener: `devpc001`  
**Decision status:** **PHASED INTEGRATION; upstream main merge explicitly PARKED**. First compile and fix our marine branch, then establish reliable simultaneous maritime voice **and** AIS, and only then consider upstream main updates.  
**Checkpoint provenance:** Reconciled with `fm-measure` experiment notes and archive analysis, standalone KA9Q GitHub source, the marine integration branch, and the operator's reported successful overnight launch. No new hardware session or compiler execution was performed as part of writing this checkpoint.

## 1. Executive state and non-negotiable decisions

1. **Proven:** `feature/rx888-vhf-standalone-test` now receives real AIS-A/B with the KA9Q **W1EUJ firmware 2.3**, including after a genuine USB cold boot. Root cause of the apparent cold-start reception fault was **one bit** in R828D register `R0C` (IF VGA manual/automatic mode). Fixed VGA `R0C=E8` with `VGA_MODE=0`.
2. **Proven:** the 21-profile R828D analogue bandwidth sweep runs with successful decodes in *every profile* and no reported USB failures. Original and repeat sweeps are **not controlled concurrent traffic tests**, nor firmware-identical.
3. **Proven:** the incorrect ADC peak calculation in the *standalone diagnostic report* was corrected and covered by an offline regression test; operator reported the rebuilt overnight receiver and AIS dashboard running.
4. **Development only:** changes are now committed directly to `feature/rx888-vhf-sync-main` for the normal KA9Q `radiod` driver. This branch **has not yet passed an independently observed full radiod/rx888.so build, live VHF marine voice+AIS test or hardware-failure/recovery acceptance**. Do not label the integrated driver production-ready on the basis of standalone success.
5. **Explicit scope:** no standalone capture CLI, CF32 recorder/sink, FIFO-based AIS producer, ADC statistics, experimental firmware-tuner calibration or hardware RF AGC gets copied into the marine `radiod` integration. Keep the `ka9q-ais` project and independent SDDC test utility outside the changes in this branch.
6. **Hardware ownership:** one owner of the RX888 USB device at a time. Preserve ongoing standalone KA9Q AIS reception and separate NooElec comparison until a user-approved maintenance switchover.
7. **Merge policy:** do **not** merge upstream `ka9q/ka9q-radio:main` now. Validate our own source, listening audio, AIS paths and long-duration stability first. Only then reconcile upstream DSP/filter/ADC changes in a separate phase.

## 2. Repositories, host paths, firmware ownership

| Purpose | Repository / branch | Typical devlap200 path | State |
|---|---|---|---|
| Proven KA9Q hardware/standalone CLI | `vk4tmz/ka9q-radio-forked` / `feature/rx888-vhf-standalone-test` | `~/tools/ka9q-radio-vhf-standalone` | Live AIS proven; do not merge wholesale into radiod |
| Marine `radiod` development | `vk4tmz/ka9q-radio-forked` / `feature/rx888-vhf-sync-main` | `~/tools/ka9q-radio-vhf` or separate clean worktree | First implementation committed; offline build pending |
| Later VHF integration reference | same repo / `feature/rx888-vhf-agc-upstream-integration` | GitHub reference only | Reference for recovery and gain safety; unvalidated AGC stays disabled |
| Experiment manager, sweep logs, archive analyses | `vk4tmz/fm-measure` / `feature/fm008-cf32-input` | `~/tools/fm-measure` | `scripts/ais/ais-session.py` drives overnight `ka9q_rx888` |
| Original upstream tuner effort | `ka9q/ka9q-radio` / `rx888-vhf` | upstream GitHub | Original Phil-derived VHF driver |
| Main upstream | `ka9q/ka9q-radio` / `main` | upstream GitHub | Later 57 commits deferred |
| Original KA9Q fork main | `vk4tmz/ka9q-radio-forked` / `main` | GitHub | Behind our integration branch, not the main to merge now |
| Separate SDR/decoder | independent SDDC `rx888` and RTL-SDR `nooelec` | local tools / AIS-Catcher | Maintain firmware/device separation |

**Branch-name clarification:** `feature/rx888-vhf-marine` was recalled from the earliest VHF work, but was **not present as a published branch** on the fork at review. It may still exist locally. Published `feature/rx888-vhf-sync-main` is the branch selected for the new work.

**Firmware separation:**

- `ka9q_rx888` = KA9Q producer with **W1EUJ_0.1.0_FX3.img**, SHA256 `f1c682293c5cb1714b75e8b8cfad0e6cfe86b5f83b987082d425404dc56e4a06`, hardware `0x04`, reported FX3 firmware **2.3**.
- `rx888` = independently managed SDDC test producer/FX3 image; firmware **2.2** on the comparison tests. The SDDC firmware SHA in the prior cold-start study was `824c575fd00ef0faa0c099ee013c6b8c19ec735bd1e555651c49273fa6b30ff2`. Do not label an SDDC firmware test as a W1EUJ acceptance test.
- `nooelec` = separate RTL-SDR receiver and AIS-Catcher, dashboard port **8101**, kept running during tests when reported.
- USB `04b4:00f3` is FX3 bootloader/unprogrammed; `04b4:00f1` is programmed. A firmware **path argument alone cannot identify an already-programmed device**; `--reuse-loaded-firmware` is an explicit opt-in only when the existing loaded W1EUJ identity was verified and no intervening reflash occurred.

## 3. Context: original VHF marine receiver and standalone recovery

- Original goal: single RX888 MkII via KA9Q `radiod` receiving maritime voice (Brisbane VTS CH12, distress/calling CH16, additional marine channels) **and both AIS channels** in the same VHF front-end.
- Earlier Phil-derived VHF code produced clearly heard Brisbane VTS CH12/CH16 audio; AIS channels also decoded in early experiments. Subsequent comparison of Phil `rx888-vhf` and KA9Q main revealed conflicts in files including `src/filter.c`, `src/rx888.c`, `src/.gitignore` and Debian changelog.
- Existing integration branch also carries the prior FM squelch fix: **explicit zero** is an always-open request; do not regress it by copying an older `fm.c`.
- Independent standalone CLI work began to isolate tuner/ADC/IF and feed AIS-Catcher directly, with 20 MS/s real signed samples translated to 400 kS/s complex CF32. Original wide 8 MHz three-register experiment changed `R0A/R0B/R1E`; at a fixed extracted IF it caused approximately **32.9 dB** reduction in CF32 RMS, largely attributable to `R0A`, and led to no usable AIS in short capture windows. That was *not* proof that 8 MHz is universally faulty: filter position, initialization and output IF alignment were not controlled.
- Standalone FM broadcast (e.g. clear 92.7 MHz MixFM) subsequently worked. A working AIS extraction used LO near **163.570 MHz** and digital IF around **1.570 MHz** to form a complex stream centred near 162 MHz containing AIS-A/B at ±25 kHz.
- Initial saved-IQ and ADC diagnostics were added without contaminating CF32 stdout; 60 s 37.2 dB/IF16.3 tests had ~100 AIS decodes and no ADC rail hits. Higher RF 43.9 dB showed larger decoded signalpower but not a defensible sensitivity or analogue overload improvement. Preferred provisional gains held at **37.2 dB RF and 16.3 dB IF**.

## 4. Friday 23 October — the one-bit R828D defect

### Symptom and narrowing of the fault

- Original 21-profile sweep at approximately **15:00–15:20 AEST** was healthy, including **77 AIS messages in 60 seconds at 290 kHz**.
- A subsequent nominally comparable `ka9q_rx888` 290 kHz run (`20261023T070531Z_rf_37.2_if_16.3`) produced **only 2 messages in 60 s**, despite 37.2 dB RF/16.3 dB IF, the nominal **1.925 MHz** IF/`163.925 MHz` LO, 16 MHz tuner reference and zero reported USB failures. Previously observed CF32 sample power after replug was ~9–14 dB below earlier captures; alone that was not a diagnosis.
- R828D calibration experiments were investigated but **not** used in the final fix. Avoid turning on unvalidated 8 MHz calibration, firmware-native `TUNERINIT`, native gain or SDDC-input flags to explain this already-solved problem.

### Root cause and exact correction

| R828D operation | Incorrect KA9Q path | Correct fixed-VGA path |
|---|---|---|
| Initial 27-register table, `R0C` | `F0` | **`EB`** |
| R828D VGA mode bit | `R0C[4]=1` (automatic/unintended mode) | **`R0C[4]=0` (fixed/manual VGA)** |
| Mask used by IF-gain code | `0x0F` (leaves mode bit unchanged) | **`0x1F`** (clears bit 4, sets code 0..15) |
| IF gain request `16.3 dB` = index 8 | `F8` after bad mask | **`E8`** |

SDDC initializes `R0C` using `E0 | DEFAULT_IF_VGA_VAL` with code 11, i.e. `EB`. The fix changes the KA9Q startup value to `EB` and the three host IF-gain update paths to the five-bit mask `0x1F`; logs must show `R0C=e8 VGA_MODE=0` with IF index 8.

The issue was an **actual RF analogue gain-mode defect**, not an AIS-Catcher message-type issue, sample-rate problem, USB transfer fault or a need for aggressive filter calibration.

### Reproducibility and true cold-boot acceptance

| Test | Firmware actually loaded | AIS / ~60 s | Successful USB transfers | Failed transfers | Interpretation |
|---|---|---:|---:|---:|---|
| Corrected host before USB replug (`20261023T091516Z`) | Existing image at that time | 61 | 4,533 | 0 | Good receive path before reset |
| USB replug, explicitly uploaded independent SDDC (`20261023T091844Z`) | SDDC FX3 **2.2** | 60 | 4,558 | 0 | Corrected KA9Q host + SDDC firmware; **not** W1EUJ acceptance |
| **Definitive USB bootloader reload** (`20261023T094158Z`) | **KA9Q W1EUJ 2.3**, pinned SHA above | **73** | **4,552** | **0** | **PASS:** correct KA9Q production firmware/host pairing |

The definitive 60-second W1EUJ run: `rx888 hardware 0x04, firmware 2.3`; 20 MS/s acquisition, 400 kS/s complex output; RF 37.2/IF 16.3 dB, analogue BW 290 kHz, IF 1.925 MHz, ~163,924,987.793 Hz LO, +16.2 ppm, AIS-Catcher model 4; R0A=DF/R0B=E7/R1E=0E **software shadow**, `R0C=E8`. Approximately 1,193,279,488 input samples (~59.664 s) and 23,865,582 complex output samples. Producer and decoder exited normally. `nooelec` remained running.

**Session-manager correctness hardening (`fm-measure`):** `ais-session.py` now separates SDDC vs KA9Q profiles, validates the intended KA9Q W1EUJ image filename/hash, refuses ambiguous already-loaded FX3 firmware by default, and supports deliberate `--reuse-loaded-firmware` after verified boot. Five firmware-profile regression tests passed at the time.

Representative `fm-measure` commits: `9c0272f` firmware selection, `bcecc26` preloaded-firmware ambiguity protection, `5ff3a26` regression tests and `d7e414e` final cold-start documentation. Source: `docs/rx888-narrow-cold-start-2026-10-23.md`.

## 5. Both complete R828D bandwidth sweeps (21 profiles each)

### Conditions and integrity

- **Original:** ~14:59–15:20 AEST, original archive `ka9q-bandwidth-sweep-review.tar.gz`, output folder `~/tools/ais-gain-runs/ka9q-bandwidth-sweep/20261023T045926Z`, **firmware 2.2** and pre-mode correction.
- **Restored:** ~19:51–20:12 AEST, archive `ka9q-bandwidth-sweep-review.tar(2).gz`, **firmware 2.3 W1EUJ** and corrected `R0C=E8 VGA_MODE=0` in all 21 profiles.
- Both: 60-second requested/profile, 20 MS/s signed input, 400 kS/s CF32, RF gain **37.2**, IF gain **16.3**, oscillator correction **+16.2 ppm**, AIS-Catcher model **4**. Producer and decoder exit status **0**, **zero USB failures** for each profile.
- The repeat re-used already-loaded W1EUJ 2.3 firmware, so `firmware_identity.json` = JSON null (identity path cannot be proven from that run file alone); immediately preceding genuine W1EUJ USB cold boot and producer firmware 2.3 banners establish the intended sequence.
- 21×60 s is ~21 minutes of capture; sweeps were about **five hours apart**, not parallel, and firmware changed. They cannot be interpreted as a controlled receiver sensitivity A/B test.

### Full 21-profile result matrix

A/B message count, per-profile unique MMSIs, decoder-reported median `signalpower` and signed ADC RMS are transcribed from both archived report reviews. **Sum of per-profile MMSIs double-counts vessels seen in multiple profiles.**

| BW MHz | Nominal IF MHz | AIS original | AIS restored | Message Δ % | MMSI orig→new | Median signalpower orig→new | ADC RMS orig→new (codes) |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 0.290 | 1.925 | 77 | 41 | -46.8% | 24 → 19 | -78.28 → -73.59 | 206.70 → 204.82 |
| 0.375 | 1.857 | 95 | 48 | -49.5% | 34 → 26 | -75.02 → -69.01 | 206.78 → 205.92 |
| 0.420 | 2.121 | 92 | 58 | -37.0% | 36 → 30 | -77.88 → -68.69 | 207.25 → 205.50 |
| 0.470 | 1.788 | 92 | 53 | -42.4% | 23 → 24 | -77.12 → -67.53 | 207.42 → 206.09 |
| 0.600 | 1.706 | 89 | 62 | -30.3% | 32 → 30 | -74.35 → -67.61 | 207.61 → 207.17 |
| 0.860 | 1.558 | 88 | 62 | -29.5% | 30 → 27 | -72.26 → -67.16 | 208.45 → 208.46 |
| 0.950 | 2.205 | 100 | 67 | -33.0% | 29 → 29 | -75.24 → -68.95 | 208.54 → 207.99 |
| 1.100 | 2.125 | 106 | 67 | -36.8% | 35 → 31 | -74.99 → -68.28 | 207.97 → 208.21 |
| 1.200 | 1.350 | 108 | 57 | -47.2% | 34 → 30 | -73.92 → -65.78 | 207.43 → 209.67 |
| 1.300 | 2.043 | 80 | 62 | -22.5% | 23 → 29 | -74.24 → -66.02 | 207.31 → 209.14 |
| 1.503 | 1.276 | 95 | 67 | -29.5% | 29 → 28 | -74.51 → -67.10 | 207.35 → 210.68 |
| 1.600 | 1.900 | 105 | 58 | -44.8% | 31 → 23 | -74.03 → -67.18 | 207.91 → 210.35 |
| 1.753 | 1.412 | 102 | 71 | -30.4% | 29 → 31 | -72.34 → -68.00 | 208.19 → 212.42 |
| 1.800 | 1.400 | 97 | 68 | -29.9% | 32 → 29 | -73.65 → -65.28 | 208.50 → 212.99 |
| 1.953 | 1.530 | 97 | 74 | -23.7% | 34 → 29 | -72.15 → -66.07 | 208.92 → 214.23 |
| 2.200 | 1.600 | 88 | 80 | -9.1% | 27 → 33 | -71.95 → -65.99 | 208.96 → 214.56 |
| 3.000 | 2.000 | 94 | 76 | -19.1% | 31 → 30 | -73.97 → -65.99 | 209.19 → 216.09 |
| 5.000 | 3.570 | 92 | 51 | -44.6% | 34 → 20 | -74.16 → -66.43 | 225.92 → 280.25 |
| 6.000 | 3.570 | 105 | 51 | -51.4% | 35 → 23 | -75.83 → -64.91 | 228.99 → 321.85 |
| 7.000 | 4.570 | 100 | 69 | -31.0% | 33 → 31 | -75.23 → -66.10 | 229.61 → 359.39 |
| 8.000 | 4.570 | 96 | 48 | -50.0% | 34 → 23 | -75.13 → -66.45 | 250.78 → 309.20 |
| **ALL 21** | — | **1,998** | **1,290** | **−35.4%** | **649 → 575**¹ | **−74.58 → −67.24**² | — |

¹ Sums of per-profile MMSI counts, **not full-sweep vessel totals**. Full-sweep distinct MMSIs: **67 → 73**.  
² Mean of the 21 per-profile median decoded-packet readings; a **+7.34** average difference in decoder-reported units, not calibrated RF power, SNR or gain.

### Strongest comparative observations and limits

- Original bandwidth with most messages: **1.2 MHz, 108 AIS**; repeat: **2.2 MHz, 80 AIS**. Do **not** change production bandwidth solely on this rank.
- Total decodes **1,998 → 1,290 (−708; −35.4%)**. Traffic population, transmitter timing, time of day and firmware differ; **not evidence alone of a receiver regression**.
- Full-sweep distinct MMSIs **67 original vs 73 restored**. Of **58 vessels present in both sweeps**, **57/58** had higher per-vessel median `signalpower` in the restored sweep; median per-vessel difference **+10.43 decoder units**. When matching **both MMSI and bandwidth**, **244/248** pairs rose (median pair difference **+9.97**). This strongly suggests a systematic reported receive-level difference, but neither firmware nor time of observation was held constant.
- Decoder median `signalpower` increased for **all 21** profiles; average **+7.34**, range **+4.34 to +10.92** in the decoder's reporting units. It is not independent RF SNR or calibrated sensitivity.
- Absolute AIS frequency-offset median `|ppm|` estimate **0.578704** in both sweeps (quantized decoder result). No meaningful drift inference from these coarse summary figures.
- NooElec was reportedly still running, but **time-matched NooElec counts are absent from these comparison archives**; no valid normalization against independent vessel traffic is possible.

## 6. ADC quality / clipping analysis and later diagnostic defect

### Digital ADC sample statistics: original versus restored 21-profile sweeps

| Statistic | Original | Restored |
|---|---:|---:|
| Raw ADC samples accounted for | **25,122,832,384** | **25,129,385,984** |
| Samples at `+32767` or `−32768` | **0** | **0** |
| Samples within 1% of signed-s16 rails | **0** | **0** |
| Samples within 5% of signed-s16 rails | **0** | **0** |
| USB transfer failures | **0** | **0** |
| Overall signed ADC extrema | min **−2649**, max **+2065** | min **−5335**, max **+5766** |
| Example 290 kHz ADC RMS (codes) | **206.700** | **204.816** |
| Example 7 MHz ADC RMS (codes) | **229.61** | **359.39** |

Largest restored recorded peak: **5766/32768 = ~17.6%** of numerical signed-s16 full scale (**−15.09 dBFS**, 6 MHz profile). At 290 kHz the RMS was near unchanged; wider analogue IF profiles admitted higher broadband sample energy, which is **not** the same measurement as narrower-channel noise or AIS SNR. The absence of ADC rail hits rules out digital *sample-code* clipping during those windows; it **does not rule out earlier analogue compression or overload inside R828D**.

### Separate software-only peak reporting bug

- In `tools/ka9q-rx888-vhf-cli/capture_host.c`, `adc_quality_report()` cast a **negative maximum signed 16-bit sample** to `unsigned` when determining the absolute peak. For runs where all samples were negative, it fabricated impossible `peak_dbfs=+102.35`.
- Corrected using `src`-independent standalone `tools/ka9q-rx888-vhf-cli/adc_peak.h`: promote samples to wider signed integers, absolute-value both min/max, then compare. Handles `INT16_MIN = −32768` safely.
- `tools/ka9q-rx888-vhf-cli/test_adc_peak.c` covers negative-only and full-rail cases; test added to its Makefile. Fix commit **`48856d42`** on `feature/rx888-vhf-standalone-test` (preceding small commits add test/helper and implementation).
- Corrected peaks reconstructed from original logged extrema: **290 kHz −41.12 dBFS; 600 kHz −39.18 dBFS; 1.2 MHz −39.30 dBFS**. This defect affects *reporting only*, not IQ samples or the valid clipping counters. It is **excluded** from the marine radiod branch because it belongs to the standalone capture CLI.
- The operator subsequently ran the pull, RX888 build/standalone regression and overnight start command, reporting **“up and running”**. Full terminal output was not independently uploaded in this chat; record this as an operator-confirmed integration smoke check, **not** a new hardware validation of the marine `radiod` branch.

## 7. Overnight proven standalone AIS setup — preserve

**Last user-confirmed state:** running on devlap200, AIS decode apparently on par with the current antenna. No subsequent runtime check was made while producing this checkpoint.

| Control | Setting |
|---|---|
| Session | `python3 scripts/ais/ais-session.py start ka9q_rx888` |
| RF gain / fixed IF gain | `--rf 37.2 --if-gain 16.3` |
| Calibration | `--rx-ppm 16.2` |
| Analogue profile | `--if-bandwidth 290000` (nominal IF 1.925 MHz; LO around 163.925 MHz for RF 162.000 MHz) |
| Decoder | `--model 4`, AIS-A 161.975 and AIS-B 162.025 MHz |
| Duration | `--seconds 0` = continuous |
| Diagnostics | `--tuner-diagnostics` |
| Web | `--web-port 8100`; independent NooElec port **8101** |
| Firmware | Verified W1EUJ 2.3; reuse only with `--reuse-loaded-firmware` on a known loaded image |
| IQ storage | **No CF32 persisted**; only metadata and receiver/decoder logs |

Manager logs and state under `~/tools/ais-gain-runs/ka9q_rx888` and `~/tools/ais-gain-runs/state/ka9q_rx888.json` respectively. ADC summary is emitted on a clean capture stop; absence of a periodic ADC summary during indefinite operation does **not** mean diagnostics are disabled. Stop through the manager rather than killing all AIS-Catcher or NooElec processes.

Typical non-disruptive status command (when user wants to check):
```bash
cd ~/tools/fm-measure
python3 scripts/ais/ais-session.py status
```

## 8. Marine radiod branch — work completed today (no standalone CLI)

**Target Git branch:** `vk4tmz/ka9q-radio-forked:feature/rx888-vhf-sync-main`.  
**Latest checkpoint commit before this document:** `a42824480a2b67d505b81be158a4122abc8d4452`.  
**Original fork main:** `5e786cc75281d4a5e8ad8c5c63bdafcb5bcd016c`, no new fork-main commits missing from our marine branch at comparison.

### Actual source implementation

- `src/rx888.c` now recognizes optional `vhf-rf-gain`, `vhf-if-gain`, `vhf-bandwidth-mhz`, `vhf-auto-gain` on VHF route.
- R828D init `R0C: F0 → EB` and fixed IF gain five-bit mask `0x1F` for startup and runtime changes. Expected IF index 8 yields shadow `E8` and `VGA_MODE=0`.
- Imported **29 real RF LNA/mixer nominal gain steps** and **16 fixed IF VGA nominal steps**, with nearest-step selection and successful-register-write-only metadata update. HF gain path intentionally not replaced.
- Imported **21 analogue IF-width and nominal-IF profiles** from standalone research into plain source headers; masked R0A/R0B/R1E writes preserve important unrelated bits.
- VHF-only default **8.000 MHz** if bandwidth omitted; unsupported values explicitly rejected; `vhf-auto-gain=true` explicitly rejected pending hardware validation. Zero/omitted `frequency` stays on conventional HF path.
- `frequency` is the **actual tuner LO request**, not RF centre; driver prints derived RF centre and nominal span from **RF centre = LO − selected nominal IF**. Changing width **does not automatically retune LO**.
- I²C short reads and failed writes no longer rely on assert to claim success; shadow is committed only after successful write. Startup/CLK2 lock, tuner init, PLL-write/lock path are partially failure-gated.
- **Known open concern:** full failure unwind/cleanup and multi-channel frequency mapping are not proven. Setup can potentially leave hardware partly configured on failure. Do not treat “in source” as passing live radiod acceptance.

**New non-CLI files:**

- `src/rx888_vhf_gain.h` (RF/IF tables).
- `src/r828d_bandwidth_profiles.h` (21 width/IF mappings, register masks).
- `src/rx888_vhf_vga_mode.h` (init byte/0x1F mask).
- `tests/test_rx888_vhf_gain.c`, `tests/test_r828d_bandwidth_profiles.c` and `tests/test_rx888_vhf_marine_config.c` (offline tests).
- `config/examples/radiod@ka9q-vhf-marine.conf.d/01-rx888.conf` (comprehensively commented example; **not a complete running service config**).
- `.github/workflows/rx888-vhf-marine-offline.yml` (RX888-only `radiod`/`rx888.so` build + tests; workflow configured, **pass status not confirmed**).
- `docs/RX888_VHF_MARINE_INTEGRATION_2026-10-23.md` (first integration checkpoint).

**Explicitly excluded:** tools/standalone CLI, CF32 sink, AIS-Catcher supervision, SDDC firmware test hooks, tuner TUNERINIT native mode, calibration experiments, automatic gain and new ADC reporting logic.

### Agreed VHF marine settings

```ini
[rx888]
device = rx888
samprate = 20m
undersample = 1
reference = 27000437.4  # 27 MHz +16.2 ppm example
frequency = 163570000  # MHz 163.570 LO, NOT RF centre
vhf-rf-gain = 37.2
vhf-if-gain = 16.3
vhf-auto-gain = false
vhf-bandwidth-mhz = 8.0
```

**Bandwidth 8 MHz nominal IF = 4.570 MHz.** With LO = 163.570 MHz, RF centre = **159.000 MHz**, nominal span ≈**155–163 MHz**. Marine channels **CH12 156.600, CH16 156.800, AIS-A 161.975, AIS-B 162.025 MHz** are nominally within this span. This is based on R828D band/profile bookkeeping, **not a verified flat RF passband or validated actual radiod channel placement**.

**Bandwidth versus 10 MHz advertising:** at 20 MS/s real sampling, the theoretical Nyquist half-range is 10 MHz. The widest *standard R828D analogue filter preset* in our profile table is 8 MHz. We choose 8 MHz for the whole marine allocation; the 10 MHz sampling limit is not proof of a flat/usable 10 MHz analogue passband. Narrow bandwidths (290 kHz etc.) suit AIS-only, not the simultaneous 156–162 MHz voice/AIS goal.

**Gain steps and full bandwidth/IF pairs:** see the commented config. Values transcribed from source:

RF (29): 0.0, 0.9, 1.4, 2.7, 3.7, 7.7, 8.7, 12.5, 14.4, 15.7, 16.6, 19.7, 20.7, 22.9, 25.4, 28.0, 29.7, 32.8, 33.8, 36.4, 37.2, 38.6, 40.2, 42.1, 43.4, 43.9, 44.5, 48.0, 49.6 dB.  
IF (16): -4.7, -2.1, 0.5, 3.5, 7.7, 11.2, 13.6, 14.9, 16.3, 19.5, 23.1, 26.5, 30.0, 33.7, 37.2, 40.8 dB.

Bandwidth MHz → nominal IF MHz mapping is in the full 21-profile table above. To hold RF centre at 159 MHz, use LO = **159 MHz + that profile’s nominal IF**. Other explicit LO/channel choices must observe the same relationship.

## 9. Latest Git ancestry review — and why we park upstream merge

Review on 23 Oct 2026:

- Comparing **fork** `vk4tmz/ka9q-radio-forked:main` to `feature/rx888-vhf-sync-main`: **0 commits behind fork main**, 394 ahead at comparison (this number is dominated by the inherited VHF integration history). No need to merge fork main.
- Comparing **KA9Q upstream** `ka9q/ka9q-radio:main` at `af940c5c9c912ae34cf5608bf836c0671aec75fe` to our branch: **upstream has 57 unique commits; our branch has 84 unique commits** since common ancestor `71832befa898a84d281c6a8b3692bc6715d64010`. Branches diverged; straight fast-forward impossible.
- Significant **upstream** changes: RX888 AVX-512 sample conversion, `cached` versus streaming memory stores (new default cached), optional impulse blanker (`blank`), FFT buffers/huge pages and futex wakeups, DSP vector kernels and revised channel mixing, FM SNR/command processing and shared build dependency restructuring.
- Upstream preserves explicit zero FM squelch threshold; **our existing zero-squelch fix must remain intact**. New upstream feature set **does not replace** the unique validated R828D R0C/VGA fix, gain steps, or 21-bandwidth/IF profiles.
- These upstream changes will affect `src/rx888.c`, `src/filter.c/h`, `src/radio.c/h`, `src/fm.c`, `src/Makefile` and cross-cutting math/timing utility files. Blind single-file cherry-picking or a large immediate merge risks ABI/build/runtime breakage and makes hardware debug harder.

**Agreed decision:** **PARK UPSTREAM MAIN MERGE**. Stabilize and fully prove *our own integrated marine radiod branch* first. When the receiver can simultaneously monitor clean maritime voice and valid AIS, and all tests pass, plan a separate, reviewed upstream-main integration with benchmark/rollback.

## 10. Phased plan — strict acceptance gates

### Phase A — static audit, offline compile and repair (NEXT; no USB ownership)

1. Verify latest GitHub/source branch SHA and isolated clean checkout/worktree; avoid switching any directory currently used by the running standalone receiver.
2. Run isolated C tests for RF gain table, 21 R828D bandwidth/IF profiles, LO/IF arithmetic, fixed manual VGA/index 8 → `R0C=E8`. Run test suite on devlap200 or CI and capture exact output.
3. **Build both** `radiod` and `rx888.so` with `ENABLE_RX888=1`, with unrelated frontends disabled. Resolve build errors and warnings. **No program installation/USB access/service restart**.
4. Source-audit `frontend->isreal`, `frontend->frequency`, tuner PLL LO and sign, `min_IF/max_IF` and the KA9Q FFT/channelizer/SSRC frequency equations. Prove the entire 156.600–162.025 MHz mapping before listening tests; verify filter centre at correct physical IF.
5. Harden failure unwind and HF/VHF transitions: abort and release cleanly on I²C/clock/PLL failure; do not mark frontend locked on a partial tune. Add fault-injection tests for short I²C reads, failed writes, PLL missing lock, and repeat retuning.
6. Preserve previous `fm.c` fix (zero squelch is always-open), HF unaffected when VHF config not selected, W1EUJ firmware ownership, and no standalone-CLI dependencies.
7. Do not call Phase A complete until **successful build logs and regression tests** are recorded.

Build/test reference (offline only; run in the selected marine working tree, not in the active standalone tree):

```bash
# Example: separate development checkout. DO NOT start radiod or touch USB.
cd ~/tools/ka9q-radio-vhf
git fetch origin
git switch feature/rx888-vhf-sync-main
git pull --ff-only

cc -std=c11 -Wall -Wextra -Werror -Isrc \
  tests/test_rx888_vhf_gain.c -lm -o /tmp/rx888-vhf-gain-test
/tmp/rx888-vhf-gain-test
cc -std=c11 -Wall -Wextra -Werror \
  tests/test_r828d_bandwidth_profiles.c -o /tmp/rx888-vhf-bw-test
/tmp/rx888-vhf-bw-test
cc -std=c11 -Wall -Wextra -Werror -Isrc \
  tests/test_rx888_vhf_marine_config.c -lm -o /tmp/rx888-vhf-marine-test
/tmp/rx888-vhf-marine-test

make -C src -j2 radiod rx888.so NATIVE=0 \
  ENABLE_RX888=1 ENABLE_AIRSPY=0 ENABLE_AIRSPYHF=0 \
  ENABLE_BLADERF=0 ENABLE_FOBOS=0 ENABLE_FUNCUBE=0 \
  ENABLE_HACKRF=0 ENABLE_HYDRASDR=0 ENABLE_RTLSDR=0 \
  ENABLE_SDRPLAY=0 ENABLE_SIG_GEN=0 ENABLE_SOAPY=0
```

### Phase B — offline marine configuration and channel/SSRC wiring

1. Add complete isolated `radiod` global receiver/status/data config, not just `[rx888]`. Reuse appropriate current control/status multicast interfaces and audio patterns; avoid overwriting live configs.
2. Explicit channel list includes **CH12 156.600**, **CH16 156.800**, **AIS-A 161.975** and **AIS-B 162.025 MHz** (plus later CH67/73/21/81 as desired).
3. Check symbol-rate/modulation/data output expected by AIS-Catcher/`ka9q-ais`; check multicast output format, sample rates, SSRC ranges and collision avoidance (known `ka9q-ais` production SSRCs **9998600/9998601**, test **7998700/7998701**). Do not modify `ka9q-ais` yet.
4. Offline validate configurations, logs/commands, FFT allocations and CPU budgets. Prepare rollback plan, unchanged known-good firmware and start scripts.

### Phase C — short, explicitly approved hardware acceptance

1. During a mutually agreed receiver window only: note the running standalone run directory, close it gracefully through `ais-session.py stop ka9q_rx888`; leave NooElec on 8101; verify RX888 sole ownership.
2. Start isolated marine `radiod` with documented W1EUJ 2.3 and 8 MHz profile; confirm R0C `E8`, RF 37.2/IF16.3, LO/IF mapping, 20 MS/s, USB failure zero, reference clock and PLL lock, correct status multicast.
3. **Voice acceptance:** listen to **actual intelligible Brisbane VTS** on CH12, CH16 where traffic exists, verify squelch zero/normal operation, no continuous self-generated hiss. Retest other channels and RF band edges.
4. **AIS acceptance:** get **genuine, valid AIS-A and AIS-B decodes**, track message types/channel counts/MMSIs, compare with independently running NooElec for same local window (allow antenna/system differences). Ensure voice and AIS streams are concurrently active on the same RX888 front-end without restart.
5. Monitor CPU/FFT scheduling, USB continuity, decoder and multicast loss, tuner/ADC behavior, long-running reliability.
6. On any serious failure, **roll back** to proven standalone `ka9q_rx888` and W1EUJ image; do not carry unrelated fixes into the experiment.
7. Only after Phase C acceptance, choose a stable baseline commit/config checkpoint.

### Phase D — upstream main reconciliation (PARKED)

**Do not start until Phase C is signed off.** Make a separate worktree/branch for upstream 57-commit reconciliation; examine `rx888.c` and shared DSP/FFTs together, integrate build graph and FM fixes; keep conditional/experimental optimization flags conservative, repeat Phase A and Phase C, compare performance and roll back if necessary.

## 11. Evidence, repos and checkpoint references

**In `fm-measure` branch `feature/fm008-cf32-input`:**

- `docs/rx888-narrow-cold-start-2026-10-23.md` — one-bit root cause, firmware ownership, W1EUJ 73-message acceptance evidence.
- `docs/ka9q-ais-bandwidth-repeat-2026-10-23.md` — exact 21-profile comparisons, decoder/ADC/clipping, peak defect and overnight settings.
- `scripts/ais/ais-session.py` — firmware-guarded standalone session manager, `start/status/stop`, port 8100, no raw CF32 in continuous mode.
- `scripts/ais/ka9q_bandwidth_sweep.py` — 21 × 60 s sweep with logs and aggregate CSV. `scripts/ais/compare_ka9q_bandwidth_sweeps.py` — aligned comparison; commit `1b44f06`.
- Archive evidence: `ka9q-bandwidth-sweep-review.tar.gz` (original), `ka9q-bandwidth-sweep-review.tar(2).gz` (repeat), and `ka9q_bandwidth_repeat_comparison_2026-10-23.csv` / `ka9q_bandwidth_repeat_review_2026-10-23.md`.

**In `ka9q-radio-forked` branch `feature/rx888-vhf-standalone-test`:**

- `src/r828d_init.h`, `src/rx888.c` and `tests/test_r828d_init.c`/`tests/test_r828d_io.c` — one-bit fixed VGA correction and tests.
- `tools/ka9q-rx888-vhf-cli/adc_peak.h` and `test_adc_peak.c` — corrected peak statistics and regression tests; commit `48856d42`.

**In `ka9q-radio-forked` branch `feature/rx888-vhf-sync-main`:**

- `src/rx888.c`, `src/rx888_vhf_gain.h`, `src/r828d_bandwidth_profiles.h`, `src/rx888_vhf_vga_mode.h`; three offline tests; commented marine configuration; offline workflow; initial integration checkpoint.
- These source changes have been committed but the complete integrated build, live voice+AIS and recovery path remain **unverified**.

### Source links

- Marine branch: https://github.com/vk4tmz/ka9q-radio-forked/tree/feature/rx888-vhf-sync-main
- Standalone branch: https://github.com/vk4tmz/ka9q-radio-forked/tree/feature/rx888-vhf-standalone-test
- FM/AIS tooling: https://github.com/vk4tmz/fm-measure/tree/feature/fm008-cf32-input
- Phil's upstream VHF: https://github.com/ka9q/ka9q-radio/tree/rx888-vhf
- Upstream main delta at reviewed refs: https://github.com/ka9q/ka9q-radio/compare/71832befa898a84d281c6a8b3692bc6715d64010...af940c5c9c912ae34cf5608bf836c0671aec75fe

## 12. Resume instructions — highest priority

**Resume Phase A only.** Do not start with upstream merge, tuner recalibration, R0C experiments, antenna/gain sweeps or USB replug. Keep the independent overnight `ka9q_rx888` and NooElec baseline running undisturbed. Obtain the actual offline `radiod` and `rx888.so` build/test output from the marine integration branch, fix whatever fails, and complete the RF/IF/channelizer sign mapping. Once a clean compiler and regression baseline exists, build the complete isolated marine config; **only then** schedule the controlled, reversible simultaneous voice-plus-AIS acceptance.

**Do not mistake today's proven standalone W1EUJ 2.3 AIS success for acceptance of the marine `radiod` pipeline.**
