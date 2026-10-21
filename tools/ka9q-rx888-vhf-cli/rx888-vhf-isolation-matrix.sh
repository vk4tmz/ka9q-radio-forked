#!/usr/bin/env bash
# Five-case R828D isolation matrix; requires exclusive RX888 access.
set -euo pipefail
if [[ "${KA9Q_RX888_ALLOW_LIVE:-}" != YES ]]; then
  echo "REFUSED: set KA9Q_RX888_ALLOW_LIVE=YES; stop SDDC first" >&2
  exit 2
fi
cd "$(dirname "$0")/../.."
[[ -x ./build/ka9q-rx888-capture-host ]] || { echo "Build capture-host first" >&2; exit 2; }
out="${1:-/tmp/rx888-vhf-isolation-$(date -u +%Y%m%dT%H%M%SZ)}"
mkdir -p "$out"
for mode in baseline r0a r0b r1e bw8; do
  echo "Capturing $mode"
  args=()
  case "$mode" in
    r0a) args=(--isolation-case 1);;
    r0b) args=(--isolation-case 2);;
    r1e) args=(--isolation-case 3);;
    bw8) args=(--analog-bandwidth 8M);;
  esac
  KA9Q_RX888_ALLOW_LIVE=YES ./build/ka9q-rx888-capture-host \
    --frequency 92.7M --samprate 20M --ppm 16.2 \
    --rf-gain 29.7 --if-gain 16.3 \
    --format cf32 --digital-if 1.602M --iq-rate 400000 \
    --seconds 4 --tuner-diagnostics "${args[@]}" \
    --output "$out/$mode.cf32" >"$out/$mode.stdout" 2>"$out/$mode.log"
done
python3 - "$out" <<'PY'
import array, math, pathlib, struct, sys
p=pathlib.Path(sys.argv[1])
lines=["case,samples,rms,p95_power_db_above_median"]
for mode in ("baseline","r0a","r0b","r1e","bw8"):
    path=p/f"{mode}.cf32"
    # CF32 interleaved I/Q (little endian); bounded-memory sample powers.
    # Subsample deterministically for percentiles, calculate exact RMS streaming.
    n=0
    power_sum=0.0
    sample_power=[]
    with path.open("rb") as fh:
        while chunk:=fh.read(8*65536):
            if len(chunk)%8: raise ValueError(f"Truncated CF32: {path}")
            vals=array.array("f")
            vals.frombytes(chunk)
            if sys.byteorder!="little": vals.byteswap()
            for i in range(0,len(vals),2):
                power=vals[i]**2+vals[i+1]**2
                power_sum+=power
                if (n%64)==0: sample_power.append(power)
                n+=1
    if not n: raise ValueError(f"Empty CF32: {path}")
    sample_power.sort()
    med=sample_power[len(sample_power)//2]
    p95=sample_power[min(len(sample_power)-1,int(.95*(len(sample_power)-1)))]
    rise=10*math.log10(max(p95,1e-20)/max(med,1e-20))
    lines.append(f"{mode},{n},{math.sqrt(power_sum/n):.7g},{rise:.3f}")
(p/"metrics.csv").write_text("\\n".join(lines)+"\\n")
print("\\n".join(lines))
PY
tar -czf "$out.tar.gz" -C "$(dirname "$out")" "$(basename "$out")"
echo "Archive: $out.tar.gz"
