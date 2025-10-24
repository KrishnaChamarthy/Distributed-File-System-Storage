#include "metadata_manager.h"
#include <algorithm>
#include <sstream>
#include <fstream>
#include <json/json.h>
#include <shared_mutex>

namespace dfs {

MetadataManager::MetadataManager() {
    Utils::logInfo("MetadataManager initialized");
}

MetadataManager::~MetadataManager() {
    Utils::logInfo("MetadataManager destroyed");
}

bool MetadataManager::createFile(const std::string& filename, const FileMetadata& metadata) {
    std::unique_lock<std::shared_mutex> lock(metadata_mutex_);
    
    if (files_.find(filename) != files_.end()) {
        Utils::logWarning("File already exists: " + filename);
        return false;
    }
    
    files_[filename] = metadata;
    file_id_to_name_[metadata.file_id] = filename;
    
    Utils::logInfo("Created file: " + filename + " with ID: " + metadata.file_id);
    return true;
}

bool MetadataManager::deleteFile(const std::string& filename) {
    std::unique_lock<std::shared_mutex> lock(metadata_mutex_);
    
    auto it = files_.find(filename);
    if (it == files_.end()) {
        Utils::logWarning("File not found for deletion: " + filename);
        return false;
    }
    
    const FileMetadata& metadata = it->second;
    
    // Remove all chunks associated with this file
    for (const std::string& chunk_id : metadata.chunk_ids) {
        removeChunkFromAllServers(chunk_id);
        chunks_.erase(chunk_id);
    }
    
    file_id_to_name_.erase(metadata.file_id);
    files_.erase(it);
    
    Utils::logInfo("Deleted file: " + filename);
    return true;
}

bool MetadataManager::getFileMetadata(const std::string& filename, FileMetadata& metadata) const {
    std::shared_lock<std::shared_mutex> lock(metadata_mutex_);
    
    auto it = files_.find(filename);
    if (it == files_.end()) {
        return false;
    }
    
    metadata = it->second;
    return true;
}

std::vector<FileMetadata> MetadataManager::listFiles(const std::string& path_prefix) const {
    std::shared_lock<std::shared_mutex> lock(metadata_mutex_);
    
    std::vector<FileMetadata> result;
    for (const auto& pair : files_) {
        if (path_prefix.empty() || pair.first.find(path_prefix) == 0) {
            result.push_back(pair.second);
        }
    }
    
    return result;
}

bool MetadataManager::updateFileMetadata(const std::string& filename, const FileMetadata& metadata) {
    std::unique_lock<std::shared_mutex> lock(metadata_mutex_);
    
    auto it = files_.find(filename);
    if (it == files_.end()) {
        return false;
    }
    
    it->second = metadata;
    return true;
}

bool MetadataManager::addChunk(const std::string& chunk_id, const ChunkMetadata& metadata) {
    std::unique_lock<std::shared_mutex> lock(metadata_mutex_);
    
    chunks_[chunk_id] = metadata;
    
    // Update chunk-server relationships
    for (const std::string& server_id : metadata.server_locations) {
        chunk_to_servers_[chunk_id].insert(server_id);
        server_to_chunks_[server_id].insert(chunk_id);
        
        // Update server's chunk count
        auto server_it = servers_.find(server_id);
        if (server_it != servers_.end()) {
            server_it->second.stored_chunks.insert(chunk_id);
            server_it->second.chunk_count = server_it->second.stored_chunks.size();
        }
    }
    
    Utils::logDebug("Added chunk: " + chunk_id + " to " + 
                   std::to_string(metadata.server_locations.size()) + " servers");
    return true;
}

bool MetadataManager::removeChunk(const std::string& chunk_id) {
    std::unique_lock<std::shared_mutex> lock(metadata_mutex_);
    
    auto it = chunks_.find(chunk_id);
    if (it == chunks_.end()) {
        return false;
    }
    
    removeChunkFromAllServers(chunk_id);
    chunks_.erase(it);
    
    Utils::logDebug("Removed chunk: " + chunk_id);
    return true;
}

bool MetadataManager::getChunkMetadata(const std::string& chunk_id, ChunkMetadata& metadata) const {
    std::shared_lock<std::shared_mutex> lock(metadata_mutex_);
    
    auto it = chunks_.find(chunk_id);
    if (it == chunks_.end()) {
        return false;
    }
    
    metadata = it->second;
    return true;
}

std::vector<ChunkMetadata> MetadataManager::getChunksForFile(const std::string& filename) const {
    std::shared_lock<std::shared_mutex> lock(metadata_mutex_);
    
    std::vector<ChunkMetadata> result;
    
    auto file_it = files_.find(filename);
    if (file_it == files_.end()) {
        return result;
    }
    
    for (const std::string& chunk_id : file_it->second.chunk_ids) {
        auto chunk_it = chunks_.find(chunk_id);
        if (chunk_it != chunks_.end()) {
            result.push_back(chunk_it->second);
        }
    }
    
    return result;
}

bool MetadataManager::updateChunkLocations(const std::string& chunk_id, 
                                          const std::vector<std::string>& locations) {
    std::unique_lock<std::shared_mutex> lock(metadata_mutex_);
    
    auto it = chunks_.find(chunk_id);
    if (it == chunks_.end()) {
        return false;
    }
    
    // Remove old relationships
    removeChunkFromAllServers(chunk_id);
    
    // Add new relationships
    it->second.server_locations = locations;
    for (const std::string& server_id : locations) {
        chunk_to_servers_[chunk_id].insert(server_id);
        server_to_chunks_[server_id].insert(chunk_id);
        
        // Update server's chunk count
        auto server_it = servers_.find(server_id);
        if (server_it != servers_.end()) {
            server_it->second.stored_chunks.insert(chunk_id);
            server_it->second.chunk_count = server_it->second.stored_chunks.size();
        }
    }
    
    return true;
}

bool MetadataManager::registerServer(const std::string& server_id, const ServerMetadata& metadata) {
    std::unique_lock<std::shared_mutex> lock(metadata_mutex_);
    
    servers_[server_id] = metadata;
    server_to_chunks_[server_id]; // Initialize empty set
    
    Utils::logInfo("Registered server: " + server_id + " at " + 
                   metadata.address + ":" + std::to_string(metadata.port));
    return true;
}

bool MetadataManager::unregisterServer(const std::string& server_id) {
    std::unique_lock<std::shared_mutex> lock(metadata_mutex_);
    
    auto it = servers_.find(server_id);
    if (it == servers_.end()) {
        return false;
    }
    
    removeAllChunksFromServer(server_id);
    servers_.erase(it);
    server_to_chunks_.erase(server_id);
    
    Utils::logInfo("Unregistered server: " + server_id);
    return true;
}

bool MetadataManager::updateServerMetadata(const std::string& server_id, const ServerMetadata& metadata) {
    std::unique_lock<std::shared_mutex> lock(metadata_mutex_);
    
    auto it = servers_.find(server_id);
    if (it == servers_.end()) {
        return false;
    }
    
    it->second = metadata;
    return true;
}

bool MetadataManager::getServerMetadata(const std::string& server_id, ServerMetadata& metadata) const {
    std::shared_lock<std::shared_mutex> lock(metadata_mutex_);
    
    auto it = servers_.find(server_id);
    if (it == servers_.end()) {
        return false;
    }
    
    metadata = it->second;
    return true;
}

std::vector<ServerMetadata> MetadataManager::getAllServers() const {
    std::shared_lock<std::shared_mutex> lock(metadata_mutex_);
    
    std::vector<ServerMetadata> result;
    for (const auto& pair : servers_) {
        result.push_back(pair.second);
    }
    
    return result;
}

std::vector<ServerMetadata> MetadataManager::getHealthyServers() const {
    std::shared_lock<std::shared_mutex> lock(metadata_mutex_);
    
    std::vector<ServerMetadata> result;
    for (const auto& pair : servers_) {
        if (pair.second.is_healthy) {
            result.push_back(pair.second);
        }
    }
    
    return result;
}

bool MetadataManager::addChunkToServer(const std::string& chunk_id, const std::string& server_id) {
    std::unique_lock<std::shared_mutex> lock(metadata_mutex_);
    
    chunk_to_servers_[chunk_id].insert(server_id);
    server_to_chunks_[server_id].insert(chunk_id);
    
    // Update chunk metadata
    auto chunk_it = chunks_.find(chunk_id);
    if (chunk_it != chunks_.end()) {
        auto& locations = chunk_it->second.server_locations;
        if (std::find(locations.begin(), locations.end(), server_id) == locations.end()) {
            locations.push_back(server_id);
        }
    }
    
    // Update server metadata
    auto server_it = servers_.find(server_id);
    if (server_it != servers_.end()) {
        server_it->second.stored_chunks.insert(chunk_id);
        server_it->second.chunk_count = server_it->second.stored_chunks.size();
    }
    
    return true;
}

bool MetadataManager::removeChunkFromServer(const std::string& chunk_id, const std::string& server_id) {
    std::unique_lock<std::shared_mutex> lock(metadata_mutex_);
    
    chunk_to_servers_[chunk_id].erase(server_id);
    server_to_chunks_[server_id].erase(chunk_id);
    
    // Update chunk metadata
    auto chunk_it = chunks_.find(chunk_id);
    if (chunk_it != chunks_.end()) {
        auto& locations = chunk_it->second.server_locations;
        locations.erase(std::remove(locations.begin(), locations.end(), server_id), 
                       locations.end());
    }
    
    // Update server metadata
    auto server_it = servers_.find(server_id);
    if (server_it != servers_.end()) {
        server_it->second.stored_chunks.erase(chunk_id);
        server_it->second.chunk_count = server_it->second.stored_chunks.size();
    }
    
    return true;
}

std::vector<std::string> MetadataManager::getServersForChunk(const std::string& chunk_id) const {
    std::shared_lock<std::shared_mutex> lock(metadata_mutex_);
    
    auto it = chunk_to_servers_.find(chunk_id);
    if (it == chunk_to_servers_.end()) {
        return {};
    }
    
    return std::vector<std::string>(it->second.begin(), it->second.end());
}

std::vector<std::string> MetadataManager::getChunksForServer(const std::string& server_id) const {
    std::shared_lock<std::shared_mutex> lock(metadata_mutex_);
    
    auto it = server_to_chunks_.find(server_id);
    if (it == server_to_chunks_.end()) {
        return {};
    }
    
    return std::vector<std::string>(it->second.begin(), it->second.end());
}

void MetadataManager::markServerUnhealthy(const std::string& server_id) {
    std::unique_lock<std::shared_mutex> lock(metadata_mutex_);
    
    auto it = servers_.find(server_id);
    if (it != servers_.end()) {
        it->second.is_healthy = false;
        Utils::logWarning("Marked server as unhealthy: " + server_id);
    }
}

void MetadataManager::markServerHealthy(const std::string& server_id) {
    std::unique_lock<std::shared_mutex> lock(metadata_mutex_);
    
    auto it = servers_.find(server_id);
    if (it != servers_.end()) {
        it->second.is_healthy = true;
        it->second.last_heartbeat = Utils::getCurrentTimestamp();
        Utils::logInfo("Marked server as healthy: " + server_id);
    }
}

std::vector<std::string> MetadataManager::getUnhealthyServers() const {
    std::shared_lock<std::shared_mutex> lock(metadata_mutex_);
    
    std::vector<std::string> result;
    for (const auto& pair : servers_) {
        if (!pair.second.is_healthy) {
            result.push_back(pair.first);
        }
    }
    
    return result;
}

MetadataManager::Statistics MetadataManager::getStatistics() const {
    std::shared_lock<std::shared_mutex> lock(metadata_mutex_);
    
    Statistics stats = {};
    stats.total_files = files_.size();
    stats.total_chunks = chunks_.size();
    stats.total_servers = servers_.size();
    
    int64_t total_replicas = 0;
    for (const auto& pair : servers_) {
        if (pair.second.is_healthy) {
            stats.healthy_servers++;
        }
        stats.total_storage_used += (pair.second.total_space - pair.second.free_space);
        stats.total_storage_available += pair.second.free_space;
    }
    
    for (const auto& pair : chunks_) {
        total_replicas += pair.second.server_locations.size();
    }
    
    if (stats.total_chunks > 0) {
        stats.average_replication_factor = static_cast<double>(total_replicas) / stats.total_chunks;
    }
    
    return stats;
}

void MetadataManager::cleanupOrphanedChunks() {
    std::unique_lock<std::shared_mutex> lock(metadata_mutex_);
    
    std::vector<std::string> chunks_to_remove;
    
    for (const auto& pair : chunks_) {
        const std::string& chunk_id = pair.first;
        
        // Check if chunk belongs to any file
        bool belongs_to_file = false;
        for (const auto& file_pair : files_) {
            const auto& chunk_ids = file_pair.second.chunk_ids;
            if (std::find(chunk_ids.begin(), chunk_ids.end(), chunk_id) != chunk_ids.end()) {
                belongs_to_file = true;
                break;
            }
        }
        
        if (!belongs_to_file) {
            chunks_to_remove.push_back(chunk_id);
        }
    }
    
    for (const std::string& chunk_id : chunks_to_remove) {
        removeChunk(chunk_id);
        Utils::logInfo("Cleaned up orphaned chunk: " + chunk_id);
    }
}

void MetadataManager::cleanupDeadServers() {
    std::unique_lock<std::shared_mutex> lock(metadata_mutex_);
    
    int64_t current_time = Utils::getCurrentTimestamp();
    int64_t timeout = Config::getInstance().getHeartbeatTimeout();
    
    std::vector<std::string> servers_to_remove;
    
    for (const auto& pair : servers_) {
        const std::string& server_id = pair.first;
        const ServerMetadata& metadata = pair.second;
        
        if (!metadata.is_healthy && 
            (current_time - metadata.last_heartbeat) > timeout * 2) {
            servers_to_remove.push_back(server_id);
        }
    }
    
    for (const std::string& server_id : servers_to_remove) {
        unregisterServer(server_id);
        Utils::logInfo("Cleaned up dead server: " + server_id);
    }
}

bool MetadataManager::saveMetadataToFile(const std::string& filename) {
    std::shared_lock<std::shared_mutex> lock(metadata_mutex_);
    
    std::string serialized = serializeMetadata();
    
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        Utils::logError("Failed to open metadata file for writing: " + filename);
        return false;
    }
    
    file.write(serialized.c_str(), serialized.size());
    
    if (!file.good()) {
        Utils::logError("Failed to write metadata to file: " + filename);
        return false;
    }
    
    Utils::logInfo("Saved metadata to file: " + filename);
    return true;
}

bool MetadataManager::loadMetadataFromFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        Utils::logWarning("Metadata file not found: " + filename);
        return false;
    }
    
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::string data(size, '\0');
    file.read(&data[0], size);
    
    if (!file.good()) {
        Utils::logError("Failed to read metadata from file: " + filename);
        return false;
    }
    
    std::unique_lock<std::shared_mutex> lock(metadata_mutex_);
    
    if (!deserializeMetadata(data)) {
        Utils::logError("Failed to deserialize metadata from file: " + filename);
        return false;
    }
    
    Utils::logInfo("Loaded metadata from file: " + filename);
    return true;
}

void MetadataManager::removeChunkFromAllServers(const std::string& chunk_id) {
    auto it = chunk_to_servers_.find(chunk_id);
    if (it != chunk_to_servers_.end()) {
        for (const std::string& server_id : it->second) {
            server_to_chunks_[server_id].erase(chunk_id);
            
            // Update server metadata
            auto server_it = servers_.find(server_id);
            if (server_it != servers_.end()) {
                server_it->second.stored_chunks.erase(chunk_id);
                server_it->second.chunk_count = server_it->second.stored_chunks.size();
            }
        }
        chunk_to_servers_.erase(it);
    }
}

void MetadataManager::removeAllChunksFromServer(const std::string& server_id) {
    auto it = server_to_chunks_.find(server_id);
    if (it != server_to_chunks_.end()) {
        for (const std::string& chunk_id : it->second) {
            chunk_to_servers_[chunk_id].erase(server_id);
            
            // Update chunk metadata
            auto chunk_it = chunks_.find(chunk_id);
            if (chunk_it != chunks_.end()) {
                auto& locations = chunk_it->second.server_locations;
                locations.erase(std::remove(locations.begin(), locations.end(), server_id), 
                               locations.end());
            }
        }
        it->second.clear();
    }
}

std::string MetadataManager::serializeMetadata() const {
    Json::Value root;
    
    // Serialize files
    Json::Value files_json(Json::arrayValue);
    for (const auto& pair : files_) {
        Json::Value file_json;
        const FileMetadata& metadata = pair.second;
        
        file_json["filename"] = pair.first;
        file_json["file_id"] = metadata.file_id;
        file_json["size"] = static_cast<Json::Int64>(metadata.size);
        file_json["created_time"] = static_cast<Json::Int64>(metadata.created_time);
        file_json["modified_time"] = static_cast<Json::Int64>(metadata.modified_time);
        file_json["is_encrypted"] = metadata.is_encrypted;
        file_json["encryption_key_id"] = metadata.encryption_key_id;
        file_json["is_erasure_coded"] = metadata.is_erasure_coded;
        file_json["checksum"] = metadata.checksum;
        
        Json::Value chunks_json(Json::arrayValue);
        for (const std::string& chunk_id : metadata.chunk_ids) {
            chunks_json.append(chunk_id);
        }
        file_json["chunk_ids"] = chunks_json;
        
        files_json.append(file_json);
    }
    root["files"] = files_json;
    
    // Serialize chunks
    Json::Value chunks_json(Json::arrayValue);
    for (const auto& pair : chunks_) {
        Json::Value chunk_json;
        const ChunkMetadata& metadata = pair.second;
        
        chunk_json["chunk_id"] = pair.first;
        chunk_json["size"] = static_cast<Json::Int64>(metadata.size);
        chunk_json["checksum"] = metadata.checksum;
        chunk_json["is_erasure_coded"] = metadata.is_erasure_coded;
        chunk_json["erasure_group_id"] = metadata.erasure_group_id;
        chunk_json["erasure_block_index"] = metadata.erasure_block_index;
        chunk_json["is_parity_block"] = metadata.is_parity_block;
        chunk_json["created_time"] = static_cast<Json::Int64>(metadata.created_time);
        chunk_json["last_accessed_time"] = static_cast<Json::Int64>(metadata.last_accessed_time);
        
        Json::Value servers_json(Json::arrayValue);
        for (const std::string& server_id : metadata.server_locations) {
            servers_json.append(server_id);
        }
        chunk_json["server_locations"] = servers_json;
        
        chunks_json.append(chunk_json);
    }
    root["chunks"] = chunks_json;
    
    // Serialize servers
    Json::Value servers_json(Json::arrayValue);
    for (const auto& pair : servers_) {
        Json::Value server_json;
        const ServerMetadata& metadata = pair.second;
        
        server_json["server_id"] = pair.first;
        server_json["address"] = metadata.address;
        server_json["port"] = metadata.port;
        server_json["total_space"] = static_cast<Json::Int64>(metadata.total_space);
        server_json["free_space"] = static_cast<Json::Int64>(metadata.free_space);
        server_json["chunk_count"] = metadata.chunk_count;
        server_json["cpu_usage"] = metadata.cpu_usage;
        server_json["memory_usage"] = metadata.memory_usage;
        server_json["is_healthy"] = metadata.is_healthy;
        server_json["last_heartbeat"] = static_cast<Json::Int64>(metadata.last_heartbeat);
        
        servers_json.append(server_json);
    }
    root["servers"] = servers_json;
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, root);
}

bool MetadataManager::deserializeMetadata(const std::string& data) {
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    std::string errors;
    
    if (!reader->parse(data.c_str(), data.c_str() + data.size(), &root, &errors)) {
        Utils::logError("Failed to parse metadata JSON: " + errors);
        return false;
    }
    
    // Clear existing metadata
    files_.clear();
    file_id_to_name_.clear();
    chunks_.clear();
    servers_.clear();
    chunk_to_servers_.clear();
    server_to_chunks_.clear();
    
    try {
        // Deserialize files
        const Json::Value& files_json = root["files"];
        for (const Json::Value& file_json : files_json) {
            FileMetadata metadata;
            std::string filename = file_json["filename"].asString();
            
            metadata.file_id = file_json["file_id"].asString();
            metadata.filename = filename;
            metadata.size = file_json["size"].asInt64();
            metadata.created_time = file_json["created_time"].asInt64();
            metadata.modified_time = file_json["modified_time"].asInt64();
            metadata.is_encrypted = file_json["is_encrypted"].asBool();
            metadata.encryption_key_id = file_json["encryption_key_id"].asString();
            metadata.is_erasure_coded = file_json["is_erasure_coded"].asBool();
            metadata.checksum = file_json["checksum"].asString();
            
            const Json::Value& chunks_json = file_json["chunk_ids"];
            for (const Json::Value& chunk_json : chunks_json) {
                metadata.chunk_ids.push_back(chunk_json.asString());
            }
            
            files_[filename] = metadata;
            file_id_to_name_[metadata.file_id] = filename;
        }
        
        // Deserialize chunks
        const Json::Value& chunks_json = root["chunks"];
        for (const Json::Value& chunk_json : chunks_json) {
            ChunkMetadata metadata;
            std::string chunk_id = chunk_json["chunk_id"].asString();
            
            metadata.chunk_id = chunk_id;
            metadata.size = chunk_json["size"].asInt64();
            metadata.checksum = chunk_json["checksum"].asString();
            metadata.is_erasure_coded = chunk_json["is_erasure_coded"].asBool();
            metadata.erasure_group_id = chunk_json["erasure_group_id"].asString();
            metadata.erasure_block_index = chunk_json["erasure_block_index"].asInt();
            metadata.is_parity_block = chunk_json["is_parity_block"].asBool();
            metadata.created_time = chunk_json["created_time"].asInt64();
            metadata.last_accessed_time = chunk_json["last_accessed_time"].asInt64();
            
            const Json::Value& servers_json = chunk_json["server_locations"];
            for (const Json::Value& server_json : servers_json) {
                std::string server_id = server_json.asString();
                metadata.server_locations.push_back(server_id);
                chunk_to_servers_[chunk_id].insert(server_id);
                server_to_chunks_[server_id].insert(chunk_id);
            }
            
            chunks_[chunk_id] = metadata;
        }
        
        // Deserialize servers
        const Json::Value& servers_json = root["servers"];
        for (const Json::Value& server_json : servers_json) {
            ServerMetadata metadata;
            std::string server_id = server_json["server_id"].asString();
            
            metadata.server_id = server_id;
            metadata.address = server_json["address"].asString();
            metadata.port = server_json["port"].asInt();
            metadata.total_space = server_json["total_space"].asInt64();
            metadata.free_space = server_json["free_space"].asInt64();
            metadata.chunk_count = server_json["chunk_count"].asInt();
            metadata.cpu_usage = server_json["cpu_usage"].asDouble();
            metadata.memory_usage = server_json["memory_usage"].asDouble();
            metadata.is_healthy = server_json["is_healthy"].asBool();
            metadata.last_heartbeat = server_json["last_heartbeat"].asInt64();
            
            // Rebuild stored_chunks set
            auto chunks_it = server_to_chunks_.find(server_id);
            if (chunks_it != server_to_chunks_.end()) {
                metadata.stored_chunks = chunks_it->second;
            }
            
            servers_[server_id] = metadata;
        }
        
    } catch (const std::exception& e) {
        Utils::logError("Exception during metadata deserialization: " + std::string(e.what()));
        return false;
    }
    
    return true;
}

} // namespace dfs