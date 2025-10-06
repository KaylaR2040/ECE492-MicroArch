#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

d() { # name expected output
  local name="$1"; local exp="$2"; local got="$3"
  echo "== $name =="
  # -i ignore case, -w ignore all whitespace (matches most graders)
  if diff -iw "$exp" "$got" > /dev/null; then
    echo "PASS"
  else
    echo "DIFF:"
    diff -iw "$exp" "$got" || true
  fi
  echo
}

d "extra1" expected/extra1.64_3584_7_0_0_0_0_gcc.txt        output/extra1.out.txt
d "extra2" expected/extra2.16_1024_64_0_0_0_0_gcc.txt       output/extra2.out.txt
d "extra3" expected/extra3.16_1024_2_0_0_0_0_perl.txt       output/extra3.out.txt
d "extra4" expected/extra4.32_1024_2_8192_4_0_0_vortex.txt  output/extra4.out.txt

echo "Note: extra5 uses prefetch (N=8,M=4). Your 463 build (no prefetch) will not match."
