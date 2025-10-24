#pragma once

#include "file_system.pb.h"
#include "file_system.grpc.pb.h"
#include "utils.h"
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <memory>
#include <fstream>

namespace dfs {

// File metadata structure
struct FileMetadata {
    std::string file_id;
    std::string filename;
    int64_t size;
    int64_t created_time;
    int64_t modified_time;
    std::vector<std::string> chunk_ids;
    bool is_encrypted;
    std::string encryption_key_id;
    bool is_erasure_coded;
    std::string checksum;
};

// Chunk metadata structure
struct ChunkMetadata {
    std::string chunk_id;
    std::vector<std::string> server_locations;
    int64_t size;
    std::string checksum;
    bool is_erasure_coded;
    std::string erasure_group_id;
    int erasure_block_index;
    bool is_parity_block;
    int64_t created_time;
    int64_t last_accessed_time;
};

// Server metadata structure
struct ServerMetadata {
    std::string server_id;
    std::string address;
    int port;
    int64_t total_space;
    int64_t free_space;
    int chunk_count;
    double cpu_usage;
    double memory_usage;
    bool is_healthy;
    int64_t last_heartbeat;
    std::unordered_set<std::string> stored_chunks;
};

// Metadata manager class
class MetadataManager {
public:
    MetadataManager();
    ~MetadataManager();
    
    // File operations
    bool createFile(const std::string& filename, const FileMetadata& metadata);
    bool deleteFile(const std::string& filename);
    bool getFileMetadata(const std::string& filename, FileMetadata& metadata) const;
    std::vector<FileMetadata> listFiles(const std::string& path_prefix = "") const;
    bool updateFileMetadata(const std::string& filename, const FileMetadata& metadata);
    
    // Chunk operations
    bool addChunk(const std::string& chunk_id, const ChunkMetadata& metadata);
    bool removeChunk(const std::string& chunk_id);
    bool getChunkMetadata(const std::string& chunk_id, ChunkMetadata& metadata) const;
    std::vector<ChunkMetadata> getChunksForFile(const std::string& filename) const;
    bool updateChunkLocations(const std::string& chunk_id, const std::vector<std::string>& locations);
    
    // Server operations
    bool registerServer(const std::string& server_id, const ServerMetadata& metadata);
    bool unregisterServer(const std::string& server_id);
    bool updateServerMetadata(const std::string& server_id, const ServerMetadata& metadata);
    bool getServerMetadata(const std::string& server_id, ServerMetadata& metadata) const;
    std::vector<ServerMetadata> getAllServers() const;
    std::vector<ServerMetadata> getHealthyServers() const;
    
    // Chunk-server relationship
    bool addChunkToServer(const std::string& chunk_id, const std::string& server_id);
    bool removeChunkFromServer(const std::string& chunk_id, const std::string& server_id);
    std::vector<std::string> getServersForChunk(const std::string& chunk_id) const;
    std::vector<std::string> getChunksForServer(const std::string& server_id) const;
    
    // Persistence
    bool saveMetadataToFile(const std::string& filename);
    bool loadMetadataFromFile(const std::string& filename);
    
    // Health checking
    void markServerUnhealthy(const std::string& server_id);
    void markServerHealthy(const std::string& server_id);
    std::vector<std::string> getUnhealthyServers() const;
    
    // Statistics
    struct Statistics {
        int total_files;
        int total_chunks;
        int total_servers;
        int healthy_servers;
        int64_t total_storage_used;
        int64_t total_storage_available;
        double average_replication_factor;
    };
    
    Statistics getStatistics() const;
    
    // Cleanup
    void cleanupOrphanedChunks();
    void cleanupDeadServers();
    
private:
    mutable std::shared_mutex metadata_mutex_;
    
    // Metadata storage
    std::unordered_map<std::string, FileMetadata> files_;           // filename -> metadata
    std::unordered_map<std::string, std::string> file_id_to_name_;  // file_id -> filename
    std::unordered_map<std::string, ChunkMetadata> chunks_;         // chunk_id -> metadata
    std::unordered_map<std::string, ServerMetadata> servers_;       // server_id -> metadata
    
    // Relationship mappings
    std::unordered_map<std::string, std::unordered_set<std::string>> chunk_to_servers_;
    std::unordered_map<std::string, std::unordered_set<std::string>> server_to_chunks_;
    
    // Helper methods
    void removeChunkFromAllServers(const std::string& chunk_id);
    void removeAllChunksFromServer(const std::string& server_id);
    
    // Serialization helpers
    std::string serializeMetadata() const;
    bool deserializeMetadata(const std::string& data);
};

} // namespace dfs