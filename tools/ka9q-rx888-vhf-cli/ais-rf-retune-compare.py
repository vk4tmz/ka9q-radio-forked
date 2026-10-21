#!/usr/bin/env python3
"""Compare two RX888 real-ADC spectra after a known 1 MHz RF retune."""
import argparse, json
from pathlib import Path
import numpy as np

def spectrum(path, rate, nfft=32768, maxblocks=1024):
    v=np.memmap(path,dtype="<i2",mode="r")
    total=len(v)//nfft
    if total<2: raise ValueError(f"Capture too short: {path}")
    ids=np.linspace(0,total-1,min(total,maxblocks),dtype=np.int64)
    window=np.hanning(nfft)
    arr=np.empty((len(ids),nfft//2+1),dtype=np.float32)
    for j,i in enumerate(ids):
        segment=v[i*nfft:(i+1)*nfft].astype(np.float32)/32768
        arr[j]=np.abs(np.fft.rfft(segment*window))**2
    return np.fft.rfftfreq(nfft,1/rate),np.median(arr,axis=0),np.percentile(arr,99,axis=0)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("capture_162",type=Path)
    ap.add_argument("capture_161",type=Path)
    ap.add_argument("--rate",type=float,default=20_000_000)
    ap.add_argument("--output",type=Path,required=True)
    a=ap.parse_args()
    freq,p162,pk162=spectrum(a.capture_162,a.rate)
    _,p161,pk161=spectrum(a.capture_161,a.rate)
    step=freq[1]-freq[0]
    # Test signed 1 MHz moves, no-move and mirrored RF/IF alternatives.
    testshifts=(-1000000,0,1000000)
    def score(shift):
        idx=np.arange(len(freq))
        j=idx+int(round(shift/step))
        good=(freq>200000)&(freq<a.rate/2-200000)&(j>=0)&(j<len(freq))
        x=np.log10(np.maximum(pk162[good],1e-22))
        y=np.log10(np.maximum(pk161[j[good]],1e-22))
        # Remove slowly changing analog IF envelope.
        width=max(3,int(round(150000/step)))
        kernel=np.ones(width)/width
        x=x-np.convolve(x,kernel,mode="same")
        y=y-np.convolve(y,kernel,mode="same")
        return float(np.corrcoef(x,y)[0,1])
    scores={str(k):round(score(k),5) for k in testshifts}
    def prominent(peak):
        vals=np.log10(np.maximum(peak,1e-22))
        mask=(freq>200000)&(freq<a.rate/2-200000)
        inds=np.flatnonzero(mask)
        top=inds[np.argsort(vals[inds])[-30:][::-1]]
        return [{"adc_hz":round(float(freq[i]),1),"log10_power":round(float(vals[i]),3)} for i in top]
    data={"rf_frequencies_hz":[162000000,161000000],"adc_rate_hz":a.rate,"bin_hz":step,
      "positive_adc_frequency_only":True,
      "shift_hypotheses_hz":scores,
      "strongest_162":prominent(pk162),"strongest_161":prominent(pk161),
      "note":"A correlation peak suggests shared spectral structure shifting with RF tuning, not necessarily AIS; a null result is inconclusive without a persistent RF reference."}
    a.output.mkdir(parents=True,exist_ok=True)
    (a.output/"comparison.json").write_text(json.dumps(data,indent=2)+"\n")
    print(json.dumps({"shift_correlations":scores,"report":str(a.output/"comparison.json")},indent=2))

if __name__=="__main__": main()
