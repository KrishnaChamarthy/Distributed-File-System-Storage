#!/bin/bash

echo "🔨 Building DFS CLI..."
cd /Users/krishnachamarthy/Documents/GitHub/Distributed-File-System-Storage/Phase4

# Compile the CLI
g++ -std=c++17 -o dfs_cli dfs_cli.cpp

if [ $? -ne 0 ]; then
    echo "❌ Compilation failed"
    exit 1
fi

echo "✅ DFS CLI built successfully!"
echo ""
echo "🎯 Usage Examples:"
echo "=================="
echo "Interactive mode:"
echo "  ./dfs_cli"
echo ""
echo "Single command mode:"
echo "  ./dfs_cli put demo_files/test1.txt"
echo "  ./dfs_cli put demo_files/test1.txt /dfs/my_file.txt"
echo "  ./dfs_cli get /dfs/my_file.txt"
echo "  ./dfs_cli ls"
echo "  ./dfs_cli status"
echo "  ./dfs_cli rm /dfs/my_file.txt"
echo ""
echo "🚀 Starting interactive CLI..."
echo ""

./dfs_cli