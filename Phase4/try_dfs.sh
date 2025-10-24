#!/bin/bash

# Simple DFS Try Script - Direct Steps
echo "🚀 DFS Phase 4 - Direct Demo Steps"
echo "=================================="

# Create demo directory
mkdir -p demo_files
cd demo_files

# Create test files
echo "Hello DFS World!" > test1.txt
echo "This is a larger test file with more content for demonstration." > test2.txt
dd if=/dev/urandom of=binary.dat bs=1024 count=10 2>/dev/null

echo "✅ Demo files created:"
ls -la

echo ""
echo "🎯 To try the DFS system:"
echo ""

echo "1. Manual Build & Run:"
echo "   cd ../build"
echo "   cmake .. && make"
echo "   cd .."
echo "   ./scripts/start_cluster.sh 3"
echo "   ./build/dfs_cli put demo_files/test1.txt /dfs/hello.txt"
echo "   ./build/dfs_cli get /dfs/hello.txt downloaded.txt"
echo "   open http://localhost:8080"
echo ""

echo "2. Features to Test:"
echo "   # Basic operations"
echo "   ./build/dfs_cli put test1.txt /dfs/file1.txt"
echo "   ./build/dfs_cli list"
echo "   ./build/dfs_cli get /dfs/file1.txt downloaded1.txt"
echo ""
echo "   # Encryption"
echo "   ./build/dfs_cli put test2.txt /dfs/encrypted.txt --encrypt --password secret123"
echo "   ./build/dfs_cli get /dfs/encrypted.txt decrypted.txt --password secret123"
echo ""
echo "   # Replication"
echo "   ./build/dfs_cli put binary.dat /dfs/replicated.dat --replication 3"
echo ""
echo "   # Erasure coding"
echo "   ./build/dfs_cli put binary.dat /dfs/erasure.dat --erasure-coding"
echo ""
echo "   # Combined features"
echo "   ./build/dfs_cli put test1.txt /dfs/secure.txt --encrypt --password pass --erasure-coding --replication 2"
echo ""

echo "3. Web Dashboard:"
echo "   http://localhost:8080 - System overview"
echo "   http://localhost:8080/files - File browser"
echo "   http://localhost:8080/servers - Server status"
echo "   http://localhost:8080/api/stats - JSON API"
echo ""

echo "4. Testing:"
echo "   ./scripts/benchmark.sh upload"
echo "   ./scripts/benchmark.sh download"
echo "   ./scripts/benchmark.sh fault"
echo ""

echo "5. Cleanup:"
echo "   ./scripts/stop_cluster.sh"
echo "   rm -rf data/ demo_files/"
echo ""

echo "📋 Current Implementation Status:"
echo "✅ Complete distributed file system architecture"
echo "✅ Master server with metadata management"
echo "✅ Chunk servers with replication"
echo "✅ Client library with encryption"
echo "✅ Web dashboard with monitoring"
echo "✅ CLI interface with all features"
echo "✅ Comprehensive testing framework"
echo "✅ Docker deployment configuration"
echo "✅ Performance benchmarking tools"
echo "✅ Production-ready configuration"
echo ""

echo "🎉 Ready to explore the DFS system!"
echo "All features implemented and documented."