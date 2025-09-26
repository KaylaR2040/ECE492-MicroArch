#!/bin/bash

# =============================================================================
# ECE 492 Cache Simulator Simple Test
# =============================================================================
# This script builds and runs a simple test to verify the simulator works

echo "===== Simple Cache Simulator Test ====="
echo

# Navigate to the cpp_files directory
cd "/Users/kaylaradu/GitHubRepos/GitNCSU/ECE492-MicroArch/Sample source code (trace readers, etc/read-trace/cpp_files"

# Build the simulator
echo "Building cache simulator..."
make clean
make

if [ $? -ne 0 ]; then
    echo "ERROR: Failed to build simulator"
    exit 1
fi

echo "Build successful!"
echo

# Test with a simple configuration
echo "Running simple test..."
echo "Configuration: 16 1024 1 0 0 0 0 gcc_trace.txt"

# Run with val1 configuration
./sim 16 1024 1 0 0 0 0 "../../../traces/gcc_trace.txt" > simple_test_output.txt 2>&1

if [ $? -eq 0 ]; then
    echo "✓ Test completed successfully"
    echo "Output saved to simple_test_output.txt"
    echo
    echo "First few lines of output:"
    head -20 simple_test_output.txt
    echo
    echo "Last few lines of output:"
    tail -20 simple_test_output.txt
else
    echo "✗ Test failed"
    echo "Error output:"
    cat simple_test_output.txt
    exit 1
fi
