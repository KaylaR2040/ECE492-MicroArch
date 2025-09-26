#!/bin/bash

# =============================================================================
# ECE 492 Cache Simulator Validation Script  
# =============================================================================
# This script runs specific validation tests and compares key metrics

echo "===== Cache Simulator Validation Test ====="
echo

cd "/Users/kaylaradu/GitHubRepos/GitNCSU/ECE492-MicroArch/Sample source code (trace readers, etc/read-trace/cpp_files"

# Build if necessary
if [ ! -f sim ]; then
    echo "Building simulator..."
    make
fi

echo "Running validation test 1 (val1 configuration)..."
echo "Configuration: 16 1024 1 0 0 0 0"

# Run our simulator
./sim 16 1024 1 0 0 0 0 "../../../traces/gcc_trace.txt" > our_val1_output.txt

# Extract key metrics from our output
echo "Our output - Key metrics:"
grep "L1 reads:" our_val1_output.txt
grep "L1 read misses:" our_val1_output.txt  
grep "L1 writes:" our_val1_output.txt
grep "L1 write misses:" our_val1_output.txt
grep "L1 miss rate:" our_val1_output.txt
grep "L1 writebacks:" our_val1_output.txt
grep "memory traffic:" our_val1_output.txt

echo
echo "Expected output - Key metrics:"
grep "L1 reads:" "../../../val-proj1/val1.16_1024_1_0_0_0_0_gcc.txt"
grep "L1 read misses:" "../../../val-proj1/val1.16_1024_1_0_0_0_0_gcc.txt"
grep "L1 writes:" "../../../val-proj1/val1.16_1024_1_0_0_0_0_gcc.txt"
grep "L1 write misses:" "../../../val-proj1/val1.16_1024_1_0_0_0_0_gcc.txt"
grep "L1 miss rate:" "../../../val-proj1/val1.16_1024_1_0_0_0_0_gcc.txt"
grep "L1 writebacks:" "../../../val-proj1/val1.16_1024_1_0_0_0_0_gcc.txt"
grep "memory traffic:" "../../../val-proj1/val1.16_1024_1_0_0_0_0_gcc.txt"

echo
echo "===== Comparison Complete ====="
