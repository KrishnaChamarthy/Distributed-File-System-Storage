#!/bin/bash

echo "🎯 Complete DFS CLI Demo"
echo "========================"
echo ""

cd /Users/krishnachamarthy/Documents/GitHub/Distributed-File-System-Storage/Phase4

echo "📋 This demo will show:"
echo "1. File uploads using CLI"
echo "2. File listing"
echo "3. File downloads"
echo "4. File existence checks"
echo "5. System status"
echo "6. File deletion"
echo ""

echo "🔨 Building CLI if needed..."
if [ ! -f dfs_cli ]; then
    g++ -std=c++17 -o dfs_cli dfs_cli.cpp
fi

echo ""
echo "▶️  Starting comprehensive CLI demo..."
echo ""

# Create comprehensive demo script
cat > full_demo.txt << 'EOF'
put demo_files/test1.txt
put demo_files/test2.txt /dfs/custom_name.txt
put demo_files/binary.dat /dfs/binary_file.dat
ls
status
exists /dfs/test1.txt
exists /dfs/nonexistent.txt
get /dfs/test1.txt
get /dfs/custom_name.txt
ls
rm /dfs/binary_file.dat
ls
status
exit
EOF

echo "📝 Demo script created. Running commands:"
echo "==========================================="
cat full_demo.txt | sed 's/^/  /'

echo ""
echo "🚀 Executing demo..."
echo ""

./dfs_cli < full_demo.txt

echo ""
echo "🔍 Verification - Downloaded files:"
echo "==================================="

if [ -f downloads/test1.txt ]; then
    echo "✅ downloads/test1.txt: $(wc -c < downloads/test1.txt) bytes"
    echo "   Content: $(cat downloads/test1.txt)"
else
    echo "❌ downloads/test1.txt not found"
fi

if [ -f downloads/custom_name.txt ]; then
    echo "✅ downloads/custom_name.txt: $(wc -c < downloads/custom_name.txt) bytes" 
    echo "   Content: $(head -c 50 downloads/custom_name.txt)..."
else
    echo "❌ downloads/custom_name.txt not found"
fi

echo ""
echo "📁 Downloads directory:"
ls -la downloads/ 2>/dev/null || echo "No downloads directory found"

echo ""
echo "💾 Current DFS data directory:"
ls -la data/ 2>/dev/null | head -10

echo ""
echo "� Web Interface Available:"
echo "   ./web_demo.sh                       # Web dashboard demo"
echo "   cat WEB_USAGE_GUIDE.md              # Complete web guide"
echo "   http://localhost:8080 (full build)  # Live web dashboard"

echo ""
echo "�🎉 Demo completed successfully!"
echo ""
echo "📖 Usage Summary:"
echo "=================="
echo "Direct CLI usage:"
echo "  ./dfs_cli                          # Interactive mode"
echo "  ./dfs_cli put myfile.txt           # Upload file"
echo "  ./dfs_cli get /dfs/myfile.txt      # Download file"
echo "  ./dfs_cli ls                       # List files"
echo ""
echo "Helper scripts:"
echo "  ./upload.sh myfile.txt             # Quick upload"
echo "  ./demo_cli.sh                      # Full demo"

# Cleanup
rm -f full_demo.txt