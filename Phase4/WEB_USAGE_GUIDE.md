# DFS Web Dashboard - User Guide

Complete guide to using the web interface for the Distributed File System.

## 🌐 Web Dashboard Overview

The DFS Web Dashboard provides a browser-based interface for monitoring and managing your distributed file system. It offers real-time system metrics, file management, and server monitoring capabilities.

## 🚀 Getting Started

### Method 1: Simple Demo Mode (Current Implementation)
The current CLI implementation simulates the web dashboard features:

```bash
cd Phase4/

# Start the CLI with web simulation
./dfs_cli
dfs> status    # Shows system status (simulates web metrics)
dfs> ls        # Shows file listing (simulates web file browser)
```

### Method 2: Full Web Server (Advanced Build)
For the complete web interface, build the full system:

```bash
cd Phase4/

# Build the complete system
mkdir build && cd build
cmake ..
make

# Start master server with web dashboard
./master_server

# Access web dashboard
open http://localhost:8080
```

### Method 3: Docker Deployment
```bash
cd Phase4/

# Start complete cluster with web dashboard
docker-compose up -d

# Access web dashboard
open http://localhost:8080
```

## 🖥️ Web Interface Features

### 📊 Dashboard Pages

#### 1. **Main Dashboard** - `http://localhost:8080`
**What you see:**
- 📈 **System Overview**: Total files, storage usage, server count
- 🔄 **Recent Activity**: Latest uploads, downloads, system events
- 📊 **Performance Metrics**: Throughput, response times, error rates
- 🖥️ **Server Health**: Quick status of all chunk servers

**Key Metrics:**
```
Total Files: 156
Total Storage: 2.4 GB
Active Servers: 3/3
Uptime: 2d 4h 23m
```

#### 2. **File Browser** - `http://localhost:8080/files`
**What you see:**
- 📁 **File Listing**: All files stored in the DFS
- 📄 **File Details**: Size, replication factor, creation date
- 🔍 **Search & Filter**: Find files by name, size, date
- 📥 **Download Links**: Direct download buttons for files

**File Management:**
- View file metadata
- Download files directly from browser
- Delete files (with confirmation)
- Monitor replication status

#### 3. **Server Status** - `http://localhost:8080/servers`
**What you see:**
- 🖥️ **Server List**: All chunk servers in the cluster
- 💾 **Storage Usage**: Disk usage per server
- ❤️ **Health Status**: Heartbeat and connectivity status
- 📊 **Performance**: Request rates, response times per server

**Server Information:**
```
chunk-server-1: ONLINE  | 85% disk | 234ms avg response
chunk-server-2: ONLINE  | 72% disk | 189ms avg response  
chunk-server-3: ONLINE  | 91% disk | 267ms avg response
```

#### 4. **Statistics** - `http://localhost:8080/stats`
**What you see:**
- 📈 **Performance Graphs**: Real-time charts of system metrics
- 📊 **Usage Analytics**: Storage trends, access patterns
- 🎯 **Health Metrics**: Error rates, success rates, uptime
- 📋 **Detailed Reports**: Comprehensive system statistics

### 🔌 API Endpoints

#### REST API Access
```bash
# System status (JSON)
curl http://localhost:8080/api/stats

# File listing (JSON)
curl http://localhost:8080/api/files

# Server status (JSON)
curl http://localhost:8080/api/servers

# Upload file via API
curl -X POST -F "file=@myfile.txt" http://localhost:8080/api/upload

# Download file via API
curl http://localhost:8080/api/files/myfile.txt -o downloaded.txt
```

#### API Response Examples
```json
{
  "status": "healthy",
  "files": 156,
  "storage_used": "2.4 GB",
  "servers_online": 3,
  "uptime": "2d 4h 23m",
  "performance": {
    "upload_rate": "45.2 MB/s",
    "download_rate": "67.8 MB/s",
    "requests_per_minute": 342
  }
}
```

## 🎯 How to Use the Web Interface

### 📤 **Upload Files via Web**

1. **Navigate to Dashboard**: Open `http://localhost:8080`
2. **Find Upload Section**: Look for "Upload Files" button/section
3. **Select Files**: 
   - Click "Choose Files" or drag & drop
   - Select one or multiple files
   - Configure upload options (encryption, replication)
4. **Monitor Progress**: Watch real-time upload progress
5. **Verify Upload**: Check file appears in file browser

### 📥 **Download Files via Web**

1. **Go to File Browser**: Navigate to `http://localhost:8080/files`
2. **Find Your File**: Use search or browse the file list
3. **Download Options**:
   - Click download icon for direct download
   - Right-click for download options
   - Use API endpoint for programmatic access

### 📊 **Monitor System Health**

1. **Dashboard Overview**: Main page shows system health at a glance
2. **Server Status**: Check `http://localhost:8080/servers` for detailed server info
3. **Performance Metrics**: View real-time graphs and statistics
4. **Alerts**: Monitor for any warning or error indicators

### 🔧 **Manage Files**

1. **File Information**: Click on files to see detailed metadata
2. **Replication Status**: Check how many replicas exist
3. **Storage Location**: See which servers store the file
4. **File Operations**: Delete, move, or copy files
5. **Search & Filter**: Find files quickly using filters

## 🎮 Interactive Demo

### Simulated Web Experience (Current CLI)
Since the current implementation focuses on CLI, you can simulate web dashboard features:

```bash
# Start CLI to simulate web dashboard
./dfs_cli

# Simulate dashboard "System Status" page
dfs> status
📊 DFS Status:
===============
Files: 3
Chunks: 3
Data directory: data
Disk usage: 1.2 GB used

# Simulate "File Browser" page  
dfs> ls
📁 Files in DFS:
=================
📄 /dfs/document.txt (64 bytes, 1 chunks)
📄 /dfs/photo.jpg (1.2 MB, 1 chunks)
📄 /dfs/video.mp4 (15.8 MB, 1 chunks)

# Simulate "File Upload" functionality
dfs> put demo_files/test1.txt /dfs/new_file.txt
📤 Uploading: demo_files/test1.txt -> /dfs/new_file.txt
✅ File '/dfs/new_file.txt' uploaded successfully
🔄 Replicating across chunk servers...
   📦 Stored on ChunkServer-1 (60051)
   📦 Stored on ChunkServer-2 (60052)  
   📦 Stored on ChunkServer-3 (60053)
✅ Replication completed (R=3)
```

## 🌟 Advanced Web Features

### 🔐 **Security Features**
- HTTPS support for secure web access
- Authentication and authorization
- Role-based access control
- API key management

### 📊 **Monitoring & Analytics**
- Real-time performance dashboards
- Historical trend analysis
- Capacity planning tools
- Alert notifications

### ⚙️ **Administration**
- Server configuration management
- User management interface
- System configuration updates
- Maintenance mode controls

### 🔄 **Real-time Updates**
- WebSocket connections for live updates
- Auto-refreshing statistics
- Live file upload/download progress
- Real-time server status changes

## 🚀 **Getting the Full Web Experience**

### Build Complete System
```bash
cd Phase4/

# Install dependencies (Ubuntu/Debian)
sudo apt-get install -y build-essential cmake libssl-dev \
  libgrpc++-dev libprotobuf-dev libjsoncpp-dev

# Build
mkdir build && cd build
cmake ..
make

# Start services
./master_server --web-port 8080 &
./chunk_server --port 60051 &
./chunk_server --port 60052 &
./chunk_server --port 60053 &

# Access web dashboard
open http://localhost:8080
```

### Docker Quick Start
```bash
cd Phase4/

# Start everything with Docker
docker-compose up -d

# Check services
docker-compose ps

# Access dashboard
open http://localhost:8080

# View logs
docker-compose logs web
```

## 🎉 **Web Dashboard Benefits**

✅ **Visual Monitoring**: See system health at a glance
✅ **Easy File Management**: Browser-based file operations  
✅ **Real-time Updates**: Live system statistics
✅ **Multi-user Access**: Multiple users can monitor simultaneously
✅ **Mobile Friendly**: Responsive design for tablets/phones
✅ **API Integration**: REST API for automation
✅ **No Installation**: Just use a web browser
✅ **Intuitive Interface**: Easy to learn and use

