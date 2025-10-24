#!/bin/bash

echo "🚀 Building and Running DFS Live Demo"
echo "======================================"

cd /Users/krishnachamarthy/Documents/GitHub/Distributed-File-System-Storage/Phase4

# Compile the demo
echo "🔨 Compiling demo..."
g++ -std=c++17 -o dfs_demo simple_dfs_demo.cpp

if [ $? -ne 0 ]; then
    echo "❌ Compilation failed"
    exit 1
fi

echo "✅ Compilation successful"

# Run the demo
echo ""
echo "🎬 Starting DFS Demo..."
echo ""

./dfs_demo

echo ""
echo "📊 Files created during demo:"
ls -la *.txt *.dat 2>/dev/null || echo "No demo files found"

echo ""
echo "💾 Data directory contents:"
ls -la data/ 2>/dev/null || echo "No data directory found"

echo ""
echo "🎉 Demo completed! Check the output above for results."