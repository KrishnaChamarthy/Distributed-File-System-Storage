#include "chunk_allocator.h"
#include "erasure_coding.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <random>

namespace dfs {

ChunkAllocator::ChunkAllocator(std::shared_ptr<MetadataManager> metadata_manager)
    : metadata_manager_(metadata_manager), 
      strategy_(AllocationStrategy::LEAST_LOADED),
      round_robin_index_(0) {
    Utils::logInfo("ChunkAllocator initialized with LEAST_LOADED strategy");
}

std::vector<ChunkInfo> ChunkAllocator::allocateChunks(const std::string& file_id,
                                                     int64_t file_size,
                                                     bool enable_erasure_coding) {
    std::lock_guard<std::mutex> lock(allocation_mutex_);
    
    std::vector<ChunkInfo> allocated_chunks;
    
    if (enable_erasure_coding) {
        // Use erasure coding
        ErasureCodedChunkManager ec_manager;
        int data_blocks = ec_manager.getMinimumBlocksNeeded();
        int total_blocks = data_blocks + ERASURE_CODING_PARITY_BLOCKS;
        
        // Calculate how many erasure coding groups we need
        int64_t chunk_size = Config::getInstance().getChunkSize();
        int groups_needed = (file_size + chunk_size - 1) / chunk_size;
        
        for (int group = 0; group < groups_needed; ++group) {
            std::string group_id = file_id + "_group_" + std::to_string(group);
            
            // Allocate servers for all blocks in this group
            std::vector<std::string> exclude_servers;
            
            for (int block = 0; block < total_blocks; ++block) {
                std::string chunk_id = group_id + "_block_" + std::to_string(block);
                
                // Allocate one server for this block (no replication in EC)
                std::vector<std::string> servers = allocateServersForChunk(chunk_id, 1, exclude_servers);
                
                if (servers.empty()) {
                    Utils::logError("Failed to allocate server for erasure coded chunk: " + chunk_id);
                    continue;
                }
                
                ChunkInfo chunk_info;
                chunk_info.chunk_id = chunk_id;
                chunk_info.server_addresses = servers;
                chunk_info.size = chunk_size / data_blocks; // Each block is smaller
                chunk_info.is_erasure_coded = true;
                
                allocated_chunks.push_back(chunk_info);
                
                // Add this server to exclude list to ensure blocks are on different servers
                exclude_servers.insert(exclude_servers.end(), servers.begin(), servers.end());
            }
        }
    } else {
        // Traditional replication
        int chunk_count = calculateChunkCount(file_size);
        int replication_factor = Config::getInstance().getReplicationFactor();
        
        for (int i = 0; i < chunk_count; ++i) {
            std::string chunk_id = file_id + "_chunk_" + std::to_string(i);
            
            std::vector<std::string> servers = allocateServersForChunk(chunk_id, replication_factor);
            
            if (servers.size() < static_cast<size_t>(replication_factor)) {
                Utils::logWarning("Could only allocate " + std::to_string(servers.size()) + 
                                 " servers for chunk " + chunk_id + " (requested " + 
                                 std::to_string(replication_factor) + ")");
            }
            
            ChunkInfo chunk_info;
            chunk_info.chunk_id = chunk_id;
            chunk_info.server_addresses = servers;
            chunk_info.size = std::min(static_cast<int64_t>(CHUNK_SIZE), 
                                     file_size - i * CHUNK_SIZE);
            chunk_info.is_erasure_coded = false;
            
            allocated_chunks.push_back(chunk_info);
        }
    }
    
    Utils::logInfo("Allocated " + std::to_string(allocated_chunks.size()) + 
                   " chunks for file " + file_id + 
                   (enable_erasure_coding ? " (erasure coded)" : " (replicated)"));
    
    return allocated_chunks;
}

std::vector<std::string> ChunkAllocator::allocateServersForChunk(const std::string& chunk_id,
                                                                int replication_factor,
                                                                const std::vector<std::string>& exclude_servers) {
    std::vector<std::string> allocated_servers;
    
    switch (strategy_) {
        case AllocationStrategy::ROUND_ROBIN:
            allocated_servers = allocateRoundRobin(replication_factor, exclude_servers);
            break;
        case AllocationStrategy::LEAST_LOADED:
            allocated_servers = allocateLeastLoaded(replication_factor, exclude_servers);
            break;
        case AllocationStrategy::RANDOM:
            allocated_servers = allocateRandom(replication_factor, exclude_servers);
            break;
        case AllocationStrategy::ZONE_AWARE:
            allocated_servers = allocateZoneAware(replication_factor, exclude_servers);
            break;
    }
    
    // Create chunk metadata
    if (!allocated_servers.empty()) {
        ChunkMetadata chunk_metadata;
        chunk_metadata.chunk_id = chunk_id;
        chunk_metadata.server_locations = allocated_servers;
        chunk_metadata.size = 0; // Will be set when chunk is actually written
        chunk_metadata.created_time = Utils::getCurrentTimestamp();
        chunk_metadata.last_accessed_time = chunk_metadata.created_time;
        chunk_metadata.is_erasure_coded = false; // Will be updated if needed
        
        metadata_manager_->addChunk(chunk_id, chunk_metadata);
    }
    
    return allocated_servers;
}

std::vector<std::string> ChunkAllocator::reallocateChunk(const std::string& chunk_id,
                                                        const std::vector<std::string>& failed_servers) {
    ChunkMetadata chunk_metadata;
    if (!metadata_manager_->getChunkMetadata(chunk_id, chunk_metadata)) {
        Utils::logError("Chunk not found for reallocation: " + chunk_id);
        return {};
    }
    
    // Remove failed servers from current locations
    std::vector<std::string> current_servers = chunk_metadata.server_locations;
    for (const std::string& failed_server : failed_servers) {
        current_servers.erase(
            std::remove(current_servers.begin(), current_servers.end(), failed_server),
            current_servers.end()
        );
    }
    
    // Calculate how many new servers we need
    int target_replication = chunk_metadata.is_erasure_coded ? 1 : 
                           Config::getInstance().getReplicationFactor();
    int servers_needed = target_replication - current_servers.size();
    
    if (servers_needed <= 0) {
        return current_servers; // Already have enough servers
    }
    
    // Allocate new servers (excluding current ones)
    std::vector<std::string> new_servers = allocateServersForChunk(
        chunk_id + "_realloc", servers_needed, current_servers);
    
    // Update chunk metadata
    std::vector<std::string> all_servers = current_servers;
    all_servers.insert(all_servers.end(), new_servers.begin(), new_servers.end());
    
    metadata_manager_->updateChunkLocations(chunk_id, all_servers);
    
    Utils::logInfo("Reallocated chunk " + chunk_id + " to " + 
                   std::to_string(new_servers.size()) + " new servers");
    
    return new_servers;
}

bool ChunkAllocator::shouldRebalance() const {
    // Check if load variance is too high
    double variance = calculateClusterLoadVariance();
    const double VARIANCE_THRESHOLD = 0.3; // 30% variance threshold
    
    if (variance > VARIANCE_THRESHOLD) {
        Utils::logInfo("Load variance (" + std::to_string(variance) + 
                      ") exceeds threshold, rebalancing recommended");
        return true;
    }
    
    // Check for severely overloaded servers
    auto overloaded = findOverloadedServers();
    if (!overloaded.empty()) {
        Utils::logInfo("Found " + std::to_string(overloaded.size()) + 
                      " overloaded servers, rebalancing recommended");
        return true;
    }
    
    return false;
}

std::vector<ReplicationTask> ChunkAllocator::generateRebalancingTasks() {
    std::vector<ReplicationTask> tasks;
    
    auto overloaded_servers = findOverloadedServers();
    auto underloaded_servers = findUnderloadedServers();
    
    if (overloaded_servers.empty() || underloaded_servers.empty()) {
        return tasks;
    }
    
    for (const auto& overloaded_pair : overloaded_servers) {
        const std::string& source_server = overloaded_pair.first;
        const std::string& chunk_to_move = overloaded_pair.second;
        
        if (!underloaded_servers.empty()) {
            const std::string& target_server = underloaded_servers.back();
            underloaded_servers.pop_back();
            
            ReplicationTask task;
            task.chunk_id = chunk_to_move;
            task.source_server = source_server;
            task.target_server = target_server;
            task.is_urgent = false;
            
            tasks.push_back(task);
            
            Utils::logInfo("Generated rebalancing task: move chunk " + chunk_to_move + 
                          " from " + source_server + " to " + target_server);
        }
    }
    
    return tasks;
}

void ChunkAllocator::setServerZone(const std::string& server_id, const std::string& zone) {
    std::lock_guard<std::mutex> lock(allocation_mutex_);
    server_zones_[server_id] = zone;
}

std::string ChunkAllocator::getServerZone(const std::string& server_id) const {
    std::lock_guard<std::mutex> lock(allocation_mutex_);
    auto it = server_zones_.find(server_id);
    return (it != server_zones_.end()) ? it->second : "default";
}

std::vector<std::string> ChunkAllocator::allocateRoundRobin(int count, 
                                                           const std::vector<std::string>& exclude) {
    auto available_servers = getAvailableServers(exclude);
    
    if (available_servers.empty()) {
        return {};
    }
    
    std::vector<std::string> result;
    
    for (int i = 0; i < count && !available_servers.empty(); ++i) {
        size_t index = round_robin_index_ % available_servers.size();
        result.push_back(available_servers[index].server_id);
        round_robin_index_++;
        
        // Remove selected server to avoid duplicates
        available_servers.erase(available_servers.begin() + index);
        if (round_robin_index_ >= available_servers.size() && !available_servers.empty()) {
            round_robin_index_ = 0;
        }
    }
    
    return result;
}

std::vector<std::string> ChunkAllocator::allocateLeastLoaded(int count, 
                                                            const std::vector<std::string>& exclude) {
    auto available_servers = getAvailableServers(exclude);
    
    // Sort by load (ascending)
    std::sort(available_servers.begin(), available_servers.end(),
              [this](const ServerMetadata& a, const ServerMetadata& b) {
                  return calculateServerLoad(a) < calculateServerLoad(b);
              });
    
    std::vector<std::string> result;
    for (int i = 0; i < count && i < static_cast<int>(available_servers.size()); ++i) {
        result.push_back(available_servers[i].server_id);
    }
    
    return result;
}

std::vector<std::string> ChunkAllocator::allocateRandom(int count, 
                                                       const std::vector<std::string>& exclude) {
    auto available_servers = getAvailableServers(exclude);
    
    if (available_servers.empty()) {
        return {};
    }
    
    // Shuffle the servers
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(available_servers.begin(), available_servers.end(), g);
    
    std::vector<std::string> result;
    for (int i = 0; i < count && i < static_cast<int>(available_servers.size()); ++i) {
        result.push_back(available_servers[i].server_id);
    }
    
    return result;
}

std::vector<std::string> ChunkAllocator::allocateZoneAware(int count, 
                                                          const std::vector<std::string>& exclude) {
    // Try to spread chunks across different zones
    std::vector<std::string> result;
    std::unordered_set<std::string> used_zones;
    auto available_servers = getAvailableServers(exclude);
    
    // First pass: try to get one server from each zone
    for (const auto& server : available_servers) {
        if (result.size() >= static_cast<size_t>(count)) break;
        
        std::string zone = getServerZone(server.server_id);
        if (used_zones.find(zone) == used_zones.end()) {
            result.push_back(server.server_id);
            used_zones.insert(zone);
        }
    }
    
    // Second pass: fill remaining slots with least loaded servers
    if (result.size() < static_cast<size_t>(count)) {
        std::vector<std::string> already_selected = result;
        auto remaining = allocateLeastLoaded(count - result.size(), 
                                           [&](const std::vector<std::string>& base_exclude) {
                                               std::vector<std::string> extended_exclude = base_exclude;
                                               extended_exclude.insert(extended_exclude.end(),
                                                                     already_selected.begin(),
                                                                     already_selected.end());
                                               return extended_exclude;
                                           }(exclude));
        
        result.insert(result.end(), remaining.begin(), remaining.end());
    }
    
    return result;
}

std::vector<ServerMetadata> ChunkAllocator::getAvailableServers(const std::vector<std::string>& exclude) const {
    auto all_servers = metadata_manager_->getHealthyServers();
    std::vector<ServerMetadata> available;
    
    std::unordered_set<std::string> exclude_set(exclude.begin(), exclude.end());
    
    for (const auto& server : all_servers) {
        if (exclude_set.find(server.server_id) == exclude_set.end() &&
            hasEnoughSpace(server, CHUNK_SIZE)) {
            available.push_back(server);
        }
    }
    
    return available;
}

double ChunkAllocator::calculateServerLoad(const ServerMetadata& server) const {
    // Calculate load as a combination of storage usage, CPU, and memory
    double storage_load = 1.0 - (static_cast<double>(server.free_space) / server.total_space);
    double cpu_load = server.cpu_usage / 100.0;
    double memory_load = server.memory_usage / 100.0;
    
    // Weighted combination
    return 0.5 * storage_load + 0.3 * cpu_load + 0.2 * memory_load;
}

bool ChunkAllocator::hasEnoughSpace(const ServerMetadata& server, int64_t required_space) const {
    // Ensure at least 10% free space remains after allocation
    int64_t space_after_allocation = server.free_space - required_space;
    int64_t min_free_space = server.total_space / 10;
    
    return space_after_allocation >= min_free_space;
}

int ChunkAllocator::calculateChunkCount(int64_t file_size) const {
    int64_t chunk_size = Config::getInstance().getChunkSize();
    return (file_size + chunk_size - 1) / chunk_size;
}

double ChunkAllocator::calculateClusterLoadVariance() const {
    auto servers = metadata_manager_->getHealthyServers();
    
    if (servers.size() < 2) {
        return 0.0;
    }
    
    std::vector<double> loads;
    for (const auto& server : servers) {
        loads.push_back(calculateServerLoad(server));
    }
    
    double mean = std::accumulate(loads.begin(), loads.end(), 0.0) / loads.size();
    
    double variance = 0.0;
    for (double load : loads) {
        variance += (load - mean) * (load - mean);
    }
    variance /= loads.size();
    
    return std::sqrt(variance);
}

std::vector<std::pair<std::string, std::string>> ChunkAllocator::findOverloadedServers() const {
    std::vector<std::pair<std::string, std::string>> overloaded;
    auto servers = metadata_manager_->getHealthyServers();
    
    const double OVERLOAD_THRESHOLD = 0.8; // 80% load threshold
    
    for (const auto& server : servers) {
        double load = calculateServerLoad(server);
        if (load > OVERLOAD_THRESHOLD) {
            // Find a chunk to move from this server
            auto chunks = metadata_manager_->getChunksForServer(server.server_id);
            if (!chunks.empty()) {
                // Pick the least recently accessed chunk
                std::string chunk_to_move = *std::min_element(chunks.begin(), chunks.end(),
                    [this](const std::string& a, const std::string& b) {
                        ChunkMetadata meta_a, meta_b;
                        metadata_manager_->getChunkMetadata(a, meta_a);
                        metadata_manager_->getChunkMetadata(b, meta_b);
                        return meta_a.last_accessed_time < meta_b.last_accessed_time;
                    });
                
                overloaded.emplace_back(server.server_id, chunk_to_move);
            }
        }
    }
    
    return overloaded;
}

std::vector<std::string> ChunkAllocator::findUnderloadedServers() const {
    std::vector<std::string> underloaded;
    auto servers = metadata_manager_->getHealthyServers();
    
    const double UNDERLOAD_THRESHOLD = 0.3; // 30% load threshold
    
    for (const auto& server : servers) {
        double load = calculateServerLoad(server);
        if (load < UNDERLOAD_THRESHOLD) {
            underloaded.push_back(server.server_id);
        }
    }
    
    return underloaded;
}

std::vector<std::string> ChunkAllocator::getServersInZone(const std::string& zone, 
                                                         const std::vector<std::string>& exclude) const {
    std::vector<std::string> servers_in_zone;
    auto available_servers = getAvailableServers(exclude);
    
    for (const auto& server : available_servers) {
        if (getServerZone(server.server_id) == zone) {
            servers_in_zone.push_back(server.server_id);
        }
    }
    
    return servers_in_zone;
}

std::string ChunkAllocator::selectOptimalZone(const std::vector<std::string>& exclude_servers) const {
    // Select zone with most available capacity
    std::unordered_map<std::string, int64_t> zone_capacity;
    auto available_servers = getAvailableServers(exclude_servers);
    
    for (const auto& server : available_servers) {
        std::string zone = getServerZone(server.server_id);
        zone_capacity[zone] += server.free_space;
    }
    
    if (zone_capacity.empty()) {
        return "default";
    }
    
    return std::max_element(zone_capacity.begin(), zone_capacity.end(),
                           [](const auto& a, const auto& b) {
                               return a.second < b.second;
                           })->first;
}

} // namespace dfs