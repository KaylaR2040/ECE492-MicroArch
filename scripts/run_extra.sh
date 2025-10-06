#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

mkdir -p output

echo "Running extra1 ..."
./sim 64 3584 7 0 0 0 0 traces/gcc_trace.txt        > output/extra1.out.txt

echo "Running extra2 ..."
./sim 16 1024 64 0 0 0 0 traces/gcc_trace.txt       > output/extra2.out.txt

echo "Running extra3 ..."
./sim 16 1024 2 0 0 0 0 traces/perl_trace.txt       > output/extra3.out.txt

echo "Running extra4 ..."
./sim 32 1024 2 8192 4 0 0 traces/vortex_trace.txt  > output/extra4.out.txt

echo "Running extra5 (prefetch case; EXPECT MISMATCH in 463) ..."
./sim 64 8192 4 0 0 8 4 traces/compress_trace.txt   > output/extra5.out.txt

echo "Done. Outputs in ./output"
