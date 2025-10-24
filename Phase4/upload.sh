#!/bin/bash

echo "📁 DFS File Upload Tool"
echo "======================="

cd /Users/krishnachamarthy/Documents/GitHub/Distributed-File-System-Storage/Phase4

# Check if CLI exists, compile if needed
if [ ! -f dfs_cli ]; then
    echo "🔨 Building DFS CLI..."
    g++ -std=c++17 -o dfs_cli dfs_cli.cpp
    if [ $? -ne 0 ]; then
        echo "❌ Failed to build CLI"
        exit 1
    fi
fi

if [ $# -eq 0 ]; then
    echo "📖 Usage: $0 <file_to_upload> [remote_path]"
    echo ""
    echo "Examples:"
    echo "  $0 mydocument.txt                    # Upload as /dfs/mydocument.txt"
    echo "  $0 mydocument.txt /dfs/my_file.txt   # Upload with custom path"
    echo ""
    echo "Available demo files:"
    echo "  demo_files/test1.txt (17 bytes)"
    echo "  demo_files/test2.txt (64 bytes)" 
    echo "  demo_files/binary.dat (10KB)"
    echo ""
    echo "Try: $0 demo_files/test1.txt"
    exit 1
fi

LOCAL_FILE="$1"
REMOTE_PATH="$2"

if [ ! -f "$LOCAL_FILE" ]; then
    echo "❌ Error: File '$LOCAL_FILE' not found"
    exit 1
fi

echo "📤 Uploading '$LOCAL_FILE' to DFS..."
echo ""

if [ -n "$REMOTE_PATH" ]; then
    ./dfs_cli put "$LOCAL_FILE" "$REMOTE_PATH"
else
    ./dfs_cli put "$LOCAL_FILE"
fi

echo ""
echo "✅ Upload completed!"
echo ""
echo "💡 To see all files in DFS, run:"
echo "   ./dfs_cli ls"
echo ""
echo "💡 To download the file, run:"
if [ -n "$REMOTE_PATH" ]; then
    echo "   ./dfs_cli get \"$REMOTE_PATH\""
else
    FILENAME=$(basename "$LOCAL_FILE")
    echo "   ./dfs_cli get \"/dfs/$FILENAME\""
fi