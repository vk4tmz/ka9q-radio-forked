#!/usr/bin/env bash
# Compare RX888 tuner initialisation and tuning without changing existing defaults.
# Invoke only when original SDDC RX888 session has been stopped.
set -euo pipefail
if [[ "${KA9Q_RX888_ALLOW_LIVE:-}" != "YES" ]]; then
  echo "REFUSED: explicitly export KA9Q_RX888_ALLOW_LIVE=YES (RX888 must be exclusively available)" >&2
  exit 2
fi
cd "$(dirname "$0")/../.."
if [[ ! -x ./build/ka9q-rx888-capture-host || ! -f ./src/rx888.so ]]; then
  echo "Build src/rx888.so and capture-host first" >&2
  exit 2
fi
out="${1:-$HOME/tools/ais-gain-runs/rx888-ka9q/tuner-diagnostics-$(date -u +%Y%m%dT%H%M%SZ)}"
mkdir -p "$out"
for mode in baseline bw8; do
  echo "=== $mode ==="
  extras=()
  [[ "$mode" == bw8 ]] && extras=(--analog-bandwidth 8M)
  KA9Q_RX888_ALLOW_LIVE=YES ./build/ka9q-rx888-capture-host \
    --frequency 92.7M --samprate 20M --ppm 16.2 \
    --rf-gain 29.7 --if-gain 16.3 --seconds 2 \
    --tuner-diagnostics "${extras[@]}" \
    --format real-s16 --output /dev/null \
    >"$out/$mode.stdout" 2>"$out/$mode.log"
done
python3 - "$out" <<'PY'
import pathlib, re, sys
root = pathlib.Path(sys.argv[1])
pat = re.compile(r'R828TRACE snapshot stage=(\S+) source=shadow(.*)$')
states = {}
for mode in ("baseline", "bw8"):
    snapshots = {}
    for line in (root/f"{mode}.log").read_text(errors="replace").splitlines():
        match = pat.search(line)
        if match:
            snapshots[match.group(1)] = dict(
                (int(k, 16), int(v, 16))
                for k, v in re.findall(r'([0-9a-f]{2})=([0-9a-f]{2})', match.group(2))
            )
    states[mode] = snapshots
out = ["RX888 tuner software-shadow comparison (NOT hardware register readback)", ""]
for stage in ("after-init", "after-bandwidth", "after-tuning"):
    a, b = (states[mode].get(stage, {}) for mode in ("baseline", "bw8"))
    out.append(f"[{stage}]")
    if not a or not b:
        out.append("MISSING snapshots; inspect logs")
    else:
        diff = [(k, a.get(k), b.get(k)) for k in sorted(set(a)|set(b)) if a.get(k) != b.get(k)]
        out += [f"reg 0x{k:02x}: baseline=0x{x:02x} bw8=0x{y:02x}" for k,x,y in diff] or ["No differences"]
    out.append("")
(root/"comparison.txt").write_text("\n".join(out)+"\n")
print("\n".join(out))
PY
archive="$out.tar.gz"
tar -czf "$archive" -C "$(dirname "$out")" "$(basename "$out")"
printf 'Report archive: %s\n' "$archive"
