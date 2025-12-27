#!/bin/bash

# Advanced Benchmark Script for OIIO Resample
# Compares Reference OIIO vs New (HWY) OIIO

REF_OIIO="/mnt/e/UBS/bin/oiiotool"
# Try to find new oiiotool
if [ -f "../build/bin/oiiotool" ]; then
    NEW_OIIO="./build/bin/oiiotool"
elif [ -f "../build/bin/Release/oiiotool" ]; then
    NEW_OIIO="../build/bin/Release/oiiotool"
else
    NEW_OIIO="oiiotool" # Fallback to PATH
fi

ITERATIONS=10

echo "Reference OIIO: $REF_OIIO"
echo "New OIIO:       $NEW_OIIO"
echo "Iterations:     $ITERATIONS"
echo "------------------------------------------------"

# 1. Generate Images
echo "Generating test images..."
if [ ! -f "bench_u8.tif" ]; then
    "$NEW_OIIO" --pattern checker 2048x2048 3 -d uint8 -o bench_u8.tif
fi
if [ ! -f "bench_u16.tif" ]; then
    "$NEW_OIIO" --pattern checker 2048x2048 3 -d uint16 -o bench_u16.tif
fi
if [ ! -f "bench_u32.tif" ]; then
    "$NEW_OIIO" --pattern checker 2048x2048 3 -d uint32 -o bench_u32.tif
fi
if [ ! -f "bench_f16.exr" ]; then
    "$NEW_OIIO" --pattern checker 2048x2048 3 -d half -o bench_f16.exr
fi
if [ ! -f "bench_f32.tif" ]; then
    "$NEW_OIIO" --pattern checker 2048x2048 3 -d float -o bench_f32.tif
fi
if [ ! -f "bench_f64.tif" ]; then
    "$NEW_OIIO" --pattern checker 2048x2048 3 -d double -o bench_f64.tif
fi

# Function to measure average time in ms
measure_avg_ms() {
    local tool=$1
    local input=$2
    local scale=$3
    local total_ns=0
    
    # Warmup
    "$tool" "$input" --resample:interp=1 "$scale" -o out.null

    for ((i=1; i<=ITERATIONS; i++)); do
        local start_ns=$(date +%s%N)
        "$tool" "$input" --resample:interp=1 "$scale" -o out.null
        local end_ns=$(date +%s%N)
        # Handle wraparound if necessary (unlikely for short durations) or use python for precision if date is usually enough
        # OSX date doesn't support %N. Assuming Linux as requested.
        local diff=$((end_ns - start_ns))
        total_ns=$((total_ns + diff))
    done
    
    local avg_ms=$((total_ns / ITERATIONS / 1000000))
    echo "$avg_ms"
}

# Store results
declare -A results_ref
declare -A results_new

# Benchmarking Loop
echo "Starting Benchmark..."
printf "% -15s % -10s | % -10s | % -10s | % -10s\n" "Image" "Scale" "Ref (ms)" "New (ms)" "Speedup"
echo "-----------------------------------------------------------------------"

for img in "bench_u8.tif" "bench_u16.tif" "bench_u32.tif" "bench_f32.tif" "bench_f64.tif" "bench_f16.exr"; do
    for scale in "75%" "50%" "25%"; do
        t_ref=$(measure_avg_ms "$REF_OIIO" "$img" "$scale")
        t_new=$(measure_avg_ms "$NEW_OIIO" "$img" "$scale")
        
        # Calculate speedup (Ref / New)
        if [ "$t_new" -eq 0 ]; then t_new=1; fi # Avoid div by zero
        # Floating point math in bash via awk
        speedup=$(awk "BEGIN {printf \"%.2fx\", $t_ref/$t_new}")
        
        printf "% -15s % -10s | % -10s | % -10s | % -10s\n" "$img" "$scale" "$t_ref" "$t_new" "$speedup"
    done
    echo "-----------------------------------------------------------------------"
done