#!/bin/bash

echo "🌐 DFS Web Dashboard Demo"
echo "========================="
echo ""

cd /Users/krishnachamarthy/Documents/GitHub/Distributed-File-System-Storage/Phase4

echo "📋 This demo simulates the web interface functionality"
echo "In a full implementation, you would access these via browser:"
echo ""

# Check if we can build the full system
if [ -d "build" ] && [ -f "build/master_server" ]; then
    echo "🎯 Full Web Server Available!"
    echo "================================"
    echo ""
    echo "1. Start the full web server:"
    echo "   cd build/"
    echo "   ./master_server --web-port 8080 &"
    echo "   ./web_server 8080 &"
    echo ""
    echo "2. Access web dashboard:"
    echo "   open http://localhost:8080"
    echo ""
    echo "3. Available web pages:"
    echo "   http://localhost:8080           - Main dashboard"
    echo "   http://localhost:8080/files     - File browser"
    echo "   http://localhost:8080/servers   - Server status"
    echo "   http://localhost:8080/stats     - System statistics"
    echo ""
    echo "4. API endpoints:"
    echo "   curl http://localhost:8080/api/stats"
    echo "   curl http://localhost:8080/api/files"
    echo "   curl http://localhost:8080/api/servers"
    echo ""
else
    echo "💡 Web Interface Simulation"
    echo "============================"
    echo ""
    echo "Since the full web server requires building, here's how"
    echo "the web interface functionality maps to our CLI:"
    echo ""
    
    echo "🖥️ 1. MAIN DASHBOARD (http://localhost:8080)"
    echo "CLI Equivalent: ./dfs_cli status"
    echo "Simulating dashboard..."
    ./dfs_cli status 2>/dev/null | head -15
    
    echo ""
    echo "📁 2. FILE BROWSER (http://localhost:8080/files)"
    echo "CLI Equivalent: ./dfs_cli ls"
    echo "Simulating file browser..."
    ./dfs_cli ls 2>/dev/null | head -15
    
    echo ""
    echo "🖥️ 3. SERVER STATUS (http://localhost:8080/servers)"
    echo "CLI Equivalent: Server simulation in CLI startup"
    echo "Simulating server status page..."
    echo "🖥️ ChunkServer-1 (Port: 60051) - RUNNING ✅ | 75% disk | 234ms avg"
    echo "🖥️ ChunkServer-2 (Port: 60052) - RUNNING ✅ | 82% disk | 189ms avg"
    echo "🖥️ ChunkServer-3 (Port: 60053) - RUNNING ✅ | 68% disk | 267ms avg"
    echo ""
    echo "📊 Cluster Health: HEALTHY"
    echo "🔄 Replication Status: ALL_REPLICAS_HEALTHY"
    echo "💾 Total Storage: 2.4 GB used / 45.8 GB available"
    
    echo ""
    echo "📊 4. STATISTICS API (http://localhost:8080/api/stats)"
    echo "Simulating JSON API response..."
    cat << 'EOF'
{
  "status": "healthy",
  "timestamp": "2025-10-25T00:15:23Z",
  "cluster": {
    "files_total": 3,
    "storage_used_gb": 2.4,
    "storage_available_gb": 45.8,
    "servers_online": 3,
    "servers_total": 3
  },
  "performance": {
    "upload_rate_mbps": 67.2,
    "download_rate_mbps": 89.4,
    "requests_per_minute": 234,
    "avg_response_time_ms": 230
  },
  "servers": [
    {"id": "chunk-server-1", "status": "online", "disk_usage": 75, "response_time": 234},
    {"id": "chunk-server-2", "status": "online", "disk_usage": 82, "response_time": 189},
    {"id": "chunk-server-3", "status": "online", "disk_usage": 68, "response_time": 267}
  ]
}
EOF
fi

echo ""
echo "🎯 Web Interface Features Available:"
echo "===================================="
echo "✅ Real-time system monitoring"
echo "✅ File management and browsing"
echo "✅ Server status and health checks"
echo "✅ Performance metrics and graphs"
echo "✅ REST API for automation"
echo "✅ Responsive web design"
echo "✅ File upload/download via browser"
echo "✅ JSON API endpoints"

echo ""
echo "🔗 How to Access Web Interface:"
echo "==============================="
echo ""
echo "Option 1: Build Full System"
echo "  mkdir build && cd build"
echo "  cmake .. && make"
echo "  ./master_server &"
echo "  open http://localhost:8080"
echo ""
echo "Option 2: Docker Deployment"
echo "  docker-compose up -d"
echo "  open http://localhost:8080"
echo ""
echo "Option 3: CLI Simulation (Current)"
echo "  ./dfs_cli status     # Dashboard info"
echo "  ./dfs_cli ls         # File browser"
echo "  ./dfs_cli help       # Available commands"

echo ""
echo "📖 For detailed web usage instructions:"
echo "   cat WEB_USAGE_GUIDE.md"

echo ""
echo "🌐 Web dashboard provides the same functionality as CLI"
echo "   but with a beautiful, interactive web interface!"