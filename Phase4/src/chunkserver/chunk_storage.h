#pragma once

#include "file_system.pb.h"
#include "file_system.grpc.pb.h"
#include "utils.h"
#include "crypto.h"
#include "erasure_coding.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <memory>

namespace dfs {

// Chunk storage manager
class ChunkStorage {
public:
    ChunkStorage(const std::string& storage_directory);
    ~ChunkStorage();
    
    // Core operations
    bool writeChunk(const std::string& chunk_id, 
                   const std::vector<uint8_t>& data,
                   bool is_encrypted = false,
                   bool is_erasure_coded = false);
    
    std::vector<uint8_t> readChunk(const std::string& chunk_id);
    
    bool deleteChunk(const std::string& chunk_id);
    
    bool chunkExists(const std::string& chunk_id) const;
    
    // Integrity checking
    bool verifyChunkIntegrity(const std::string& chunk_id);
    std::string getChunkChecksum(const std::string& chunk_id);
    
    // Statistics
    int64_t getTotalStorageUsed() const;
    int64_t getAvailableStorage() const;
    int getChunkCount() const;
    std::vector<std::string> getAllChunkIds() const;
    
    // Maintenance
    void performGarbageCollection();
    void rebuildChecksumIndex();
    
private:
    std::string storage_directory_;
    std::string checksum_index_file_;
    
    mutable std::shared_mutex storage_mutex_;
    std::unordered_map<std::string, std::string> chunk_checksums_;
    std::unordered_set<std::string> stored_chunks_;
    
    // Helper methods
    std::string getChunkFilePath(const std::string& chunk_id) const;
    std::string getChunkMetadataPath(const std::string& chunk_id) const;
    bool saveChecksumIndex();
    bool loadChecksumIndex();
    bool saveChunkMetadata(const std::string& chunk_id, 
                          const std::string& checksum,
                          bool is_encrypted,
                          bool is_erasure_coded);
    bool loadChunkMetadata(const std::string& chunk_id,
                          std::string& checksum,
                          bool& is_encrypted,
                          bool& is_erasure_coded);
    void updateStorageStats();
};

} // namespace dfs