#!/bin/bash

echo "🚀 Starting DFS Server in background..."
cd /Users/krishnachamarthy/Documents/GitHub/Distributed-File-System-Storage/Phase4

# Check if server is already running
if pgrep -f "dfs_server" > /dev/null; then
    echo "⚠️  DFS Server is already running"
    echo "Use 'pkill -f dfs_server' to stop it first"
    exit 1
fi

# Start server in background
./dfs_server &
SERVER_PID=$!

echo "✅ DFS Server started with PID: $SERVER_PID"
echo "📝 Server PID saved to .server_pid"
echo $SERVER_PID > .server_pid

echo ""
echo "🎯 Now you can use the client:"
echo "============================="
echo "Upload files:"
echo "  ./dfs_client put demo_files/test1.txt"
echo "  ./dfs_client put demo_files/test2.txt /dfs/my_doc.txt"
echo ""
echo "List files:"
echo "  ./dfs_client ls"
echo ""
echo "Download files:"
echo "  ./dfs_client get /dfs/test1.txt"
echo ""
echo "Check status:"
echo "  ./dfs_client status"
echo ""
echo "Stop server:"
echo "  ./stop_server.sh"