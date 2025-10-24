#pragma once

#include "metadata_manager.h"
#include "utils.h"
#include <vector>
#include <string>
#include <memory>

namespace dfs {

// Chunk allocation strategy
enum class AllocationStrategy {
    ROUND_ROBIN,
    LEAST_LOADED,
    RANDOM,
    ZONE_AWARE
};

// Chunk allocator class
class ChunkAllocator {
public:
    ChunkAllocator(std::shared_ptr<MetadataManager> metadata_manager);
    
    // Allocate chunks for a new file
    std::vector<ChunkInfo> allocateChunks(const std::string& file_id,
                                         int64_t file_size,
                                         bool enable_erasure_coding = false);
    
    // Allocate servers for a specific chunk
    std::vector<std::string> allocateServersForChunk(const std::string& chunk_id,
                                                    int replication_factor = 3,
                                                    const std::vector<std::string>& exclude_servers = {});
    
    // Reallocate chunks due to server failure
    std::vector<std::string> reallocateChunk(const std::string& chunk_id,
                                            const std::vector<std::string>& failed_servers);
    
    // Load balancing
    bool shouldRebalance() const;
    std::vector<ReplicationTask> generateRebalancingTasks();
    
    // Strategy management
    void setAllocationStrategy(AllocationStrategy strategy) { strategy_ = strategy; }
    AllocationStrategy getAllocationStrategy() const { return strategy_; }
    
    // Zone awareness (for future expansion)
    void setServerZone(const std::string& server_id, const std::string& zone);
    std::string getServerZone(const std::string& server_id) const;
    
private:
    std::shared_ptr<MetadataManager> metadata_manager_;
    AllocationStrategy strategy_;
    std::unordered_map<std::string, std::string> server_zones_;
    mutable std::mutex allocation_mutex_;
    
    // Allocation algorithms
    std::vector<std::string> allocateRoundRobin(int count, 
                                               const std::vector<std::string>& exclude = {});
    std::vector<std::string> allocateLeastLoaded(int count, 
                                                const std::vector<std::string>& exclude = {});
    std::vector<std::string> allocateRandom(int count, 
                                           const std::vector<std::string>& exclude = {});
    std::vector<std::string> allocateZoneAware(int count, 
                                              const std::vector<std::string>& exclude = {});
    
    // Helper functions
    std::vector<ServerMetadata> getAvailableServers(const std::vector<std::string>& exclude = {}) const;
    double calculateServerLoad(const ServerMetadata& server) const;
    bool hasEnoughSpace(const ServerMetadata& server, int64_t required_space) const;
    int calculateChunkCount(int64_t file_size) const;
    
    // Load balancing helpers
    double calculateClusterLoadVariance() const;
    std::vector<std::pair<std::string, std::string>> findOverloadedServers() const;
    std::vector<std::string> findUnderloadedServers() const;
    
    // Zone awareness helpers
    std::vector<std::string> getServersInZone(const std::string& zone, 
                                             const std::vector<std::string>& exclude = {}) const;
    std::string selectOptimalZone(const std::vector<std::string>& exclude_servers) const;
    
    // Round-robin state
    mutable size_t round_robin_index_;
};

} // namespace dfs