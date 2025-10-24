#!/bin/bash

# DFS Cluster Shutdown Script
# Usage: ./scripts/stop_cluster.sh

set -e

DATA_DIR="data"
PID_FILE="$DATA_DIR/cluster.pids"

echo "Stopping DFS Cluster..."

if [ ! -f "$PID_FILE" ]; then
    echo "❌ PID file not found: $PID_FILE"
    echo "Cluster may not be running or was started manually."
    exit 1
fi

# Read PIDs and stop processes
while IFS=':' read -r service pid; do
    if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
        echo "Stopping $service (PID: $pid)..."
        kill -TERM "$pid"
        
        # Wait for graceful shutdown
        timeout=10
        while [ $timeout -gt 0 ] && kill -0 "$pid" 2>/dev/null; do
            sleep 1
            ((timeout--))
        done
        
        # Force kill if still running
        if kill -0 "$pid" 2>/dev/null; then
            echo "Force killing $service (PID: $pid)..."
            kill -KILL "$pid"
        fi
        
        echo "✅ $service stopped"
    else
        echo "⚠️  $service (PID: $pid) not running"
    fi
done < "$PID_FILE"

# Clean up PID file
rm -f "$PID_FILE"

echo ""
echo "✅ DFS Cluster stopped successfully!"
echo ""
echo "Data directories preserved in: $DATA_DIR/"
echo "To clean up data: ./scripts/clean_data.sh"