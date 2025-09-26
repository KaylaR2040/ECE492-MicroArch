#!/bin/bash

# Script to collect data for GRAPH #1: L1 Size vs Miss Rate
echo "=== COLLECTING GRAPH #1 DATA ==="
echo "L1 Size vs Miss Rate (Associativity Curves)"
echo

# Cache sizes in bytes (1KB to 1MB)
sizes=(1024 2048 4096 8192 16384 32768 65536 131072 262144 524288 1048576)
# Associativities  
assocs=(1 2 4 8 1024)  # 1024 = fully-associative for most practical purposes

echo "Size(KB),Direct-Mapped,2-Way,4-Way,8-Way,Fully-Assoc"

for size in "${sizes[@]}"; do
    size_kb=$((size / 1024))
    echo -n "${size_kb},"
    
    for assoc in "${assocs[@]}"; do
        # Run simulation and extract miss rate
        miss_rate=$(./sim $size 32 $assoc 0 0 0 0 "../../../traces/gcc_trace.txt" 2>/dev/null | grep "L1 miss rate:" | awk '{print $4}')
        
        if [ -z "$miss_rate" ]; then
            miss_rate="ERROR"
        fi
        
        if [ $assoc -eq 1024 ]; then
            echo "$miss_rate"  # Last column, newline
        else
            echo -n "$miss_rate,"  # Not last column, comma
        fi
    done
done

echo
echo "Data collection complete! Use this CSV data for your graph."
