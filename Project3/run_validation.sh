#!/usr/bin/env bash
set -euo pipefail

declare -a cases=(
  "16 8 1 proj3-traces/val_trace_gcc1 validation/validation/val1.txt out_val1.txt"
  "16 8 2 proj3-traces/val_trace_gcc1 validation/validation/val2.txt out_val2.txt"
  "60 15 3 proj3-traces/val_trace_gcc1 validation/validation/val3.txt out_val3.txt"
  "64 16 8 proj3-traces/val_trace_gcc1 validation/validation/val4.txt out_val4.txt"
  "64 16 4 proj3-traces/val_trace_perl1 validation/validation/val5.txt out_val5.txt"
  "128 16 5 proj3-traces/val_trace_perl1 validation/validation/val6.txt out_val6.txt"
  "256 64 5 proj3-traces/val_trace_perl1 validation/validation/val7.txt out_val7.txt"
  "512 64 7 proj3-traces/val_trace_perl1 validation/validation/val8.txt out_val8.txt"
)

all_ok=1
tmpdiff=$(mktemp)
trap 'rm -f "$tmpdiff"' EXIT

for entry in "${cases[@]}"; do
  read -r rob iq width trace gold out <<<"$entry"
  echo "Running: ./sim $rob $iq $width $trace"
  ./sim "$rob" "$iq" "$width" "$trace" >"$out"
  if diff -iw "$gold" "$out" >"$tmpdiff"; then
    echo "  -> PASS (matches $gold)"
  else
    echo "  -> FAIL (differs from $gold)"
    all_ok=0
    sed -n '1,20p' "$tmpdiff"
  fi
done

if [[ $all_ok -eq 1 ]]; then
  echo "All validation cases passed."
else
  echo "One or more validation cases failed."
  exit 1
fi
