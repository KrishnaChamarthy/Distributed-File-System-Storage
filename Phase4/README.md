# DFS Phase 4 - Production-Ready Distributed File System

The complete implementation of a Google File System-like distributed file system with enterprise-grade features.

## 🚀 Quick Start

### Simple CLI Usage (Recommended)
```bash
# Compile the CLI
g++ -std=c++17 -o dfs_cli dfs_cli.cpp

# Interactive mode
./dfs_cli
dfs> put demo_files/test1.txt
dfs> ls
dfs> get /dfs/test1.txt
dfs> exit

# Single commands
./dfs_cli put myfile.txt
./dfs_cli get /dfs/myfile.txt
./dfs_cli ls
```

### Helper Scripts
```bash
./upload.sh myfile.txt               # Quick upload
./demo_cli.sh                        # Interactive demo
./final_demo.sh                      # Complete feature demo
```

## 📁 File Organization

```
Phase4/
├── dfs_cli                 # Main CLI executable
├── downloads/              # Downloaded files (auto-created)
├── demo_files/             # Sample files for testing
├── data/                   # DFS storage (chunks)
├── CLI_USER_GUIDE.md       # Detailed CLI guide
├── TESTING_GUIDE.md        # Testing instructions
├── QUICKSTART.md           # Quick start guide
└── [helper scripts]
```

## ✨ Key Features

- 🎯 **Interactive CLI** - Easy-to-use command line interface
- 📥 **Auto Downloads Folder** - Downloaded files organized in downloads/
- 🔄 **3-Way Replication** - Files replicated across multiple servers
- 🔐 **Encryption Support** - AES-256-GCM encryption simulation
- 📊 **Real-time Status** - System monitoring and file management
- 🧪 **Comprehensive Testing** - Extensive test coverage
- 🐳 **Docker Ready** - Easy deployment and scaling

## 📖 Documentation

- **[CLI User Guide](CLI_USER_GUIDE.md)** - Complete CLI usage guide
- **[Web Usage Guide](WEB_USAGE_GUIDE.md)** - Web dashboard and API guide
- **[Testing Guide](TESTING_GUIDE.md)** - Testing instructions and examples
- **[Quick Start](QUICKSTART.md)** - Get started in 5 minutes
- **[Implementation Status](IMPLEMENTATION_STATUS.md)** - Feature coverage

## � Usage Examples

### Basic Operations
```bash
# Upload files
./dfs_cli put document.txt
./dfs_cli put photo.jpg /dfs/my_photo.jpg

# List files
./dfs_cli ls

# Download files (saved to downloads/)
./dfs_cli get /dfs/document.txt
./dfs_cli get /dfs/my_photo.jpg backup_photo.jpg

# File management
./dfs_cli rm /dfs/old_file.txt
./dfs_cli exists /dfs/document.txt
./dfs_cli status
```

### Demo Files Available
- `demo_files/test1.txt` (17 bytes) - "Hello DFS World!"
- `demo_files/test2.txt` (64 bytes) - Longer text content
- `demo_files/binary.dat` (10KB) - Binary test data

## 🔧 Advanced Usage

### Full System Build
```bash
mkdir build && cd build
cmake ..
make

# Start complete cluster
../scripts/start_cluster.sh 3
```

### Web Dashboard
```bash
# Start web server (in full build)
./web_server
# Access: http://localhost:8080

# Or try the web demo
./web_demo.sh
```

### Docker Deployment
```bash
docker-compose up --build
docker-compose up --scale chunkserver=5
```