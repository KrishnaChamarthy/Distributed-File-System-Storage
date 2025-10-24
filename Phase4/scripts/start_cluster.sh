#!/bin/bash

# DFS Cluster Startup Script
# Usage: ./scripts/start_cluster.sh [num_chunk_servers]

set -e

# Configuration
MASTER_PORT=50051
WEB_PORT=8080
CHUNK_SERVER_BASE_PORT=60051
NUM_CHUNK_SERVERS=${1:-3}
BUILD_DIR="build"
DATA_DIR="data"
CONFIG_DIR="config"

echo "Starting DFS Cluster with $NUM_CHUNK_SERVERS chunk servers..."

# Create necessary directories
mkdir -p $DATA_DIR/{master,web}
for ((i=1; i<=NUM_CHUNK_SERVERS; i++)); do
    mkdir -p $DATA_DIR/chunk_server_$i
done

# Start master server
echo "Starting master server on port $MASTER_PORT..."
$BUILD_DIR/master_server \
    --config=$CONFIG_DIR/master.json \
    --port=$MASTER_PORT \
    --data-dir=$DATA_DIR/master &
MASTER_PID=$!
echo "Master server started with PID $MASTER_PID"

# Wait for master to start
sleep 2

# Start web dashboard
echo "Starting web dashboard on port $WEB_PORT..."
$BUILD_DIR/web_server $WEB_PORT &
WEB_PID=$!
echo "Web dashboard started with PID $WEB_PID"

# Start chunk servers
CHUNK_PIDS=()
for ((i=1; i<=NUM_CHUNK_SERVERS; i++)); do
    PORT=$((CHUNK_SERVER_BASE_PORT + i - 1))
    SERVER_ID="chunk-server-$i"
    
    echo "Starting chunk server $SERVER_ID on port $PORT..."
    $BUILD_DIR/chunk_server \
        --config=$CONFIG_DIR/chunkserver.json \
        --server-id=$SERVER_ID \
        --port=$PORT \
        --master-address=localhost:$MASTER_PORT \
        --data-dir=$DATA_DIR/chunk_server_$i &
    
    CHUNK_PID=$!
    CHUNK_PIDS+=($CHUNK_PID)
    echo "Chunk server $SERVER_ID started with PID $CHUNK_PID"
    
    # Small delay between chunk server starts
    sleep 1
done

# Save PIDs to file for easy cleanup
PID_FILE="$DATA_DIR/cluster.pids"
echo "master:$MASTER_PID" > $PID_FILE
echo "web:$WEB_PID" >> $PID_FILE
for ((i=0; i<${#CHUNK_PIDS[@]}; i++)); do
    echo "chunk$((i+1)):${CHUNK_PIDS[$i]}" >> $PID_FILE
done

echo ""
echo "✅ DFS Cluster started successfully!"
echo ""
echo "Services:"
echo "  Master Server:   localhost:$MASTER_PORT"
echo "  Web Dashboard:   http://localhost:$WEB_PORT"
echo "  Chunk Servers:   ports $CHUNK_SERVER_BASE_PORT-$((CHUNK_SERVER_BASE_PORT + NUM_CHUNK_SERVERS - 1))"
echo ""
echo "Usage:"
echo "  CLI: $BUILD_DIR/dfs_cli"
echo "  Web: http://localhost:$WEB_PORT"
echo ""
echo "To stop the cluster: ./scripts/stop_cluster.sh"
echo "PIDs saved to: $PID_FILE"