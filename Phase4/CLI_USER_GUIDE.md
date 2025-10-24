# DFS Phase 4 - CLI User Guide

## 🚀 Quick Start

### Upload Files Using CLI

The DFS CLI provides multiple ways to upload files:

#### 1. Interactive Mode
```bash
./dfs_cli
# Then use commands:
dfs> put myfile.txt
dfs> put myfile.txt /dfs/custom_name.txt
dfs> ls
dfs> exit
```

#### 2. Single Command Mode
```bash
./dfs_cli put demo_files/test1.txt
./dfs_cli put demo_files/test2.txt /dfs/my_document.txt
./dfs_cli ls
./dfs_cli get /dfs/test1.txt
```

#### 3. Quick Upload Script
```bash
./upload.sh demo_files/test1.txt
./upload.sh demo_files/test2.txt /dfs/custom_path.txt
```

## 📋 Available Commands

| Command | Description | Example |
|---------|-------------|---------|
| `put <local_file> [remote_path]` | Upload a file | `put document.txt /dfs/my_doc.txt` |
| `get <remote_path> [local_file]` | Download to downloads/ | `get /dfs/my_doc.txt` → `downloads/my_doc.txt` |
| `ls` | List all files in DFS | `ls` |
| `status` | Show DFS system status | `status` |
| `rm <remote_path>` | Delete a file | `rm /dfs/my_doc.txt` |
| `exists <remote_path>` | Check if file exists | `exists /dfs/my_doc.txt` |
| `help` | Show help message | `help` |
| `exit` | Exit the CLI | `exit` |

## 🎯 Demo Scripts

### Complete Demo
```bash
./final_demo.sh
```
Shows all features: upload, download, list, delete, status

### Simple Upload Demo
```bash
./demo_cli.sh
```
Demonstrates basic file operations

### Upload Tool
```bash
./upload.sh myfile.txt [/dfs/remote_path.txt]
```
Quick file upload utility

## 🎮 Try It Now!

1. **Upload a file:**
   ```bash
   ./dfs_cli put demo_files/test1.txt
   ```

2. **List files:**
   ```bash
   ./dfs_cli ls
   ```

3. **Download a file:**
   ```bash
   ./dfs_cli get /dfs/test1.txt   # Downloads to downloads/test1.txt
   ```

4. **Interactive session:**
   ```bash
   ./dfs_cli
   dfs> put demo_files/test2.txt /dfs/my_file.txt
   dfs> ls
   dfs> get /dfs/my_file.txt      # Downloads to downloads/my_file.txt
   dfs> rm /dfs/my_file.txt
   dfs> exit
   ```

## 📁 **Download Location**

**All downloaded files are automatically saved to the `downloads/` directory:**

- `get /dfs/myfile.txt` → `downloads/myfile.txt`
- `get /dfs/myfile.txt custom.txt` → `downloads/custom.txt`  
- `get /dfs/myfile.txt /full/path/file.txt` → `/full/path/file.txt` (absolute paths work too)

## ✨ Features Demonstrated

- ✅ **File Upload/Download**: Put and get files with CLI commands
- ✅ **Custom Paths**: Specify custom remote paths for uploaded files
- ✅ **File Management**: List, delete, and check file existence
- ✅ **Replication**: Files automatically replicated across 3 chunk servers
- ✅ **Status Monitoring**: View system status and disk usage
- ✅ **Interactive & Batch**: Both interactive and single-command modes
- ✅ **Data Persistence**: Files stored in data/ directory as chunks
- ✅ **Error Handling**: Proper error messages for invalid operations

## 📂 File Structure

- `dfs_cli` - Main CLI executable
- `dfs_cli.cpp` - CLI source code
- `upload.sh` - Quick upload script
- `demo_cli.sh` - Interactive demo
- `final_demo.sh` - Complete feature demo
- `data/` - DFS storage directory (chunks stored here)
- `demo_files/` - Sample files for testing

## 🎉 Success!

The DFS CLI successfully demonstrates:
- Multi-server distributed storage simulation
- File chunking and replication (R=3)
- Master server coordination
- Client upload/download operations
- Real-time status monitoring
- Complete file lifecycle management

Try the demos and start uploading your own files! 🚀