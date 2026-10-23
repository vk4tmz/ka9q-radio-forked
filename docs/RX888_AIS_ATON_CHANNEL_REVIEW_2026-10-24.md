# AIS overnight 24 October 2026 — source-level channel-orientation correction and AtoN diagnostics

**Analysis source:** `ais-overnight-20261023T183320Z.tar.gz`, exact matched observation Fri 23 Oct 20:35:33 to Sat 24 Oct 04:33:20 Australia/Brisbane; live KA9Q and independently running NooElec AIS-Catcher snapshots. Do not confuse a decoded-message gap with proven USB stall without transfer metrics. This report does not change an active receiver.

## Decisions / scope

1. Work on **`feature/rx888-vhf-standalone-test` first**. Do not change `feature/rx888-vhf-sync-main` until controlled AIS orientation validation succeeds. Upstream main merge remains parked.
2. Preserve overnight continuous `ka9q_rx888` on W1EUJ 2.3 + 290 kHz analogue IF, fixed RF37.2/IF16.3, 16.2 ppm, AIS model 4 port 8100, and independent NooElec port 8101. No raw CF32 file is stored.
3. Fix channel *labeling/order* separately from reduced AtoN/channel reception. **Conjugation flips the frequency axis but cannot restore an RF packet never decoded.**

## Exact KA9Q timing

The KA9Q overnight session **began 20:35:33 AEST**, according to `ka9q_rx888/session-state.json` (`20261023T103533Z`); its supervision remained RUNNING at 04:33:20 AEST the next morning. The low-reception cluster started around **21:45:33 AEST**, **70 minutes after startup**. Individual gaps included 91.72 seconds from 21:45:33 and 208.94 seconds from 21:47:27, with NooElec decoding 66 and 114 messages respectively in those gaps. Additional 66.11 s and 56.92 s gaps began 21:52:48 and 22:03:58. No conclusive cause is present in the live log snapshot (ADC/USB totals reported at capture end).

Between **21:45 and 22:05 AEST**, KA9Q decoded **426 messages (12 Type 21, 414 other types, 62 MMSIs)** while NooElec decoded **722 (67 Type 21, 655 other, 53 MMSIs)**. Therefore this *temporary interruption* affected more than AtoNs; it is separate from the throughout-the-night Type 21 channel imbalance. NooElec had 67 Type 21 messages from all ten active virtual AtoNs in that 20-minute span; KA9Q had only 12 from nine.

## Full matched-window summary

- KA9Q: **23,665 decoded AIS messages, 88 distinct MMSIs**; NooElec: **18,416, 72 MMSIs**; 71 shared MMSIs, 17 KA9Q-only, 1 NooElec-only.
- **16,225 identical normalized AIS NMEA payloads** matched within 1 second; **16,220 (99.969%)** received opposite A/B channel labels; only 5 labels agreed. This is strong direct evidence of a systematic I/Q spectrum orientation difference, **not bad AIS payloads**.
- Type 21: KA9Q **816**, NooElec **1,594**. Ten **virtual** AtoNs are ~one complete RF channel missing on KA9Q over the entire night.

## Type 21: individual Brisbane AtoNs and PPM

`KA9Q B/A` and `Noo A/B` reflect their *current, inverted labels*. Values are **counts per reported channel in the matched 7 h 58 min window**. Median PPM values are AIS-Catcher's per-packet estimated carrier frequency errors **relative to each receiver's own frequency calibration**, not transmitter-frequency absolutes. Both devices' estimators are quantized and show occasional outliers.

| Virtual AtoN MMSI | AIS reported name | KA9Q B/A | NooElec A/B | KA9Q median PPM | NooElec median PPM |
|---|---|---:|---:|---:|---:|
| 995036070 | BNE-FLINDERS REEF | 76/1 | 79/80 | +0.289352 | −2.314815 |
| 995036071 | BNE-M1R STARBOARD | 78/1 | 80/78 | +0.289352 | −2.314815 |
| 995036072 | BNE NW2R SPEC MARK | 75/2 | 80/79 | +0.289352 | −2.314815 |
| 995036097 | BNE SWMARK 1.4'EB | 78/3 | 80/80 | +0.289352 | −2.314815 |
| 995036098 | BNE FAIRWAY | 76/4 | 79/80 | +0.289352 | −2.314815 |
| 995036099 | BNE M4-M6 PORT MARK | 78/2 | 79/80 | +0.289352 | −2.314815 |
| 995036114 | BNE F/W STBD ONE | 77/1 | 80/79 | +0.289352 | −2.314815 |
| 995036115 | BNE F/W STBD TWO | 79/2 | 80/79 | +0.289352 | −2.314815 |
| 995036116 | BNE F/W STBD THREE | 76/0 | 79/80 | +0.289352 | −2.314815 |
| 995036117 | BNE NW2 STBD | 76/4 | 79/80 | +0.289352 | −2.314815 |

**Example MMSI 995036097** (`BNE SWMARK 1.4'EB`):
- `virtual_aid=true`; KA9Q **78 B + 3 A = 81**, NooElec **80 A + 80 B = 160**.
- Median per-message PPM: **KA9Q +0.289352 ppm** (observed range −8.391 to +9.259); **NooElec −2.314815 ppm** (range −10.995 to +13.021). Outliers do not shift the medians, but do show why a single decoded packet's PPM is not definitive.
- **All 10 virtual AtoNs** in the table show exactly these same receiver-specific median PPM estimates, consistent with a common transmitter/source or a shared frequency reference. **This does not prove a common RF transmitter**: each receiver applies its own frequency calibration, the decoder estimates are quantized, and the physical transmitting site is not identified in AIS Type 21 alone.
- Separate exception: MMSI **995036160** `BNE BC17 SPECIAL` is flagged **`virtual_aid=false`**, so it is **not part of the common ten-virtual-aid pattern**. KA9Q 27 messages (20 A, 7 B), NooElec 4 messages (1 A, 3 B); median PPM KA9Q −0.289352 / NooElec −1.591435 (very small Noo sample).

## Why current AIS A/B labels are inverted

AIS physical RF channels: **A=161.975 MHz**, **B=162.025 MHz**. For our KA9Q receiver the tuner **LO is high-side** (163.925 MHz) and nominal IF=1.925 MHz:
- Physical RF A (161.975) becomes *real-ADC* **IF 1.950 MHz** (nominal IF +25 kHz).
- Physical RF B (162.025) becomes *real-ADC* **IF 1.900 MHz** (nominal IF −25 kHz).
- Existing `rx888_cf32_stream.c` and offline `rx888_cf32.c` translate 1.925 MHz down to complex DC, preserving **IF** frequency sign, so A appears at **+25 kHz** and B at **−25 kHz**, swapped relative to ordinary RF-centred CF32 whose AIS-A should be −25 kHz.
- Complex **conjugation** flips the sign of the spectrum. For this high-side tuning arrangement only, the expected output is RF **A −25 kHz, B +25 kHz**.
- This is a **digital phase/spectrum convention**, not an R828D register problem; no R0C changes are needed.

## Implemented on standalone branch only (opt-in, safe)

- `--invert-iq` is parsed **only with `--format cf32`**. The previous default remains unchanged.
- Added `rx888_cf32_stream_init_ex(...,bool invert_iq)` and `rx888_s16_to_cf32_ex(...,bool invert_iq,...)` for streaming and offline paths. Legacy APIs remain wrappers calling the new versions with `invert_iq=false`.
- With `--invert-iq`, conjugate complex output (negate Q/imaginary component), without changing amplitude, ADC, analogue BW, gains, firmware, USB handling, sample rate or AIS-Catcher settings.
- Added `test_cf32_orientation.c` and `test_cf32_offline_orientation.c`, generating 20 MS/s real tones at IF ±25 kHz around 1.925 MHz. Both tests assert legacy and inverted ±25 kHz output bins at 400 kS/s. Included in CLI Makefile `test` target alongside CLI argument regressions.
- A **local hardware-free C test using the equivalent stream algorithm** compiled with `-O2 -std=gnu11 -Wall -Wextra -Werror` and passed all four mode/tone cases:
  - Legacy A lower RF → +25 kHz; legacy B higher RF → −25 kHz.
  - Invert-IQ A lower RF → −25 kHz; invert-IQ B higher RF → +25 kHz.
- A separate hardware-free C test of the equivalent offline file-conversion algorithm also compiled with `-O2 -std=gnu11 -Wall -Wextra -Werror` and passed all four A/B and legacy/inverted cases.
- No live data was changed. **GitHub source checkouts, integrated capture-host build/unit tests on devlap200 and live AIS-Catcher validation are pending**; do not claim on-air fix until tests + controlled live comparison are completed.
- The standalone `--invert-iq` option does **not** automatically correct or alter normal `radiod`; its channelisation path is different and should be reviewed only after this source-level test is accepted.

## Remaining investigation and acceptance

1. On a separate safe clone/build directory or without restarting any service, pull the standalone branch and run `make -C tools/ka9q-rx888-vhf-cli test`; separately build `capture-host` (this does not open USB). Ensure existing users of legacy APIs still build and tests pass.
2. For a short **approved** receiver trial later, stop the continuous KA9Q session gracefully and start with `--invert-iq`, while leaving NooElec running. Compare identical payload NMEA matches, expected A/B labels, total decoded messages and Type 21 by physical channel. Restore old on-air configuration if the signal quality or decoder success degrades.
3. **Separate diagnosis:** the ~80/160 Type 21 discrepancy across ten virtual AtoNs will not be repaired by merely relabelling channels. Investigate 290 kHz analogue filter asymmetry, exact IF centre/frequency calibration, channel A/B discriminator performance, bias/LO mismatch and signal strength using *controlled data*. No sweeping gains/firmware changes justified by this snapshot.
4. For the 21:45–22:05 outage, collect time-bucket producer/USB/ADC health or decoded diagnostic snapshots before attributing cause. The current logs do not provide continuous USB failure counters.
5. Once standalone orientation + both-channel reception are *proven*, make only the justified mapping/orientation changes to `feature/rx888-vhf-sync-main` (the native `radiod` channeliser may need a different fix). **Do not** merge upstream main until normal marine VTS voice **and** simultaneous AIS have passed acceptance.

## References

- Snapshot `ais-overnight-20261023T183320Z.tar.gz`; companion review `ais_overnight_review_2026-10-24.md`.
- GitHub standalone branch: https://github.com/vk4tmz/ka9q-radio-forked/tree/feature/rx888-vhf-standalone-test
- Target marine branch, unchanged: https://github.com/vk4tmz/ka9q-radio-forked/tree/feature/rx888-vhf-sync-main
