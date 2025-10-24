#!/bin/bash

echo "🎯 DFS CLI - Upload Files Demo"
echo "=============================="

cd /Users/krishnachamarthy/Documents/GitHub/Distributed-File-System-Storage/Phase4

# Compile CLI if needed
if [ ! -f dfs_cli ]; then
    echo "🔨 Compiling CLI..."
    g++ -std=c++17 -o dfs_cli dfs_cli.cpp
fi

echo ""
echo "🚀 Starting DFS with file uploads..."
echo ""

# Create a demo script to run multiple commands
cat > demo_session.txt << 'EOF'
put demo_files/test1.txt
put demo_files/test2.txt /dfs/my_document.txt
put demo_files/binary.dat /dfs/large_file.dat
ls
status
get /dfs/test1.txt
exists /dfs/test1.txt
help
exit
EOF

echo "📋 Demo commands to execute:"
echo "=============================="
cat demo_session.txt

echo ""
echo "▶️  Running DFS CLI with demo commands..."
echo ""

# Run CLI with input from file
./dfs_cli < demo_session.txt

echo ""
echo "✅ Demo completed!"
echo ""
echo "📁 Files created:"
ls -la downloads/ 2>/dev/null || echo "No downloads directory found"

echo ""
echo "💾 DFS data directory:"
ls -la data/ 2>/dev/null || echo "No data directory found"

# Clean up
rm -f demo_session.txt