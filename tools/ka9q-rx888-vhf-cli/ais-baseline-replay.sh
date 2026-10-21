#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
raw="${1:-/tmp/ka9q-ais-if-survey.s16}"
out="${2:-/tmp/ka9q-ais-replay-$(date -u +%Y%m%dT%H%M%SZ)}"
decoder="$HOME/tools/AIS-catcher/build/AIS-catcher"
test -s "$raw" || { echo "Missing ADC file: $raw" >&2; exit 1; }
test -x "$decoder" || { echo "Missing AIS-Catcher: $decoder" >&2; exit 1; }
mkdir -p "$out"
printf 'ADC source: %s\nADC rate: 19200000\nRF centre: 162000000\n' "$raw" >"$out/metadata.txt"
for offset in 1602000 -1602000 1620000 -1620000; do
  echo "Replaying IF $offset"
  ./build/ka9q-rx888-capture-host --input "$raw" --samprate 19.2M \
    --format cf32 --digital-if "$offset" --iq-rate 384000 \
    --output "$out/if-$offset.cf32" >"$out/convert-$offset.log" 2>&1
  "$decoder" -c AB -v 10 -M D -o 5 -m 4 \
    -r CF32 "$out/if-$offset.cf32" -s 384000 \
    >"$out/ais-$offset.log" 2>&1 || echo "Decoder returned nonzero for $offset" >&2
done
tar -czf "$out-report.tar.gz" -C "$out" --exclude='*.cf32' .
echo "Diagnostic report: $out-report.tar.gz"
