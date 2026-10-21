#!/usr/bin/env python3
"""Offline RX888 AIS spectral evidence report. No USB, firmware or tuner writes."""
import argparse
import csv
import json
from pathlib import Path
import numpy as np

PARSER = argparse.ArgumentParser()
PARSER.add_argument("directory", type=Path, help="Existing ais-baseline-replay output directory")
PARSER.add_argument("--rate", type=float, default=384000)
args = PARSER.parse_args()
root = args.directory.resolve()
if not root.is_dir():
    PARSER.error(f"No directory: {root}")
N = 8192
step = N
win = np.hanning(N).astype(np.float64)
bins = np.fft.fftshift(np.fft.fftfreq(N, 1/args.rate))
results = []
for if_hz in (1602000, -1602000, 1620000, -1620000):
    file = root / f"if-{if_hz}.cf32"
    if not file.exists():
        raise SystemExit(f"Missing IQ file {file}; rerun replay only if necessary")
    iq = np.memmap(file, dtype="<c8", mode="r")
    blocks = len(iq)//N
    if not blocks:
        raise SystemExit(f"Insufficient IQ samples in {file}")
    # Per-block spectra expose intermittent AIS bursts rather than just average FFTs.
    powers = np.empty((blocks, N), dtype=np.float32)
    for i in range(blocks):
        x = np.asarray(iq[i*N:(i+1)*N], dtype=np.complex128)
        powers[i] = np.abs(np.fft.fftshift(np.fft.fft(x*win)))**2
    med = np.median(powers, axis=0)
    peak = np.percentile(powers, 99, axis=0)
    # Exclude near-DC artefacts from ranked spectrum features.
    valid = np.where(np.abs(bins)>10000)[0]
    ix = valid[np.argsort(peak[valid])[-12:][::-1]]
    features = [{"hz":round(float(bins[j]),1),"peak_db":round(float(10*np.log10(max(peak[j],1e-25))),1)} for j in ix]
    channels = []
    for center in (-25000,25000):
        band = np.abs(bins-center)<=12500
        guard = (np.abs(bins-center)>=18000)&(np.abs(bins-center)<=33000)
        bp = np.mean(powers[:,band],axis=1)
        noise = float(np.median(powers[:,guard]))
        channels.append({
            "offset_hz":center,
            "median_db_over_guard":round(float(10*np.log10(max(float(np.median(bp)),1e-25)/max(noise,1e-25))),2),
            "p99_db_over_guard":round(float(10*np.log10(max(float(np.percentile(bp,99)),1e-25)/max(noise,1e-25))),2),
            "max_db_over_guard":round(float(10*np.log10(max(float(np.max(bp)),1e-25)/max(noise,1e-25))),2)
        })
    results.append({"digital_if_hz":if_hz,"complex_samples":len(iq),"duration_seconds":round(len(iq)/args.rate,3),
                    "fft_bin_hz":round(args.rate/N,2),"blocks":blocks,"channel_bands":channels,
                    "strongest_fft_bins":features})
report = {"note":"Offline CF32 spectrum. Channel centres assume 162 MHz RF and correct IF mapping; peaks do not prove AIS decodes.",
          "analysis_rate_hz":args.rate,"results":results}
(root/"spectral-report.json").write_text(json.dumps(report,indent=2)+"\n")
with (root/"spectral-summary.csv").open("w",newline="") as f:
    w=csv.writer(f)
    w.writerow(["digital_if_hz","channel_offset_hz","median_db_vs_guard","p99_db_vs_guard","max_db_vs_guard","duration_s"])
    for r in results:
        for c in r["channel_bands"]:
            w.writerow([r["digital_if_hz"],c["offset_hz"],c["median_db_over_guard"],c["p99_db_over_guard"],c["max_db_over_guard"],r["duration_seconds"]])
print((root/"spectral-summary.csv").read_text())
print("Spectral reports:",root/"spectral-report.json",root/"spectral-summary.csv")
