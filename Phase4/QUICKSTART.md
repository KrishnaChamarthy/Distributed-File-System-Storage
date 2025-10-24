# 🚀 Quick Start Guide - DFS Phase 4

This guide will help you quickly get the Distributed File System up and running for demonstration and testing.

## 📋 Prerequisites

### macOS (Homebrew)
```bash
# Install dependencies
brew install cmake protobuf grpc openssl jsoncpp

# Optional: Install Google Test for unit testing
brew install googletest
```

### Ubuntu/Debian
```bash
# Install dependencies
sudo apt-get update
sudo apt-get install -y \
    build-essential cmake pkg-config \
    libssl-dev libprotobuf-dev protobuf-compiler \
    libgrpc++-dev libgrpc-dev protobuf-compiler-grpc \
    libjsoncpp-dev

# Optional: Install Google Test
sudo apt-get install -y libgtest-dev libgmock-dev
```

## 🏗️ Build the Project

```bash
# Navigate to Phase4 directory
cd Phase4

# Create build directory
mkdir -p build && cd build

# Configure with CMake
cmake ..

# Build the project
make -j$(nproc)

# Go back to project root
cd ..
```

## 🎯 Quick Demo (3 Minutes)

### Step 1: Start the Cluster
```bash
# Terminal 1: Start Master Server
./build/master_server &
MASTER_PID=$!

# Wait a moment for master to start
sleep 2

# Terminal 2: Start Web Dashboard
./build/web_server 8080 &
WEB_PID=$!

# Terminal 3: Start Chunk Servers
./build/chunk_server --server-id=chunk1 --port=60051 &
CHUNK1_PID=$!

./build/chunk_server --server-id=chunk2 --port=60052 &
CHUNK2_PID=$!

./build/chunk_server --server-id=chunk3 --port=60053 &
CHUNK3_PID=$!

echo "Cluster started! PIDs: Master=$MASTER_PID Web=$WEB_PID Chunks=$CHUNK1_PID,$CHUNK2_PID,$CHUNK3_PID"
```

### Step 2: Create Test Files
```bash
# Create some test files
echo "Hello, DFS World!" > test1.txt
echo "This is a larger test file with more content to demonstrate the distributed file system capabilities." > test2.txt
dd if=/dev/urandom of=binary_test.dat bs=1024 count=100 2>/dev/null  # 100KB binary file
```

### Step 3: Use the CLI
```bash
# Upload files
./build/dfs_cli put test1.txt /dfs/hello.txt
./build/dfs_cli put test2.txt /dfs/large_text.txt
./build/dfs_cli put binary_test.dat /dfs/binary_file.dat

# List files
./build/dfs_cli list

# Download files
./build/dfs_cli get /dfs/hello.txt downloaded_hello.txt
./build/dfs_cli get /dfs/large_text.txt downloaded_large.txt

# Verify content
diff test1.txt downloaded_hello.txt
diff test2.txt downloaded_large.txt

# Upload with encryption
./build/dfs_cli put test1.txt /dfs/encrypted_hello.txt --encrypt --password mysecret

# Download with decryption
./build/dfs_cli get /dfs/encrypted_hello.txt decrypted_hello.txt --password mysecret
diff test1.txt decrypted_hello.txt

# Get file info
./build/dfs_cli info /dfs/hello.txt

# Delete files
./build/dfs_cli delete /dfs/hello.txt
```

### Step 4: Check Web Dashboard
```bash
# Open web dashboard
open http://localhost:8080
# or visit http://localhost:8080 in your browser

# You should see:
# - System statistics
# - File listings
# - Server health status
# - Real-time metrics
```

### Step 5: Clean Up
```bash
# Stop all processes
kill $MASTER_PID $WEB_PID $CHUNK1_PID $CHUNK2_PID $CHUNK3_PID

# Clean up test files
rm -f test*.txt binary_test.dat downloaded_*.txt decrypted_*.txt

# Clean up data directories
rm -rf data/
```

## 🔧 Alternative: Using Scripts

### Start Cluster (Easy Way)
```bash
# Make scripts executable
chmod +x scripts/*.sh

# Start cluster with 3 chunk servers
./scripts/start_cluster.sh 3

# Check if everything is running
ps aux | grep -E "(master_server|chunk_server|web_server)"
```

### Demo Script
```bash
# Run the interactive demo
./scripts/demo.sh
```

### Stop Cluster
```bash
./scripts/stop_cluster.sh
```

## 🐳 Docker Quick Start

### Using Docker Compose
```bash
# Start entire cluster
docker-compose up -d

# Check status
docker-compose ps

# Access CLI
docker-compose exec client bash
./bin/dfs_cli

# View logs
docker-compose logs master
docker-compose logs chunkserver1

# Stop cluster
docker-compose down
```

## 🧪 Testing Different Features

### Test Replication
```bash
# Upload with different replication factors
./build/dfs_cli put test1.txt /dfs/repl2.txt --replication 2
./build/dfs_cli put test1.txt /dfs/repl5.txt --replication 5
```

### Test Encryption
```bash
# Upload encrypted files
./build/dfs_cli put test1.txt /dfs/secret1.txt --encrypt --password pass123
./build/dfs_cli put test2.txt /dfs/secret2.txt --encrypt --password different_pass

# Download with correct passwords
./build/dfs_cli get /dfs/secret1.txt decrypted1.txt --password pass123
./build/dfs_cli get /dfs/secret2.txt decrypted2.txt --password different_pass
```

### Test Erasure Coding
```bash
# Create larger file for erasure coding
dd if=/dev/urandom of=large_file.dat bs=1M count=50  # 50MB

# Upload with erasure coding
./build/dfs_cli put large_file.dat /dfs/ec_file.dat --erasure-coding

# Download and verify
./build/dfs_cli get /dfs/ec_file.dat recovered_large.dat
diff large_file.dat recovered_large.dat
```

### Test Fault Tolerance
```bash
# Upload a file
./build/dfs_cli put test1.txt /dfs/fault_test.txt --replication 3

# Kill one chunk server
kill $CHUNK1_PID

# File should still be downloadable
./build/dfs_cli get /dfs/fault_test.txt recovered_fault.txt
diff test1.txt recovered_fault.txt
```

## 📊 Monitoring and Debugging

### Check System Status
```bash
# CLI status
./build/dfs_cli status
./build/dfs_cli servers

# Web API
curl http://localhost:8080/api/stats
curl http://localhost:8080/api/files
curl http://localhost:8080/api/servers
```

### Check Logs
```bash
# If running manually, check terminal output
# If using Docker:
docker-compose logs -f master
docker-compose logs -f chunkserver1
```

### Check Network Connectivity
```bash
# Check if ports are open
netstat -tlnp | grep -E "(50051|60051|60052|60053|8080)"

# Test gRPC connectivity
curl -v http://localhost:50051  # Should connect but fail (gRPC vs HTTP)
```

## 🚨 Troubleshooting

### Build Issues
```bash
# If protobuf not found
brew install protobuf  # macOS
sudo apt install libprotobuf-dev  # Ubuntu

# If gRPC not found
brew install grpc  # macOS
sudo apt install libgrpc++-dev  # Ubuntu

# Clean build
rm -rf build/
mkdir build && cd build
cmake .. && make
```

### Runtime Issues
```bash
# If "Connection refused"
# Make sure master server is running first
ps aux | grep master_server

# If "Port already in use"
lsof -ti:50051 | xargs kill -9

# If permission denied
chmod +x build/*
```

### Data Issues
```bash
# Clean all data
rm -rf data/

# Reset to fresh state
./scripts/stop_cluster.sh
rm -rf data/
./scripts/start_cluster.sh
```

## 🎉 What to Expect

After following this guide, you should have:

✅ **Working DFS cluster** with master and 3 chunk servers  
✅ **Web dashboard** at http://localhost:8080  
✅ **CLI interface** for file operations  
✅ **File upload/download** working correctly  
✅ **Encryption** protecting sensitive data  
✅ **Replication** ensuring fault tolerance  
✅ **Real-time monitoring** via web interface  

This demonstrates a fully functional distributed file system with enterprise-grade features!

## 🔗 Next Steps

- Explore the web dashboard features
- Try larger files and stress testing
- Experiment with different configurations
- Set up monitoring and alerting
- Scale to more chunk servers
- Deploy in a distributed environment

Happy exploring! 🚀