# DFS Testing Guide

This guide covers all aspects of testing the Distributed File System, from unit tests to integration tests and performance benchmarks.

## 🧪 Testing Overview

### Test Types Available
1. **Unit Tests** - Individual component testing
2. **Integration Tests** - End-to-end system testing
3. **Performance Tests** - Benchmarking and load testing
4. **Fault Tolerance Tests** - Failure simulation and recovery
5. **Manual Testing** - Interactive CLI testing

## 🔨 Building and Setting Up Tests

### 1. Build the System
```bash
cd Phase4
mkdir build && cd build
cmake ..
make -j$(nproc)
cd ..
```

### 2. Install Dependencies (if needed)
```bash
# Ubuntu/Debian
sudo apt-get install -y libgtest-dev libgmock-dev

# macOS
brew install googletest
```

## 🏃‍♂️ Running Unit Tests

### Individual Component Tests

```bash
# Test cryptographic functions
./build/crypto_test

# Test erasure coding implementation
./build/erasure_coding_test

# Test metadata management
./build/metadata_manager_test

# Run all unit tests
make test
# or
ctest
```

### Expected Output Example
```
[==========] Running tests from CryptoTest
[ RUN      ] CryptoTest.KeyGeneration
[       OK ] CryptoTest.KeyGeneration (15 ms)
[ RUN      ] CryptoTest.EncryptionDecryption
[       OK ] CryptoTest.EncryptionDecryption (23 ms)
[==========] 8 tests from CryptoTest (145 ms total)
```

## 🌐 Integration Testing

### 1. Start Test Cluster
```bash
# Option 1: Using scripts
./scripts/start_cluster.sh 3

# Option 2: Using Docker
docker-compose up -d

# Option 3: Manual startup (3 terminals)
# Terminal 1:
./build/master_server

# Terminal 2:
./build/web_server 8080

# Terminal 3-5:
./build/chunk_server --server-id=chunk-server-1 --port=60051
./build/chunk_server --server-id=chunk-server-2 --port=60052
./build/chunk_server --server-id=chunk-server-3 --port=60053
```

### 2. Run Integration Tests
```bash
# Wait for cluster to be ready (2-3 seconds)
sleep 3

# Run integration test suite
./build/integration_test

# Or run specific test categories
./build/integration_test --gtest_filter="*BasicFileOperations*"
./build/integration_test --gtest_filter="*EncryptedFileOperations*"
./build/integration_test --gtest_filter="*FaultTolerance*"
```

### 3. Manual CLI Testing
```bash
# Interactive mode
./build/dfs_cli

# Commands to try:
put /path/to/local/file.txt remote_file.txt
get remote_file.txt /path/to/download.txt
list
info remote_file.txt
delete remote_file.txt
quit

# Single command mode
./build/dfs_cli put test.txt dfs_test.txt
./build/dfs_cli get dfs_test.txt downloaded_test.txt
./build/dfs_cli list
./build/dfs_cli delete dfs_test.txt
```

## 📊 Performance Testing

### 1. Automated Benchmarks
```bash
# Run comprehensive benchmark
./scripts/benchmark.sh all

# Run specific benchmark types
./scripts/benchmark.sh upload
./scripts/benchmark.sh download
./scripts/benchmark.sh concurrent
./scripts/benchmark.sh fault
```

### 2. Manual Performance Testing
```bash
# Create test files
dd if=/dev/urandom of=test_1mb.dat bs=1M count=1
dd if=/dev/urandom of=test_10mb.dat bs=1M count=10

# Test upload performance
time ./build/dfs_cli put test_1mb.dat test_1mb.dat
time ./build/dfs_cli put test_10mb.dat test_10mb.dat --erasure-coding

# Test download performance
time ./build/dfs_cli get test_1mb.dat downloaded_1mb.dat
time ./build/dfs_cli get test_10mb.dat downloaded_10mb.dat

# Test with encryption
time ./build/dfs_cli put test_1mb.dat encrypted_test.dat --encrypt --password test123
time ./build/dfs_cli get encrypted_test.dat decrypted_test.dat --password test123
```

### 3. Concurrent Operations Test
```bash
# Create a script for concurrent testing
cat > concurrent_test.sh << 'EOF'
#!/bin/bash
for i in {1..10}; do
    ./build/dfs_cli put test_1mb.dat concurrent_file_$i.dat &
done
wait
echo "All uploads completed"

for i in {1..10}; do
    ./build/dfs_cli get concurrent_file_$i.dat download_$i.dat &
done
wait
echo "All downloads completed"
EOF

chmod +x concurrent_test.sh
./concurrent_test.sh
```

## 🛡️ Fault Tolerance Testing

### 1. Server Failure Simulation
```bash
# Upload a file with replication
./build/dfs_cli put important.txt replicated_file.txt --replication 3

# Simulate chunk server failure
docker-compose stop chunkserver1
# or kill the process manually

# Verify file is still accessible
./build/dfs_cli get replicated_file.txt recovered_file.txt

# Check if content is identical
diff important.txt recovered_file.txt
```

### 2. Network Partition Testing
```bash
# Using Docker network manipulation
docker network disconnect dfs_network dfs_chunkserver1_1

# Test if system continues operating
./build/dfs_cli put network_test.txt network_test.txt

# Reconnect and verify
docker network connect dfs_network dfs_chunkserver1_1
```

### 3. Data Corruption Testing
```bash
# Upload file with erasure coding
./build/dfs_cli put test.txt ec_test.txt --erasure-coding

# Manually corrupt a chunk file (simulate disk corruption)
# Find chunk files in data directories and modify them

# Verify system can recover
./build/dfs_cli get ec_test.txt recovered_ec_test.txt
diff test.txt recovered_ec_test.txt
```

## 🕸️ Web Dashboard Testing

### 1. Access Dashboard
```bash
# Open in browser
open http://localhost:8080

# Or test with curl
curl http://localhost:8080/
curl http://localhost:8080/api/stats
curl http://localhost:8080/api/files
curl http://localhost:8080/api/servers
```

### 2. Dashboard Functionality Tests
- Navigate to different pages (Files, Servers, Stats)
- Verify real-time updates
- Check file listings match CLI output
- Verify server health status
- Test auto-refresh functionality

## 🔍 Testing Specific Features

### Encryption Testing
```bash
# Test different encryption scenarios
./build/dfs_cli put secret.txt encrypted1.txt --encrypt --password pass123
./build/dfs_cli put secret.txt encrypted2.txt --encrypt --password different_pass

# Verify different passwords create different encrypted data
./build/dfs_cli get encrypted1.txt decrypted1.txt --password pass123
./build/dfs_cli get encrypted2.txt decrypted2.txt --password different_pass

# Test wrong password (should fail)
./build/dfs_cli get encrypted1.txt wrong_decrypt.txt --password wrong_pass
```

### Erasure Coding Testing
```bash
# Upload with erasure coding
./build/dfs_cli put large_file.dat ec_file.dat --erasure-coding

# Verify file integrity
./build/dfs_cli get ec_file.dat recovered_large.dat
sha256sum large_file.dat recovered_large.dat
```

### Replication Testing
```bash
# Test different replication factors
./build/dfs_cli put test.txt repl_2.txt --replication 2
./build/dfs_cli put test.txt repl_5.txt --replication 5

# Verify files are accessible
./build/dfs_cli get repl_2.txt test_repl_2.txt
./build/dfs_cli get repl_5.txt test_repl_5.txt
```

## 📈 Performance Monitoring

### 1. System Metrics
```bash
# Monitor during tests
top
iotop
nethogs

# Check disk usage
df -h

# Monitor network traffic
ss -tuln
```

### 2. Application Metrics
```bash
# Check web dashboard for metrics
curl http://localhost:8080/api/stats | jq

# Check server status
./build/dfs_cli status
./build/dfs_cli servers
```

## 🧹 Cleanup and Reset

### Reset Test Environment
```bash
# Stop all services
./scripts/stop_cluster.sh

# or with Docker
docker-compose down

# Clean test data
rm -rf data/
rm -rf test_data/
rm -rf benchmark_results/

# Clean downloaded files
rm -f *.dat *.txt test_* downloaded_* concurrent_* recovered_*
```

## 🚨 Troubleshooting Tests

### Common Issues and Solutions

1. **Test fails with "Connection refused"**
   ```bash
   # Check if services are running
   ps aux | grep -E "(master_server|chunk_server)"
   netstat -tlnp | grep -E "(50051|60051|60052|60053)"
   
   # Restart cluster
   ./scripts/stop_cluster.sh
   ./scripts/start_cluster.sh
   ```

2. **Permission denied errors**
   ```bash
   # Fix permissions
   chmod +x build/*
   chmod +x scripts/*
   
   # Fix data directory permissions
   chmod -R 755 data/
   ```

3. **Out of disk space**
   ```bash
   # Clean up test files
   rm -rf test_data/ benchmark_results/
   
   # Check disk usage
   df -h
   ```

4. **Port already in use**
   ```bash
   # Find and kill processes using ports
   lsof -ti:50051 | xargs kill -9
   lsof -ti:60051 | xargs kill -9
   ```

## ✅ Test Verification Checklist

### Basic Functionality
- [ ] Upload files successfully
- [ ] Download files with correct content
- [ ] List files shows uploaded files
- [ ] Delete files removes them
- [ ] File info shows correct metadata

### Advanced Features
- [ ] Encryption works with correct passwords
- [ ] Encryption fails with wrong passwords
- [ ] Erasure coding preserves data integrity
- [ ] Replication survives server failures
- [ ] Load balancing distributes chunks

### Performance
- [ ] Concurrent operations complete successfully
- [ ] Large files upload/download correctly
- [ ] Performance meets expectations
- [ ] System handles load gracefully

### Web Dashboard
- [ ] Dashboard loads and displays data
- [ ] Real-time updates work
- [ ] API endpoints return valid data
- [ ] Navigation works correctly

### Fault Tolerance
- [ ] System survives single server failure
- [ ] Data recovery works correctly
- [ ] Corrupted chunks are detected
- [ ] System rebalances after failures

This comprehensive testing approach ensures the DFS system works correctly under all conditions!