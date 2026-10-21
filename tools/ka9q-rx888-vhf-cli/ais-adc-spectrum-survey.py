#!/usr/bin/env python3
"""Survey complete real ADC spectrum for RF energy and intermittent signals.

Offline only: no receiver operations. Produce bounded-size reports rather than
repackaging the raw ADC. FFT is on contiguous non-overlapping real samples.
"""
import argparse
import csv
import json
from pathlib import Path

import numpy as np

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--raw",type=Path,default=Path("/tmp/ka9q-ais-if-survey.s16"))
    p.add_argument("--rate",type=float,default=19_200_000)
    p.add_argument("--output",type=Path,default=Path("/tmp/ka9q-ais-full-spectrum"))
    p.add_argument("--fft",type=int,default=32768)
    p.add_argument("--max-blocks",type=int,default=2048)
    a=p.parse_args()
    if a.fft<1024 or a.fft&(a.fft-1) or a.rate<=0:
        p.error("FFT must be power of two >=1024 and sample rate positive")
    if not a.raw.is_file():
        p.error(f"Raw capture missing: {a.raw}")
    a.output.mkdir(parents=True,exist_ok=True)
    adc=np.memmap(a.raw,dtype="<i2",mode="r")
    available=len(adc)//a.fft
    blocks=min(available,a.max_blocks)
    if blocks<2:
        p.error("Insufficient ADC samples")
    # Uniformly sample blocks across full recording, not just its start.
    indices=np.linspace(0,available-1,blocks,dtype=np.int64)
    window=np.hanning(a.fft).astype(np.float32)
    freq=np.fft.rfftfreq(a.fft,1/a.rate)
    acc=np.zeros(len(freq),dtype=np.float64)
    max_power=np.zeros(len(freq),dtype=np.float64)
    selected=np.empty((blocks,len(freq)),dtype=np.float32)
    for i,j in enumerate(indices):
        x=adc[j*a.fft:(j+1)*a.fft].astype(np.float32)/32768.
        v=np.abs(np.fft.rfft(x*window))**2
        selected[i]=v
        acc+=v
        np.maximum(max_power,v,out=max_power)
    median=np.median(selected,axis=0)
    p99=np.percentile(selected,99,axis=0)
    mean=acc/blocks
    # local noise floor estimate from 100kHz-wide bins; avoid a single
    # dominant DC/IF spur skewing the whole-spectrum reference.
    group=max(3,round(100000/(a.rate/a.fft)))
    baseline=np.empty(len(freq),dtype=np.float64)
    for start in range(0,len(freq),group):
        stop=min(len(freq),start+group)
        baseline[start:stop]=np.median(median[start:stop])
    contrast=10*np.log10(np.maximum(p99,1e-25)/np.maximum(baseline,1e-25))
    limit=(freq>=100000)&(freq<=a.rate/2-100000)
    candidates=np.flatnonzero(limit)
    winners=candidates[np.argsort(contrast[candidates])[-40:][::-1]]
    peaks=[{"adc_frequency_hz":round(float(freq[k]),1),
            "p99_db_above_local_floor":round(float(contrast[k]),2),
            "mean_power":float(mean[k]),
            "max_power":float(max_power[k])} for k in winners]
    report={
        "input":str(a.raw),"rate_hz":a.rate,
        "duration_seconds":round(len(adc)/a.rate,3),"fft_size":a.fft,
        "bin_width_hz":a.rate/a.fft,"blocks_sampled":blocks,
        "note":"Real ADC yields mirrored positive/negative frequencies; peaks are not RF frequency identifications or confirmed AIS bursts.",
        "strongest_contrast_bins":peaks,
    }
    (a.output/"adc-spectrum-report.json").write_text(json.dumps(report,indent=2)+"\n")
    with (a.output/"adc-spectrum-bands.csv").open("w",newline="") as file:
        writer=csv.writer(file)
        writer.writerow(["start_hz","end_hz","mean_power","p99_to_local_floor_db","max_to_local_floor_db"])
        width=max(1,int(round(25000/(a.rate/a.fft))))
        for start in range(0,len(freq),width):
            stop=min(start+width,len(freq))
            floor=max(float(np.median(baseline[start:stop])),1e-25)
            writer.writerow([round(float(freq[start])),round(float(freq[stop-1])),
                float(np.mean(mean[start:stop])),
                round(float(10*np.log10(max(float(np.mean(p99[start:stop])),1e-25)/floor)),2),
                round(float(10*np.log10(max(float(np.mean(max_power[start:stop])),1e-25)/floor)),2)])
    print("Raw ADC:",a.raw)
    print("Duration:",report["duration_seconds"],"s; FFT blocks:",blocks)
    for x in peaks[:15]:
        print(f'  {x["adc_frequency_hz"]:12.1f} Hz  {x["p99_db_above_local_floor"]:+7.2f} dB')
    print("Report:",a.output)
if __name__=="__main__":
    main()
