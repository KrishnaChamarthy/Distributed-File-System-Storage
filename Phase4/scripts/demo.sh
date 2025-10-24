#!/bin/bash

# DFS Interactive Demo Script
# This script provides a guided demo of the Distributed File System

set -e

echo "🚀 DFS Phase 4 - Interactive Demo"
echo "================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BUILD_DIR="build"
DATA_DIR="data"
DEMO_DIR="demo_files"

# Check if build exists
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}❌ Build directory not found. Please run:${NC}"
    echo "   mkdir build && cd build && cmake .. && make && cd .."
    exit 1
fi

# Check if binaries exist
BINARIES=("master_server" "chunk_server" "dfs_cli" "web_server")
for binary in "${BINARIES[@]}"; do
    if [ ! -f "$BUILD_DIR/$binary" ]; then
        echo -e "${RED}❌ $binary not found. Please build the project first.${NC}"
        exit 1
    fi
done

echo -e "${GREEN}✅ All binaries found!${NC}"

# Create demo directory
mkdir -p $DEMO_DIR

# Function to wait for user input
wait_for_user() {
    echo -e "${YELLOW}Press Enter to continue...${NC}"
    read
}

# Function to check if process is running
is_running() {
    ps aux | grep -v grep | grep "$1" > /dev/null
}

# Function to start services
start_cluster() {
    echo -e "${BLUE}🏗️  Starting DFS Cluster...${NC}"
    
    # Create data directories
    mkdir -p $DATA_DIR/{master,chunk1,chunk2,chunk3}
    
    # Start master server
    echo "Starting master server..."
    $BUILD_DIR/master_server > $DATA_DIR/master.log 2>&1 &
    MASTER_PID=$!
    echo "Master PID: $MASTER_PID"
    
    # Wait for master to start
    sleep 2
    
    # Start web server
    echo "Starting web dashboard..."
    $BUILD_DIR/web_server 8080 > $DATA_DIR/web.log 2>&1 &
    WEB_PID=$!
    echo "Web PID: $WEB_PID"
    
    # Start chunk servers
    echo "Starting chunk servers..."
    $BUILD_DIR/chunk_server --server-id=chunk1 --port=60051 --data-dir=$DATA_DIR/chunk1 > $DATA_DIR/chunk1.log 2>&1 &
    CHUNK1_PID=$!
    
    $BUILD_DIR/chunk_server --server-id=chunk2 --port=60052 --data-dir=$DATA_DIR/chunk2 > $DATA_DIR/chunk2.log 2>&1 &
    CHUNK2_PID=$!
    
    $BUILD_DIR/chunk_server --server-id=chunk3 --port=60053 --data-dir=$DATA_DIR/chunk3 > $DATA_DIR/chunk3.log 2>&1 &
    CHUNK3_PID=$!
    
    echo "Chunk server PIDs: $CHUNK1_PID, $CHUNK2_PID, $CHUNK3_PID"
    
    # Save PIDs for cleanup
    echo "$MASTER_PID $WEB_PID $CHUNK1_PID $CHUNK2_PID $CHUNK3_PID" > $DATA_DIR/pids.txt
    
    # Wait for services to start
    echo "Waiting for services to initialize..."
    sleep 3
    
    echo -e "${GREEN}✅ Cluster started!${NC}"
    echo ""
    echo "Services running:"
    echo "  📊 Web Dashboard: http://localhost:8080"
    echo "  🖥️  Master Server: localhost:50051"
    echo "  💾 Chunk Servers: localhost:60051, 60052, 60053"
    echo ""
}

# Function to stop services
stop_cluster() {
    echo -e "${BLUE}🛑 Stopping DFS Cluster...${NC}"
    
    if [ -f "$DATA_DIR/pids.txt" ]; then
        PIDS=$(cat $DATA_DIR/pids.txt)
        for pid in $PIDS; do
            if kill -0 $pid 2>/dev/null; then
                echo "Stopping process $pid..."
                kill $pid
            fi
        done
        rm -f $DATA_DIR/pids.txt
    else
        # Fallback: kill by name
        pkill -f master_server || true
        pkill -f chunk_server || true
        pkill -f web_server || true
    fi
    
    echo -e "${GREEN}✅ Cluster stopped!${NC}"
}

# Function to create demo files
create_demo_files() {
    echo -e "${BLUE}📝 Creating demo files...${NC}"
    
    echo "Hello, DFS World!" > $DEMO_DIR/hello.txt
    echo "This is a sample document for the Distributed File System demo." > $DEMO_DIR/document.txt
    
    # Create a larger text file
    cat > $DEMO_DIR/large_text.txt << 'EOF'
This is a larger text file to demonstrate the DFS capabilities.
It contains multiple lines and paragraphs to show how the system
handles files of different sizes.

The Distributed File System (DFS) provides:
- Fault tolerance through replication
- Data encryption for security
- Erasure coding for storage efficiency
- Load balancing across servers
- Real-time monitoring via web dashboard

This file will be split into chunks and distributed across
multiple chunk servers in the cluster.
EOF
    
    # Create a binary file
    dd if=/dev/urandom of=$DEMO_DIR/binary_data.dat bs=1024 count=50 2>/dev/null
    
    # Create a JSON file
    cat > $DEMO_DIR/config.json << 'EOF'
{
    "name": "DFS Demo Configuration",
    "version": "4.0",
    "features": [
        "replication",
        "encryption",
        "erasure_coding",
        "load_balancing",
        "web_dashboard"
    ],
    "servers": {
        "master": "localhost:50051",
        "chunks": [
            "localhost:60051",
            "localhost:60052", 
            "localhost:60053"
        ]
    }
}
EOF
    
    echo -e "${GREEN}✅ Demo files created in $DEMO_DIR/${NC}"
    ls -la $DEMO_DIR/
}

# Function to demonstrate basic operations
demo_basic_operations() {
    echo -e "${BLUE}🔄 Demo: Basic File Operations${NC}"
    echo ""
    
    echo "1. Uploading files..."
    $BUILD_DIR/dfs_cli put $DEMO_DIR/hello.txt /dfs/hello.txt
    $BUILD_DIR/dfs_cli put $DEMO_DIR/document.txt /dfs/document.txt
    $BUILD_DIR/dfs_cli put $DEMO_DIR/config.json /dfs/config.json
    
    echo ""
    echo "2. Listing files in DFS..."
    $BUILD_DIR/dfs_cli list
    
    echo ""
    echo "3. Getting file information..."
    $BUILD_DIR/dfs_cli info /dfs/hello.txt
    
    echo ""
    echo "4. Downloading files..."
    $BUILD_DIR/dfs_cli get /dfs/hello.txt $DEMO_DIR/downloaded_hello.txt
    $BUILD_DIR/dfs_cli get /dfs/document.txt $DEMO_DIR/downloaded_document.txt
    
    echo ""
    echo "5. Verifying file integrity..."
    if diff $DEMO_DIR/hello.txt $DEMO_DIR/downloaded_hello.txt > /dev/null; then
        echo -e "${GREEN}✅ hello.txt integrity verified${NC}"
    else
        echo -e "${RED}❌ hello.txt integrity check failed${NC}"
    fi
    
    if diff $DEMO_DIR/document.txt $DEMO_DIR/downloaded_document.txt > /dev/null; then
        echo -e "${GREEN}✅ document.txt integrity verified${NC}"
    else
        echo -e "${RED}❌ document.txt integrity check failed${NC}"
    fi
    
    echo ""
    wait_for_user
}

# Function to demonstrate encryption
demo_encryption() {
    echo -e "${BLUE}🔐 Demo: Encryption Features${NC}"
    echo ""
    
    echo "1. Uploading file with encryption..."
    $BUILD_DIR/dfs_cli put $DEMO_DIR/hello.txt /dfs/encrypted_hello.txt --encrypt --password demo123
    
    echo ""
    echo "2. Attempting download with wrong password (should fail)..."
    if $BUILD_DIR/dfs_cli get /dfs/encrypted_hello.txt $DEMO_DIR/wrong_password.txt --password wrongpass 2>/dev/null; then
        echo -e "${RED}❌ Security issue: Wrong password accepted${NC}"
    else
        echo -e "${GREEN}✅ Correctly rejected wrong password${NC}"
    fi
    
    echo ""
    echo "3. Downloading with correct password..."
    $BUILD_DIR/dfs_cli get /dfs/encrypted_hello.txt $DEMO_DIR/decrypted_hello.txt --password demo123
    
    echo ""
    echo "4. Verifying decrypted content..."
    if diff $DEMO_DIR/hello.txt $DEMO_DIR/decrypted_hello.txt > /dev/null; then
        echo -e "${GREEN}✅ Encryption/decryption successful${NC}"
    else
        echo -e "${RED}❌ Encryption/decryption failed${NC}"
    fi
    
    echo ""
    wait_for_user
}

# Function to demonstrate replication
demo_replication() {
    echo -e "${BLUE}🔄 Demo: Replication Features${NC}"
    echo ""
    
    echo "1. Uploading with different replication factors..."
    $BUILD_DIR/dfs_cli put $DEMO_DIR/document.txt /dfs/repl_2.txt --replication 2
    $BUILD_DIR/dfs_cli put $DEMO_DIR/document.txt /dfs/repl_3.txt --replication 3
    
    echo ""
    echo "2. Checking file info to see replication..."
    $BUILD_DIR/dfs_cli info /dfs/repl_2.txt
    echo ""
    $BUILD_DIR/dfs_cli info /dfs/repl_3.txt
    
    echo ""
    echo "3. Files are now replicated across multiple chunk servers"
    echo "   This provides fault tolerance - if one server fails,"
    echo "   the data is still available from other servers."
    
    echo ""
    wait_for_user
}

# Function to demonstrate erasure coding
demo_erasure_coding() {
    echo -e "${BLUE}🧮 Demo: Erasure Coding${NC}"
    echo ""
    
    echo "1. Uploading larger file with erasure coding..."
    $BUILD_DIR/dfs_cli put $DEMO_DIR/binary_data.dat /dfs/ec_binary.dat --erasure-coding
    
    echo ""
    echo "2. Erasure coding provides fault tolerance with less storage overhead"
    echo "   than replication. It splits data into chunks with parity information."
    
    echo ""
    echo "3. Downloading and verifying erasure coded file..."
    $BUILD_DIR/dfs_cli get /dfs/ec_binary.dat $DEMO_DIR/recovered_binary.dat
    
    if diff $DEMO_DIR/binary_data.dat $DEMO_DIR/recovered_binary.dat > /dev/null; then
        echo -e "${GREEN}✅ Erasure coding successful${NC}"
    else
        echo -e "${RED}❌ Erasure coding failed${NC}"
    fi
    
    echo ""
    wait_for_user
}

# Function to demonstrate web dashboard
demo_web_dashboard() {
    echo -e "${BLUE}🌐 Demo: Web Dashboard${NC}"
    echo ""
    
    echo "The web dashboard provides real-time monitoring of your DFS cluster."
    echo ""
    echo "🌍 Opening web dashboard at http://localhost:8080"
    echo ""
    echo "In the dashboard you can:"
    echo "  📊 View system statistics"
    echo "  📁 Browse uploaded files"
    echo "  🖥️  Monitor server health"
    echo "  📈 Check performance metrics"
    echo ""
    
    # Try to open browser (works on macOS and many Linux systems)
    if command -v open >/dev/null 2>&1; then
        open http://localhost:8080
    elif command -v xdg-open >/dev/null 2>&1; then
        xdg-open http://localhost:8080
    else
        echo "Please manually open: http://localhost:8080"
    fi
    
    echo ""
    echo "You can also test the API endpoints:"
    echo "  curl http://localhost:8080/api/stats"
    echo "  curl http://localhost:8080/api/files"
    echo "  curl http://localhost:8080/api/servers"
    
    echo ""
    wait_for_user
}

# Function to demonstrate fault tolerance
demo_fault_tolerance() {
    echo -e "${BLUE}🛡️  Demo: Fault Tolerance${NC}"
    echo ""
    
    echo "1. Uploading file with high replication..."
    $BUILD_DIR/dfs_cli put $DEMO_DIR/large_text.txt /dfs/fault_test.txt --replication 3
    
    echo ""
    echo "2. Current chunk servers:"
    ps aux | grep chunk_server | grep -v grep
    
    echo ""
    echo "3. Simulating chunk server failure..."
    # Kill one chunk server
    PIDS=$(cat $DATA_DIR/pids.txt)
    CHUNK1_PID=$(echo $PIDS | cut -d' ' -f3)
    
    if kill -0 $CHUNK1_PID 2>/dev/null; then
        kill $CHUNK1_PID
        echo "Killed chunk server 1 (PID: $CHUNK1_PID)"
    fi
    
    echo ""
    echo "4. Attempting to download file after server failure..."
    if $BUILD_DIR/dfs_cli get /dfs/fault_test.txt $DEMO_DIR/recovered_fault.txt; then
        if diff $DEMO_DIR/large_text.txt $DEMO_DIR/recovered_fault.txt > /dev/null; then
            echo -e "${GREEN}✅ Fault tolerance successful! File recovered despite server failure.${NC}"
        else
            echo -e "${RED}❌ File corrupted during recovery${NC}"
        fi
    else
        echo -e "${RED}❌ Could not recover file after server failure${NC}"
    fi
    
    echo ""
    echo "5. This demonstrates how replication ensures data availability"
    echo "   even when chunk servers fail."
    
    echo ""
    wait_for_user
}

# Function to clean up demo files
cleanup_demo() {
    echo -e "${BLUE}🧹 Cleaning up demo...${NC}"
    
    # Clean up demo files
    rm -rf $DEMO_DIR/
    
    # Clean up data directory
    rm -rf $DATA_DIR/
    
    echo -e "${GREEN}✅ Cleanup complete!${NC}"
}

# Main demo flow
main() {
    echo "This demo will showcase the key features of the DFS system:"
    echo "  🔄 File upload/download operations"
    echo "  🔐 Encryption and security"
    echo "  🔄 Replication for fault tolerance"
    echo "  🧮 Erasure coding for efficiency"
    echo "  🌐 Web dashboard monitoring"
    echo "  🛡️  Fault tolerance testing"
    echo ""
    
    wait_for_user
    
    # Cleanup any previous runs
    stop_cluster 2>/dev/null || true
    sleep 1
    
    # Start the demo
    start_cluster
    create_demo_files
    
    demo_basic_operations
    demo_encryption
    demo_replication
    demo_erasure_coding
    demo_web_dashboard
    demo_fault_tolerance
    
    echo -e "${GREEN}🎉 Demo completed successfully!${NC}"
    echo ""
    echo "The DFS system demonstrated:"
    echo "  ✅ Reliable file storage and retrieval"
    echo "  ✅ Strong encryption capabilities"
    echo "  ✅ Fault tolerance through replication"
    echo "  ✅ Storage efficiency with erasure coding"
    echo "  ✅ Real-time monitoring via web dashboard"
    echo "  ✅ Graceful handling of server failures"
    echo ""
    
    echo -e "${YELLOW}Would you like to keep the cluster running for further exploration? (y/n)${NC}"
    read -r response
    
    if [[ "$response" =~ ^[Yy]$ ]]; then
        echo ""
        echo -e "${GREEN}Cluster is still running!${NC}"
        echo "  🌍 Web Dashboard: http://localhost:8080"
        echo "  💻 CLI: ./build/dfs_cli"
        echo "  📊 Stop cluster: ./scripts/stop_cluster.sh"
        echo ""
        echo "Explore the system and have fun! 🚀"
    else
        stop_cluster
        cleanup_demo
        echo ""
        echo "Thank you for trying the DFS system! 👋"
    fi
}

# Trap Ctrl+C to clean up
trap 'echo -e "\n${YELLOW}Demo interrupted. Cleaning up...${NC}"; stop_cluster; cleanup_demo; exit' INT

# Run the main demo
main