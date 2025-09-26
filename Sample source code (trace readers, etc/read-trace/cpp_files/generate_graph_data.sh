#!/bin/bash

# Script to generate data for your cache exploration graphs
# This will help you collect data for GRAPH #1, #2, #3, etc.

echo "=== CACHE SIMULATOR DATA GENERATION FOR GRAPHS ==="
echo "This script will help you generate data for your report graphs"
echo ""

# Function to run simulation and extract miss rate
run_simulation() {
    local l1_size=$1
    local l1_assoc=$2
    local l1_blocksize=$3
    local l2_size=$4
    local l2_assoc=$5
    local l2_blocksize=$6
    local prefetch_n=$7
    local trace=$8
    
    echo "Running: L1($l1_size, $l1_blocksize, $l1_assoc) L2($l2_size, $l2_blocksize, $l2_assoc) PREF($prefetch_n) on $trace"
    
    # Run the simulator
    ./output/sim $l1_size $l1_blocksize $l1_assoc $l2_size $l2_blocksize $l2_assoc $prefetch_n "../../../traces/$trace" | \
    grep -E "(L1 reads|L1 read misses|L1 miss rate|L2 reads|L2 read misses|L2 miss rate|memory traffic)"
    echo "---"
}

echo "GRAPH #1 DATA: L1 Size vs Miss Rate (Associativity Curves)"
echo "Cache sizes: 1KB, 2KB, 4KB, 8KB, 16KB, 32KB, 64KB, 128KB, 256KB, 512KB, 1MB"
echo "Associativities: 1, 2, 4, 8, fully-associative"
echo ""

# Example for GRAPH #1 - you can expand this
echo "=== Sample GRAPH #1 Data Points ==="
# Direct-mapped (assoc=1) examples
run_simulation 1024 1 32 0 0 0 0 gcc_trace.txt
run_simulation 2048 1 32 0 0 0 0 gcc_trace.txt
run_simulation 4096 1 32 0 0 0 0 gcc_trace.txt

# 2-way set-associative examples  
run_simulation 1024 2 32 0 0 0 0 gcc_trace.txt
run_simulation 2048 2 32 0 0 0 0 gcc_trace.txt
run_simulation 4096 2 32 0 0 0 0 gcc_trace.txt

echo ""
echo "=== Sample GRAPH #3 Data (with L2) ==="
# L1 + L2 hierarchy examples
run_simulation 1024 1 32 16384 8 32 0 gcc_trace.txt
run_simulation 2048 1 32 16384 8 32 0 gcc_trace.txt
run_simulation 4096 1 32 16384 8 32 0 gcc_trace.txt

echo ""
echo "=== Sample GRAPH #4 Data (Block Size Study) ==="
# Block size variation with L1=4KB, assoc=4
run_simulation 4096 4 16 0 0 0 0 gcc_trace.txt
run_simulation 4096 4 32 0 0 0 0 gcc_trace.txt
run_simulation 4096 4 64 0 0 0 0 gcc_trace.txt
run_simulation 4096 4 128 0 0 0 0 gcc_trace.txt

echo ""
echo "Instructions:"
echo "1. Modify this script to generate all data points you need"
echo "2. Record miss rates for each configuration"
echo "3. Plot the data in your preferred graphing tool"
echo "4. Use the miss rates and memory traffic to calculate AAT"
echo ""
echo "AAT Calculation: AAT = Hit_Time + (Miss_Rate × Miss_Penalty)"
echo "Where: Hit_Time = 1 cycle, Miss_Penalty = 100 cycles for memory"
