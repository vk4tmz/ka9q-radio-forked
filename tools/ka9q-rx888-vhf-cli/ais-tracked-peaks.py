#!/usr/bin/env python3
"""Compare tracked narrow spectral features in existing 162/161 MHz raw ADC files.
Offline only. No tuner/firmware/USB actions."""
import argparse,json
from pathlib import Path
import numpy as np
def spec(path,rate,n=65536,limit=1000):
    x=np.memmap(path,dtype="<i2",mode="r")
    blocks=len(x)//n
    if blocks<2: raise ValueError("too few samples: "+str(path))
    ids=np.linspace(0,blocks-1,min(blocks,limit),dtype=int)
    w=np.hanning(n)
    powers=np.empty((len(ids),n//2+1),dtype=np.float32)
    for k,i in enumerate(ids):
        frame=x[i*n:(i+1)*n].astype(np.float32)/32768
        powers[k]=np.abs(np.fft.rfft(frame*w))**2
    return np.fft.rfftfreq(n,1/rate),np.median(powers,axis=0),np.percentile(powers,99,axis=0)
def features(freq,p,step=1):
    db=10*np.log10(np.maximum(p,1e-25))
    peaks=np.flatnonzero((db[1:-1]>db[:-2])&(db[1:-1]>=db[2:]))+1
    peaks=peaks[(freq[peaks]>100000)&(freq[peaks]<freq[-1]-100000)]
    peaks=peaks[np.argsort(db[peaks])[-80:][::-1]]
    return [(float(freq[k]),float(db[k]),int(k)) for k in peaks]
def main():
    a=argparse.ArgumentParser()
    a.add_argument("at162",type=Path);a.add_argument("at161",type=Path)
    a.add_argument("--rate",type=float,default=20000000)
    a.add_argument("--output",type=Path,required=True)
    v=a.parse_args();v.output.mkdir(parents=True,exist_ok=True)
    f,meda,pa=spec(v.at162,v.rate)
    _,medb,pb=spec(v.at161,v.rate)
    aa=features(f,pa);bb=features(f,pb)
    # Spectral peak pairing hypotheses: stationary and +/-1 MHz.
    tolerance=max(2000,3*(f[1]-f[0]))
    hypotheses={}
    for shift in (-1e6,0.,1e6):
        pair=[]
        for hz,db,k in aa:
            options=[(abs(other-(hz+shift)),other,power) for other,power,_ in bb if abs(other-(hz+shift))<=tolerance]
            if options:
                delta,other,power=min(options)
                pair.append({"at162_adc_hz":round(hz,1),"at161_adc_hz":round(other,1),
                             "observed_shift_hz":round(other-hz,1),
                             "peak_162_db":round(db,2),"peak_161_db":round(power,2)})
        hypotheses[str(int(shift))]=sorted(pair,key=lambda z:z["peak_162_db"],reverse=True)[:30]
    def neighborhood(hz):
        band=np.abs(f-hz)<20000
        if not np.any(band):return None
        return {"frequency_hz":hz,"median_162_power":float(np.max(meda[band])),
                "p99_162_power":float(np.max(pa[band])),
                "median_161_power":float(np.max(medb[band])),
                "p99_161_power":float(np.max(pb[band]))}
    report={"source_162":str(v.at162),"source_161":str(v.at161),
       "sample_rate_hz":v.rate,"frequency_resolution_hz":float(f[1]-f[0]),
       "matching_tolerance_hz":tolerance,"candidate_pairs_by_shift_hz":hypotheses,
       "target_windows":[neighborhood(x) for x in (1e6,1.602e6,1.62e6,2e6,3.594e6,6.044e6,7.595e6)],
       "caveats":["An apparent 1MHz spectral shift alone cannot establish an RF transmission.",
                  "Real ADC spectra have sign ambiguity; persistent local spurs may be tuning dependent.",
                  "Short captures and unequal capture durations limit AIS detection confidence."]}
    (v.output/"tracked-peaks.json").write_text(json.dumps(report,indent=2)+"\n")
    for shift,pairs in hypotheses.items():
        print("Shift",shift,"Hz; matched prominent peaks:",len(pairs))
        for x in pairs[:5]: print(" ",x["at162_adc_hz"],"->",x["at161_adc_hz"])
    print("Report:",v.output/"tracked-peaks.json")
if __name__=="__main__":main()
