#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <random>
#include <map>

namespace dfs {

// Configuration constants
constexpr size_t CHUNK_SIZE = 4 * 1024 * 1024; // 4MB chunks
constexpr int DEFAULT_REPLICATION_FACTOR = 3;
constexpr int ERASURE_CODING_DATA_BLOCKS = 4;
constexpr int ERASURE_CODING_PARITY_BLOCKS = 2;
constexpr int HEARTBEAT_INTERVAL_MS = 5000;
constexpr int HEARTBEAT_TIMEOUT_MS = 15000;
constexpr int MASTER_ELECTION_TIMEOUT_MS = 5000;
constexpr int CACHE_SIZE_MB = 100;

// Utility functions
class Utils {
public:
    // Generate unique IDs
    static std::string generateChunkId();
    static std::string generateFileId();
    static std::string generateServerId();
    
    // Hash functions
    static std::string calculateSHA256(const std::vector<uint8_t>& data);
    static std::string calculateSHA256(const std::string& data);
    
    // File system utilities
    static bool fileExists(const std::string& path);
    static bool createDirectory(const std::string& path);
    static std::vector<uint8_t> readFile(const std::string& path);
    static bool writeFile(const std::string& path, const std::vector<uint8_t>& data);
    static int64_t getFileSize(const std::string& path);
    static bool deleteFile(const std::string& path);
    
    // Network utilities
    static std::vector<std::string> splitString(const std::string& str, char delimiter);
    static std::string joinStrings(const std::vector<std::string>& strings, const std::string& delimiter);
    static bool isValidIPAddress(const std::string& ip);
    static bool isPortOpen(const std::string& host, int port);
    
    // Time utilities
    static int64_t getCurrentTimestamp();
    static std::string timestampToString(int64_t timestamp);
    
    // Random utilities
    static int getRandomInt(int min, int max);
    static std::string getRandomString(int length);
    static std::vector<int> getRandomPermutation(int size);
    
    // Logging
    static void logInfo(const std::string& message);
    static void logWarning(const std::string& message);
    static void logError(const std::string& message);
    static void logDebug(const std::string& message);
    
private:
    static std::mt19937 rng_;
};

// Configuration management
class Config {
public:
    static Config& getInstance();
    
    // Load configuration from file
    bool loadFromFile(const std::string& configFile);
    
    // Getters
    int getReplicationFactor() const { return replication_factor_; }
    int getChunkSize() const { return chunk_size_; }
    int getHeartbeatInterval() const { return heartbeat_interval_; }
    int getHeartbeatTimeout() const { return heartbeat_timeout_; }
    bool isEncryptionEnabled() const { return encryption_enabled_; }
    bool isErasureCodingEnabled() const { return erasure_coding_enabled_; }
    const std::string& getDataDirectory() const { return data_directory_; }
    const std::string& getMasterAddress() const { return master_address_; }
    int getMasterPort() const { return master_port_; }
    const std::vector<std::string>& getMasterPeers() const { return master_peers_; }
    
    // Setters
    void setReplicationFactor(int factor) { replication_factor_ = factor; }
    void setChunkSize(int size) { chunk_size_ = size; }
    void setDataDirectory(const std::string& dir) { data_directory_ = dir; }
    void setMasterAddress(const std::string& addr) { master_address_ = addr; }
    void setMasterPort(int port) { master_port_ = port; }
    
private:
    Config() = default;
    
    int replication_factor_ = DEFAULT_REPLICATION_FACTOR;
    int chunk_size_ = CHUNK_SIZE;
    int heartbeat_interval_ = HEARTBEAT_INTERVAL_MS;
    int heartbeat_timeout_ = HEARTBEAT_TIMEOUT_MS;
    bool encryption_enabled_ = true;
    bool erasure_coding_enabled_ = false;
    std::string data_directory_ = "./data";
    std::string master_address_ = "localhost";
    int master_port_ = 50051;
    std::vector<std::string> master_peers_;
};

// Performance metrics
class Metrics {
public:
    static Metrics& getInstance();
    
    // Counter metrics
    void incrementChunksWritten() { chunks_written_++; }
    void incrementChunksRead() { chunks_read_++; }
    void incrementFilesUploaded() { files_uploaded_++; }
    void incrementFilesDownloaded() { files_downloaded_++; }
    void incrementReplicationTasks() { replication_tasks_++; }
    
    // Timing metrics
    void recordUploadTime(int64_t milliseconds);
    void recordDownloadTime(int64_t milliseconds);
    void recordReplicationTime(int64_t milliseconds);
    
    // Resource metrics
    void updateDiskUsage(int64_t bytes) { disk_usage_ = bytes; }
    void updateMemoryUsage(double percentage) { memory_usage_ = percentage; }
    void updateCpuUsage(double percentage) { cpu_usage_ = percentage; }
    
    // Getters
    int64_t getChunksWritten() const { return chunks_written_; }
    int64_t getChunksRead() const { return chunks_read_; }
    int64_t getFilesUploaded() const { return files_uploaded_; }
    int64_t getFilesDownloaded() const { return files_downloaded_; }
    double getAverageUploadTime() const;
    double getAverageDownloadTime() const;
    int64_t getDiskUsage() const { return disk_usage_; }
    double getMemoryUsage() const { return memory_usage_; }
    double getCpuUsage() const { return cpu_usage_; }
    
    // Export metrics as JSON
    std::string toJSON() const;
    
private:
    Metrics() = default;
    
    std::atomic<int64_t> chunks_written_{0};
    std::atomic<int64_t> chunks_read_{0};
    std::atomic<int64_t> files_uploaded_{0};
    std::atomic<int64_t> files_downloaded_{0};
    std::atomic<int64_t> replication_tasks_{0};
    
    std::vector<int64_t> upload_times_;
    std::vector<int64_t> download_times_;
    std::vector<int64_t> replication_times_;
    std::mutex timing_mutex_;
    
    std::atomic<int64_t> disk_usage_{0};
    std::atomic<double> memory_usage_{0.0};
    std::atomic<double> cpu_usage_{0.0};
};

} // namespace dfs