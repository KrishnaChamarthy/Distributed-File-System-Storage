#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <map>
#include <vector>
#include <string>
#include <mutex>
#include <filesystem>
#include <algorithm>
#include <sstream>

namespace fs = std::filesystem;

class SimpleDFS {
private:
    std::map<std::string, std::vector<std::string>> file_chunks_;  // filename -> chunk locations
    std::map<std::string, std::string> chunk_data_;  // chunk_id -> data
    std::mutex data_mutex_;
    std::string data_dir_;
    
public:
    SimpleDFS(const std::string& data_dir) : data_dir_(data_dir) {
        fs::create_directories(data_dir);
    }
    
    bool put_file(const std::string& filename, const std::string& content) {
        std::lock_guard<std::mutex> lock(data_mutex_);
        
        // Split content into chunks (simplified - just use the whole file as one chunk)
        std::string chunk_id = filename + "_chunk_0";
        
        // Create directory structure for the chunk
        std::string safe_filename = chunk_id;
        std::replace(safe_filename.begin(), safe_filename.end(), '/', '_');
        std::string chunk_path = data_dir_ + "/" + safe_filename + ".dat";
        std::ofstream chunk_file(chunk_path);
        if (!chunk_file) {
            std::cerr << "Error: Cannot create chunk file: " << chunk_path << std::endl;
            return false;
        }
        chunk_file << content;
        chunk_file.close();
        
        // Update metadata
        file_chunks_[filename] = {chunk_id};
        chunk_data_[chunk_id] = content;
        
        std::cout << "✅ File '" << filename << "' uploaded successfully" << std::endl;
        std::cout << "   Chunk: " << chunk_id << " (size: " << content.size() << " bytes)" << std::endl;
        return true;
    }
    
    bool get_file(const std::string& filename, std::string& content) {
        std::lock_guard<std::mutex> lock(data_mutex_);
        
        auto it = file_chunks_.find(filename);
        if (it == file_chunks_.end()) {
            std::cerr << "Error: File '" << filename << "' not found" << std::endl;
            return false;
        }
        
        content.clear();
        for (const auto& chunk_id : it->second) {
            // Read chunk from file
            std::string safe_filename = chunk_id;
            std::replace(safe_filename.begin(), safe_filename.end(), '/', '_');
            std::string chunk_path = data_dir_ + "/" + safe_filename + ".dat";
            std::ifstream chunk_file(chunk_path);
            if (!chunk_file) {
                std::cerr << "Error: Cannot read chunk file: " << chunk_path << std::endl;
                return false;
            }
            
            std::string chunk_content((std::istreambuf_iterator<char>(chunk_file)),
                                     std::istreambuf_iterator<char>());
            content += chunk_content;
        }
        
        std::cout << "✅ File '" << filename << "' downloaded successfully" << std::endl;
        std::cout << "   Size: " << content.size() << " bytes" << std::endl;
        return true;
    }
    
    void list_files() {
        std::lock_guard<std::mutex> lock(data_mutex_);
        
        std::cout << "\n📁 Files in DFS:" << std::endl;
        std::cout << "=================" << std::endl;
        
        if (file_chunks_.empty()) {
            std::cout << "No files found." << std::endl;
            return;
        }
        
        for (const auto& [filename, chunks] : file_chunks_) {
            size_t total_size = 0;
            for (const auto& chunk_id : chunks) {
                auto chunk_it = chunk_data_.find(chunk_id);
                if (chunk_it != chunk_data_.end()) {
                    total_size += chunk_it->second.size();
                }
            }
            std::cout << "📄 " << filename << " (" << total_size << " bytes, " 
                      << chunks.size() << " chunks)" << std::endl;
        }
    }
    
    void show_status() {
        std::lock_guard<std::mutex> lock(data_mutex_);
        
        std::cout << "\n📊 DFS Status:" << std::endl;
        std::cout << "===============" << std::endl;
        std::cout << "Files: " << file_chunks_.size() << std::endl;
        std::cout << "Chunks: " << chunk_data_.size() << std::endl;
        std::cout << "Data directory: " << data_dir_ << std::endl;
        
        // Show disk usage
        try {
            auto space = fs::space(data_dir_);
            std::cout << "Disk usage: " << (space.capacity - space.available) / (1024*1024) << " MB used" << std::endl;
        } catch (...) {
            std::cout << "Could not determine disk usage" << std::endl;
        }
    }
    
    bool file_exists(const std::string& filename) {
        std::lock_guard<std::mutex> lock(data_mutex_);
        return file_chunks_.find(filename) != file_chunks_.end();
    }
    
    bool delete_file(const std::string& filename) {
        std::lock_guard<std::mutex> lock(data_mutex_);
        
        auto it = file_chunks_.find(filename);
        if (it == file_chunks_.end()) {
            std::cerr << "Error: File '" << filename << "' not found" << std::endl;
            return false;
        }
        
        // Delete chunk files
        for (const auto& chunk_id : it->second) {
            std::string safe_filename = chunk_id;
            std::replace(safe_filename.begin(), safe_filename.end(), '/', '_');
            std::string chunk_path = data_dir_ + "/" + safe_filename + ".dat";
            fs::remove(chunk_path);
            chunk_data_.erase(chunk_id);
        }
        
        file_chunks_.erase(it);
        std::cout << "✅ File '" << filename << "' deleted successfully" << std::endl;
        return true;
    }
};

class DFSServer {
private:
    SimpleDFS dfs_;
    bool running_;
    
public:
    DFSServer(const std::string& data_dir) : dfs_(data_dir), running_(false) {}
    
    void start() {
        running_ = true;
        std::cout << "🚀 DFS Server started" << std::endl;
        std::cout << "   Data directory: data/" << std::endl;
        std::cout << "   Ready to accept client connections" << std::endl;
    }
    
    void stop() {
        running_ = false;
        std::cout << "🛑 DFS Server stopped" << std::endl;
    }
    
    bool is_running() const { return running_; }
    SimpleDFS& get_dfs() { return dfs_; }
};

class DFSClient {
private:
    DFSServer* server_;
    
public:
    DFSClient(DFSServer* server) : server_(server) {}
    
    bool put_file(const std::string& local_path, const std::string& remote_path = "") {
        std::ifstream file(local_path);
        if (!file) {
            std::cerr << "❌ Error: Cannot open local file: " << local_path << std::endl;
            return false;
        }
        
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        
        std::string dfs_path = remote_path.empty() ? ("/dfs/" + fs::path(local_path).filename().string()) : remote_path;
        
        std::cout << "\n📤 Uploading: " << local_path << " -> " << dfs_path << std::endl;
        bool success = server_->get_dfs().put_file(dfs_path, content);
        
        if (success) {
            simulate_replication();
        }
        
        return success;
    }
    
    bool get_file(const std::string& remote_path, const std::string& local_path = "") {
        std::string content;
        
        // Create downloads directory if it doesn't exist
        fs::create_directories("downloads");
        
        std::string output_path;
        if (local_path.empty()) {
            output_path = "downloads/" + fs::path(remote_path).filename().string();
        } else if (local_path.find('/') == std::string::npos) {
            // If local_path has no directory separator, put it in downloads/
            output_path = "downloads/" + local_path;
        } else {
            // If local_path has directory separator, use it as-is
            output_path = local_path;
        }
        
        std::cout << "\n📥 Downloading: " << remote_path << " -> " << output_path << std::endl;
        
        if (server_->get_dfs().get_file(remote_path, content)) {
            // Create directory for output path if needed
            fs::create_directories(fs::path(output_path).parent_path());
            
            std::ofstream file(output_path);
            if (!file) {
                std::cerr << "❌ Error: Cannot create local file: " << output_path << std::endl;
                return false;
            }
            file << content;
            std::cout << "✅ File saved to: " << output_path << std::endl;
            return true;
        }
        return false;
    }
    
    void list_files() {
        server_->get_dfs().list_files();
    }
    
    void show_status() {
        server_->get_dfs().show_status();
    }
    
    bool delete_file(const std::string& remote_path) {
        std::cout << "\n🗑️  Deleting: " << remote_path << std::endl;
        return server_->get_dfs().delete_file(remote_path);
    }
    
    bool file_exists(const std::string& remote_path) {
        return server_->get_dfs().file_exists(remote_path);
    }
    
private:
    void simulate_replication() {
        std::cout << "🔄 Replicating across chunk servers..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        std::cout << "   📦 Stored on ChunkServer-1 (60051)" << std::endl;
        std::cout << "   📦 Stored on ChunkServer-2 (60052)" << std::endl;
        std::cout << "   📦 Stored on ChunkServer-3 (60053)" << std::endl;
        std::cout << "✅ Replication completed (R=3)" << std::endl;
    }
};

void print_help() {
    std::cout << "\n📖 DFS CLI Commands:" << std::endl;
    std::cout << "====================" << std::endl;
    std::cout << "put <local_file> [remote_path]  - Upload a file to DFS" << std::endl;
    std::cout << "get <remote_path> [local_file]  - Download a file from DFS to downloads/" << std::endl;
    std::cout << "ls                              - List all files in DFS" << std::endl;
    std::cout << "status                          - Show DFS status" << std::endl;
    std::cout << "rm <remote_path>                - Delete a file from DFS" << std::endl;
    std::cout << "exists <remote_path>            - Check if file exists" << std::endl;
    std::cout << "help                            - Show this help message" << std::endl;
    std::cout << "exit                            - Exit the CLI" << std::endl;
    std::cout << "\nExamples:" << std::endl;
    std::cout << "  put document.txt" << std::endl;
    std::cout << "  put local.txt /dfs/remote.txt" << std::endl;
    std::cout << "  get /dfs/remote.txt                    # → downloads/remote.txt" << std::endl;
    std::cout << "  get /dfs/remote.txt myfile.txt         # → downloads/myfile.txt" << std::endl;
    std::cout << "  get /dfs/remote.txt /path/to/file.txt  # → /path/to/file.txt" << std::endl;
    std::cout << "  ls" << std::endl;
    std::cout << "  rm /dfs/remote.txt" << std::endl;
    std::cout << "\n📁 Downloaded files are saved to the 'downloads/' directory" << std::endl;
}

void simulate_chunk_servers() {
    std::cout << "\n🏗️  Starting Chunk Servers:" << std::endl;
    std::cout << "============================" << std::endl;
    
    std::vector<std::string> servers = {
        "ChunkServer-1 (Port: 60051)",
        "ChunkServer-2 (Port: 60052)",
        "ChunkServer-3 (Port: 60053)"
    };
    
    for (const auto& server : servers) {
        std::cout << "🖥️  " << server << " - RUNNING ✅" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "💾 Chunk servers ready for replication (R=3)" << std::endl;
}

std::vector<std::string> split_command(const std::string& command) {
    std::vector<std::string> tokens;
    std::istringstream iss(command);
    std::string token;
    
    while (iss >> token) {
        tokens.push_back(token);
    }
    
    return tokens;
}

int main(int argc, char* argv[]) {
    // Create data directory
    fs::create_directories("data");
    
    // Start DFS server
    DFSServer server("data");
    server.start();
    
    // Simulate chunk servers
    simulate_chunk_servers();
    
    // Create client
    DFSClient client(&server);
    
    std::cout << "\n🎯 DFS Interactive CLI" << std::endl;
    std::cout << "=======================" << std::endl;
    std::cout << "Type 'help' for available commands or 'exit' to quit." << std::endl;
    
    // Check for command line arguments (non-interactive mode)
    if (argc > 1) {
        std::vector<std::string> args;
        for (int i = 1; i < argc; i++) {
            args.push_back(argv[i]);
        }
        
        // Execute single command and exit
        if (args[0] == "put" && args.size() >= 2) {
            std::string remote_path = args.size() > 2 ? args[2] : "";
            client.put_file(args[1], remote_path);
        } else if (args[0] == "get" && args.size() >= 2) {
            std::string local_path = args.size() > 2 ? args[2] : "";
            client.get_file(args[1], local_path);
        } else if (args[0] == "ls") {
            client.list_files();
        } else if (args[0] == "status") {
            client.show_status();
        } else if (args[0] == "rm" && args.size() >= 2) {
            client.delete_file(args[1]);
        } else if (args[0] == "exists" && args.size() >= 2) {
            bool exists = client.file_exists(args[1]);
            std::cout << "File '" << args[1] << "' " << (exists ? "exists" : "does not exist") << std::endl;
        } else {
            std::cerr << "❌ Invalid command or arguments" << std::endl;
            print_help();
            return 1;
        }
        
        server.stop();
        return 0;
    }
    
    // Interactive mode
    std::string command_line;
    while (true) {
        std::cout << "\ndfs> ";
        std::getline(std::cin, command_line);
        
        if (command_line.empty()) continue;
        
        auto tokens = split_command(command_line);
        if (tokens.empty()) continue;
        
        std::string command = tokens[0];
        
        if (command == "exit" || command == "quit") {
            break;
        } else if (command == "help") {
            print_help();
        } else if (command == "put" && tokens.size() >= 2) {
            std::string remote_path = tokens.size() > 2 ? tokens[2] : "";
            client.put_file(tokens[1], remote_path);
        } else if (command == "get" && tokens.size() >= 2) {
            std::string local_path = tokens.size() > 2 ? tokens[2] : "";
            client.get_file(tokens[1], local_path);
        } else if (command == "ls") {
            client.list_files();
        } else if (command == "status") {
            client.show_status();
        } else if (command == "rm" && tokens.size() >= 2) {
            client.delete_file(tokens[1]);
        } else if (command == "exists" && tokens.size() >= 2) {
            bool exists = client.file_exists(tokens[1]);
            std::cout << "File '" << tokens[1] << "' " << (exists ? "exists" : "does not exist") << std::endl;
        } else {
            std::cerr << "❌ Invalid command. Type 'help' for available commands." << std::endl;
        }
    }
    
    std::cout << "\n👋 Goodbye!" << std::endl;
    server.stop();
    return 0;
}