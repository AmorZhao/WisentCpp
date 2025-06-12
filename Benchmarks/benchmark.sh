#!/bin/bash

BENCHMARK_FOLDERS=(
    "WisentCompressor"
    # "WisentFileSize"
    # "Compression"
)

for BENCHMARK_DIR in "${BENCHMARK_FOLDERS[@]}"; do
    echo "=== Building and running benchmark in: $BENCHMARK_DIR ==="
    BUILD_DIR="$BENCHMARK_DIR/build"
    SRC_DIR="$BENCHMARK_DIR"

    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR" || exit 1

    cmake -DCMAKE_BUILD_TYPE=Debug ..
    make

    if [ -f "./Benchmarks" ]; then
        ./Benchmarks --benchmark_out=benchmark_results.csv --benchmark_out_format=csv
        echo "Results written to $BUILD_DIR/benchmark_results.csv"
    else
        echo "Benchmarks executable not found in $BUILD_DIR"
    fi

    cd - > /dev/null
done