#!/bin/bash

echo "🛑 Stopping DFS Server..."
cd /Users/krishnachamarthy/Documents/GitHub/Distributed-File-System-Storage/Phase4

if [ -f .server_pid ]; then
    SERVER_PID=$(cat .server_pid)
    if kill $SERVER_PID 2>/dev/null; then
        echo "✅ DFS Server stopped (PID: $SERVER_PID)"
    else
        echo "⚠️  Server PID $SERVER_PID not found, trying general cleanup..."
    fi
    rm -f .server_pid
fi

# Fallback: kill any remaining dfs_server processes
pkill -f "dfs_server" 2>/dev/null
pkill -f "dfs_cli" 2>/dev/null

echo "🧹 Cleanup completed"