#!/bin/bash

# DFS Performance Benchmark Script
# Usage: ./scripts/benchmark.sh [test_type] [options]

set -e

BUILD_DIR="build"
CLI="$BUILD_DIR/dfs_cli"
TEST_DATA_DIR="test_data"
RESULTS_DIR="benchmark_results"

# Default parameters
TEST_TYPE=${1:-"all"}
FILE_SIZES=(1024 10240 102400 1048576 10485760)  # 1KB to 10MB
NUM_FILES=10
CONCURRENT_CLIENTS=5

echo "DFS Performance Benchmark"
echo "========================="

# Create directories
mkdir -p $TEST_DATA_DIR $RESULTS_DIR

# Generate test files
generate_test_files() {
    echo "Generating test files..."
    for size in "${FILE_SIZES[@]}"; do
        for ((i=1; i<=NUM_FILES; i++)); do
            filename="$TEST_DATA_DIR/test_${size}_${i}.dat"
            if [ ! -f "$filename" ]; then
                dd if=/dev/urandom of="$filename" bs=$size count=1 2>/dev/null
            fi
        done
    done
}

# Upload performance test
test_upload_performance() {
    echo ""
    echo "Upload Performance Test"
    echo "======================="
    
    results_file="$RESULTS_DIR/upload_results_$(date +%Y%m%d_%H%M%S).csv"
    echo "file_size,replication,encryption,erasure_coding,duration_ms,throughput_mbps" > $results_file
    
    for size in "${FILE_SIZES[@]}"; do
        for replication in 1 2 3; do
            for encryption in false true; do
                for erasure_coding in false true; do
                    echo "Testing: ${size}B, R=$replication, E=$encryption, EC=$erasure_coding"
                    
                    total_time=0
                    successful_uploads=0
                    
                    for ((i=1; i<=NUM_FILES; i++)); do
                        filename="test_${size}_${i}.dat"
                        remote_name="bench_${size}_${replication}_${encryption}_${erasure_coding}_${i}.dat"
                        
                        start_time=$(date +%s%3N)
                        
                        cmd="$CLI put $TEST_DATA_DIR/$filename $remote_name --replication $replication"
                        if [ "$encryption" = "true" ]; then
                            cmd="$cmd --encrypt --password benchpass123"
                        fi
                        if [ "$erasure_coding" = "true" ]; then
                            cmd="$cmd --erasure-coding"
                        fi
                        
                        if $cmd >/dev/null 2>&1; then
                            end_time=$(date +%s%3N)
                            duration=$((end_time - start_time))
                            total_time=$((total_time + duration))
                            ((successful_uploads++))
                        fi
                    done
                    
                    if [ $successful_uploads -gt 0 ]; then
                        avg_time=$((total_time / successful_uploads))
                        throughput=$(echo "scale=2; $size * $successful_uploads * 8 / ($total_time / 1000) / 1048576" | bc -l)
                        echo "$size,$replication,$encryption,$erasure_coding,$avg_time,$throughput" >> $results_file
                        echo "  Average time: ${avg_time}ms, Throughput: ${throughput} Mbps"
                    fi
                done
            done
        done
    done
    
    echo "Upload results saved to: $results_file"
}

# Download performance test
test_download_performance() {
    echo ""
    echo "Download Performance Test"
    echo "========================="
    
    results_file="$RESULTS_DIR/download_results_$(date +%Y%m%d_%H%M%S).csv"
    echo "file_size,replication,encryption,erasure_coding,duration_ms,throughput_mbps" > $results_file
    
    for size in "${FILE_SIZES[@]}"; do
        for replication in 1 2 3; do
            for encryption in false true; do
                for erasure_coding in false true; do
                    echo "Testing download: ${size}B, R=$replication, E=$encryption, EC=$erasure_coding"
                    
                    total_time=0
                    successful_downloads=0
                    
                    for ((i=1; i<=NUM_FILES; i++)); do
                        remote_name="bench_${size}_${replication}_${encryption}_${erasure_coding}_${i}.dat"
                        local_name="$TEST_DATA_DIR/downloaded_${remote_name}"
                        
                        start_time=$(date +%s%3N)
                        
                        cmd="$CLI get $remote_name $local_name"
                        if [ "$encryption" = "true" ]; then
                            cmd="$cmd --password benchpass123"
                        fi
                        
                        if $cmd >/dev/null 2>&1; then
                            end_time=$(date +%s%3N)
                            duration=$((end_time - start_time))
                            total_time=$((total_time + duration))
                            ((successful_downloads++))
                            rm -f "$local_name"  # Clean up
                        fi
                    done
                    
                    if [ $successful_downloads -gt 0 ]; then
                        avg_time=$((total_time / successful_downloads))
                        throughput=$(echo "scale=2; $size * $successful_downloads * 8 / ($total_time / 1000) / 1048576" | bc -l)
                        echo "$size,$replication,$encryption,$erasure_coding,$avg_time,$throughput" >> $results_file
                        echo "  Average time: ${avg_time}ms, Throughput: ${throughput} Mbps"
                    fi
                done
            done
        done
    done
    
    echo "Download results saved to: $results_file"
}

# Concurrent operations test
test_concurrent_operations() {
    echo ""
    echo "Concurrent Operations Test"
    echo "=========================="
    
    results_file="$RESULTS_DIR/concurrent_results_$(date +%Y%m%d_%H%M%S).csv"
    echo "concurrent_clients,operation,total_time_ms,operations_per_second" > $results_file
    
    for clients in 1 2 5 10; do
        echo "Testing with $clients concurrent clients..."
        
        # Concurrent uploads
        start_time=$(date +%s%3N)
        for ((c=1; c<=clients; c++)); do
            (
                for ((i=1; i<=5; i++)); do
                    filename="test_10240_$i.dat"
                    remote_name="concurrent_upload_${c}_${i}.dat"
                    $CLI put "$TEST_DATA_DIR/$filename" "$remote_name" >/dev/null 2>&1
                done
            ) &
        done
        wait
        end_time=$(date +%s%3N)
        upload_time=$((end_time - start_time))
        upload_ops_per_sec=$(echo "scale=2; $clients * 5 * 1000 / $upload_time" | bc -l)
        echo "$clients,upload,$upload_time,$upload_ops_per_sec" >> $results_file
        
        # Concurrent downloads
        start_time=$(date +%s%3N)
        for ((c=1; c<=clients; c++)); do
            (
                for ((i=1; i<=5; i++)); do
                    remote_name="concurrent_upload_${c}_${i}.dat"
                    local_name="$TEST_DATA_DIR/concurrent_download_${c}_${i}.dat"
                    $CLI get "$remote_name" "$local_name" >/dev/null 2>&1
                    rm -f "$local_name"
                done
            ) &
        done
        wait
        end_time=$(date +%s%3N)
        download_time=$((end_time - start_time))
        download_ops_per_sec=$(echo "scale=2; $clients * 5 * 1000 / $download_time" | bc -l)
        echo "$clients,download,$download_time,$download_ops_per_sec" >> $results_file
        
        echo "  Upload: ${upload_ops_per_sec} ops/sec, Download: ${download_ops_per_sec} ops/sec"
        
        # Clean up test files
        for ((c=1; c<=clients; c++)); do
            for ((i=1; i<=5; i++)); do
                remote_name="concurrent_upload_${c}_${i}.dat"
                $CLI delete "$remote_name" >/dev/null 2>&1 || true
            done
        done
    done
    
    echo "Concurrent operations results saved to: $results_file"
}

# Fault tolerance test
test_fault_tolerance() {
    echo ""
    echo "Fault Tolerance Test"
    echo "===================="
    
    # Upload test files with different replication factors
    for replication in 2 3 5; do
        echo "Testing fault tolerance with replication factor $replication..."
        
        filename="test_102400_1.dat"
        remote_name="fault_test_r${replication}.dat"
        
        # Upload file
        if $CLI put "$TEST_DATA_DIR/$filename" "$remote_name" --replication $replication >/dev/null 2>&1; then
            echo "  ✅ File uploaded with replication $replication"
            
            # Verify file can be downloaded
            local_name="$TEST_DATA_DIR/fault_download_r${replication}.dat"
            if $CLI get "$remote_name" "$local_name" >/dev/null 2>&1; then
                echo "  ✅ File successfully downloaded"
                
                # Verify file integrity
                if cmp -s "$TEST_DATA_DIR/$filename" "$local_name"; then
                    echo "  ✅ File integrity verified"
                else
                    echo "  ❌ File integrity check failed"
                fi
                rm -f "$local_name"
            else
                echo "  ❌ File download failed"
            fi
            
            # Clean up
            $CLI delete "$remote_name" >/dev/null 2>&1 || true
        else
            echo "  ❌ File upload failed"
        fi
    done
}

# Generate summary report
generate_report() {
    echo ""
    echo "Generating Summary Report"
    echo "========================="
    
    report_file="$RESULTS_DIR/benchmark_summary_$(date +%Y%m%d_%H%M%S).txt"
    
    {
        echo "DFS Performance Benchmark Summary"
        echo "Generated: $(date)"
        echo "================================="
        echo ""
        
        # Find latest result files
        latest_upload=$(ls -t $RESULTS_DIR/upload_results_*.csv 2>/dev/null | head -1)
        latest_download=$(ls -t $RESULTS_DIR/download_results_*.csv 2>/dev/null | head -1)
        latest_concurrent=$(ls -t $RESULTS_DIR/concurrent_results_*.csv 2>/dev/null | head -1)
        
        if [ -n "$latest_upload" ]; then
            echo "Upload Performance (Best Results):"
            echo "=================================="
            awk -F',' 'NR>1 {if($6>max[$1]) {max[$1]=$6; line[$1]=$0}} END {for(size in max) print "Size: " size "B, Best throughput: " max[size] " Mbps"}' "$latest_upload"
            echo ""
        fi
        
        if [ -n "$latest_download" ]; then
            echo "Download Performance (Best Results):"
            echo "===================================="
            awk -F',' 'NR>1 {if($6>max[$1]) {max[$1]=$6; line[$1]=$0}} END {for(size in max) print "Size: " size "B, Best throughput: " max[size] " Mbps"}' "$latest_download"
            echo ""
        fi
        
        if [ -n "$latest_concurrent" ]; then
            echo "Concurrent Operations:"
            echo "====================="
            awk -F',' 'NR>1 && $2=="upload" {print "Upload with " $1 " clients: " $4 " ops/sec"}' "$latest_concurrent"
            awk -F',' 'NR>1 && $2=="download" {print "Download with " $1 " clients: " $4 " ops/sec"}' "$latest_concurrent"
            echo ""
        fi
        
        echo "System Information:"
        echo "=================="
        echo "OS: $(uname -a)"
        echo "CPU: $(grep 'model name' /proc/cpuinfo | head -1 | cut -d':' -f2 | xargs)"
        echo "Memory: $(free -h | grep Mem | awk '{print $2}')"
        echo "Disk: $(df -h . | tail -1 | awk '{print $2 " total, " $4 " available"}')"
        
    } > "$report_file"
    
    echo "Summary report saved to: $report_file"
    echo ""
    cat "$report_file"
}

# Main execution
case $TEST_TYPE in
    "upload")
        generate_test_files
        test_upload_performance
        ;;
    "download")
        generate_test_files
        test_upload_performance  # Need files to download
        test_download_performance
        ;;
    "concurrent")
        generate_test_files
        test_concurrent_operations
        ;;
    "fault")
        generate_test_files
        test_fault_tolerance
        ;;
    "all")
        generate_test_files
        test_upload_performance
        test_download_performance
        test_concurrent_operations
        test_fault_tolerance
        generate_report
        ;;
    *)
        echo "Usage: $0 [upload|download|concurrent|fault|all]"
        exit 1
        ;;
esac

echo ""
echo "✅ Benchmark completed!"
echo "Results saved in: $RESULTS_DIR/"