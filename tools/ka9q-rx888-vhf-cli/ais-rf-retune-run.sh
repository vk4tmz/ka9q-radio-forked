#!/usr/bin/env bash
# Two controlled RF tuning positions; preserve original Phil-derived tuner setup.
set -euo pipefail
cd "$(dirname "$0")/../.."
if [[ "${KA9Q_RX888_ALLOW_LIVE:-}" != YES ]]; then
  echo "REFUSED: stop SDDC, then set KA9Q_RX888_ALLOW_LIVE=YES" >&2
  exit 2
fi
out="${1:-/tmp/ka9q-ais-rf-retune-$(date -u +%Y%m%dT%H%M%SZ)}"
mkdir -p "$out"
for mhz in 162 161; do
  echo "Capturing RF ${mhz} MHz, original tuner settings"
  KA9Q_RX888_ALLOW_LIVE=YES ./build/ka9q-rx888-capture-host \
    --frequency "${mhz}M" --samprate 20M --ppm 16.2 \
    --rf-gain 29.7 --if-gain 16.3 --seconds 5 \
    --format real-s16 --tuner-diagnostics \
    --output "$out/rf-${mhz}.s16" \
    >"$out/rf-${mhz}.stdout" 2>"$out/rf-${mhz}.log"
done
python3 tools/ka9q-rx888-vhf-cli/ais-rf-retune-compare.py \
  "$out/rf-162.s16" "$out/rf-161.s16" \
  --rate 20000000 --output "$out"
printf 'Original tuner register table, 20 MS/s, +16.2ppm, RF gain 29.7 dB, IF gain 16.3 dB\n' >"$out/metadata.txt"
tar -czf "$out-report.tar.gz" -C "$out" --exclude='*.s16' .
echo "Report: $out-report.tar.gz"
echo "Raw samples retained locally: $out"
