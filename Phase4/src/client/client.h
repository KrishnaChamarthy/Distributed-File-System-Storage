#pragma once

#include "file_system.pb.h"
#include "file_system.grpc.pb.h"
#include "utils.h"
#include "crypto.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace dfs {

// Client cache manager for frequently accessed chunks
class CacheManager {
public:
    CacheManager(size_t max_cache_size_mb = 100);
    ~CacheManager();
    
    // Cache operations
    bool put(const std::string& chunk_id, const std::vector<uint8_t>& data);
    std::vector<uint8_t> get(const std::string& chunk_id);
    bool contains(const std::string& chunk_id) const;
    void remove(const std::string& chunk_id);
    void clear();
    
    // Statistics
    size_t size() const { return cache_.size(); }
    size_t getTotalSize() const { return total_size_; }
    double getHitRate() const;
    
private:
    struct CacheEntry {
        std::vector<uint8_t> data;
        int64_t last_accessed;
        size_t size;
    };
    
    size_t max_size_;
    size_t total_size_;
    std::unordered_map<std::string, CacheEntry> cache_;
    mutable std::mutex cache_mutex_;
    
    // Statistics
    std::atomic<int64_t> cache_hits_;
    std::atomic<int64_t> cache_misses_;
    
    // LRU eviction
    void evictLRU();
    void updateAccessTime(const std::string& chunk_id);
};

// File uploader
class Uploader {
public:
    Uploader(std::shared_ptr<FileService::Stub> file_service,
             std::shared_ptr<CacheManager> cache_manager);
    
    bool uploadFile(const std::string& local_path, 
                   const std::string& remote_path,
                   bool enable_encryption = true,
                   bool enable_erasure_coding = false);
    
    // Progress callback
    void setProgressCallback(std::function<void(int64_t, int64_t)> callback) {
        progress_callback_ = callback;
    }
    
private:
    std::shared_ptr<FileService::Stub> file_service_;
    std::shared_ptr<CacheManager> cache_manager_;
    std::function<void(int64_t, int64_t)> progress_callback_;
    
    bool uploadChunk(const std::string& chunk_id,
                    const std::vector<uint8_t>& data,
                    const std::vector<std::string>& server_addresses,
                    bool is_encrypted = false);
    
    std::vector<std::vector<uint8_t>> splitFileIntoChunks(const std::vector<uint8_t>& file_data);
};

// File downloader
class Downloader {
public:
    Downloader(std::shared_ptr<FileService::Stub> file_service,
               std::shared_ptr<CacheManager> cache_manager);
    
    bool downloadFile(const std::string& remote_path, 
                     const std::string& local_path);
    
    // Progress callback
    void setProgressCallback(std::function<void(int64_t, int64_t)> callback) {
        progress_callback_ = callback;
    }
    
private:
    std::shared_ptr<FileService::Stub> file_service_;
    std::shared_ptr<CacheManager> cache_manager_;
    std::function<void(int64_t, int64_t)> progress_callback_;
    
    std::vector<uint8_t> downloadChunk(const std::string& chunk_id,
                                      const std::vector<std::string>& server_addresses);
    
    std::vector<uint8_t> assembleChunks(const std::vector<std::vector<uint8_t>>& chunks);
};

// Main DFS client
class DFSClient {
public:
    DFSClient(const std::string& master_address, int master_port);
    ~DFSClient();
    
    // File operations
    bool put(const std::string& local_file, const std::string& remote_file,
            bool enable_encryption = true, bool enable_erasure_coding = false);
    bool get(const std::string& remote_file, const std::string& local_file);
    bool deleteFile(const std::string& remote_file);
    bool listFiles(const std::string& path_prefix = "");
    bool getFileInfo(const std::string& remote_file);
    
    // Configuration
    void enableVerboseLogging(bool enable) { verbose_logging_ = enable; }
    void setCacheSize(size_t size_mb);
    
    // Statistics
    void printStatistics();
    
private:
    std::shared_ptr<grpc::Channel> channel_;
    std::shared_ptr<FileService::Stub> file_service_;
    std::shared_ptr<CacheManager> cache_manager_;
    std::unique_ptr<Uploader> uploader_;
    std::unique_ptr<Downloader> downloader_;
    
    bool verbose_logging_;
    
    // Helper methods
    void printProgressBar(int64_t current, int64_t total, const std::string& operation);
    std::string formatFileSize(int64_t bytes);
    std::string formatDuration(int64_t milliseconds);
};

} // namespace dfs