#include "chunk_storage.h"
#include <fstream>
#include <filesystem>
#include <json/json.h>
#include <sys/statvfs.h>

namespace dfs {

ChunkStorage::ChunkStorage(const std::string& storage_directory) 
    : storage_directory_(storage_directory),
      checksum_index_file_(storage_directory + "/checksums.json") {
    
    // Create storage directory if it doesn't exist
    if (!Utils::fileExists(storage_directory_)) {
        if (!Utils::createDirectory(storage_directory_)) {
            Utils::logError("Failed to create storage directory: " + storage_directory_);
        }
    }
    
    // Load existing checksum index
    loadChecksumIndex();
    
    // Scan directory for existing chunks
    updateStorageStats();
    
    Utils::logInfo("ChunkStorage initialized at: " + storage_directory_);
}

ChunkStorage::~ChunkStorage() {
    saveChecksumIndex();
    Utils::logInfo("ChunkStorage destroyed");
}

bool ChunkStorage::writeChunk(const std::string& chunk_id, 
                             const std::vector<uint8_t>& data,
                             bool is_encrypted,
                             bool is_erasure_coded) {
    std::unique_lock<std::shared_mutex> lock(storage_mutex_);
    
    std::string file_path = getChunkFilePath(chunk_id);
    
    // Calculate checksum before writing
    std::string checksum = Utils::calculateSHA256(data);
    
    // Write chunk data
    if (!Utils::writeFile(file_path, data)) {
        Utils::logError("Failed to write chunk file: " + file_path);
        return false;
    }
    
    // Save metadata
    if (!saveChunkMetadata(chunk_id, checksum, is_encrypted, is_erasure_coded)) {
        Utils::logError("Failed to save chunk metadata: " + chunk_id);
        // Clean up the data file
        Utils::deleteFile(file_path);
        return false;
    }
    
    // Update indices
    chunk_checksums_[chunk_id] = checksum;
    stored_chunks_.insert(chunk_id);
    
    Utils::logDebug("Wrote chunk: " + chunk_id + " (" + std::to_string(data.size()) + " bytes)");
    return true;
}

std::vector<uint8_t> ChunkStorage::readChunk(const std::string& chunk_id) {
    std::shared_lock<std::shared_mutex> lock(storage_mutex_);
    
    if (stored_chunks_.find(chunk_id) == stored_chunks_.end()) {
        Utils::logWarning("Chunk not found: " + chunk_id);
        return {};
    }
    
    std::string file_path = getChunkFilePath(chunk_id);
    std::vector<uint8_t> data = Utils::readFile(file_path);
    
    if (data.empty()) {
        Utils::logError("Failed to read chunk file: " + file_path);
        return {};
    }
    
    // Verify integrity
    std::string expected_checksum;
    auto checksum_it = chunk_checksums_.find(chunk_id);
    if (checksum_it != chunk_checksums_.end()) {
        expected_checksum = checksum_it->second;
    } else {
        // Try to load from metadata
        bool is_encrypted, is_erasure_coded;
        if (!loadChunkMetadata(chunk_id, expected_checksum, is_encrypted, is_erasure_coded)) {
            Utils::logWarning("No checksum available for chunk: " + chunk_id);
            return data; // Return data without verification
        }
    }
    
    std::string actual_checksum = Utils::calculateSHA256(data);
    if (actual_checksum != expected_checksum) {
        Utils::logError("Checksum mismatch for chunk " + chunk_id + 
                       " (expected: " + expected_checksum + 
                       ", actual: " + actual_checksum + ")");
        return {}; // Return empty data for corrupted chunk
    }
    
    Utils::logDebug("Read chunk: " + chunk_id + " (" + std::to_string(data.size()) + " bytes)");
    return data;
}

bool ChunkStorage::deleteChunk(const std::string& chunk_id) {
    std::unique_lock<std::shared_mutex> lock(storage_mutex_);
    
    if (stored_chunks_.find(chunk_id) == stored_chunks_.end()) {
        Utils::logWarning("Chunk not found for deletion: " + chunk_id);
        return false;
    }
    
    std::string file_path = getChunkFilePath(chunk_id);
    std::string metadata_path = getChunkMetadataPath(chunk_id);
    
    // Delete files
    bool data_deleted = Utils::deleteFile(file_path);
    bool metadata_deleted = Utils::deleteFile(metadata_path);
    
    if (!data_deleted || !metadata_deleted) {
        Utils::logError("Failed to delete chunk files for: " + chunk_id);
        return false;
    }
    
    // Update indices
    chunk_checksums_.erase(chunk_id);
    stored_chunks_.erase(chunk_id);
    
    Utils::logDebug("Deleted chunk: " + chunk_id);
    return true;
}

bool ChunkStorage::chunkExists(const std::string& chunk_id) const {
    std::shared_lock<std::shared_mutex> lock(storage_mutex_);
    return stored_chunks_.find(chunk_id) != stored_chunks_.end();
}

bool ChunkStorage::verifyChunkIntegrity(const std::string& chunk_id) {
    std::shared_lock<std::shared_mutex> lock(storage_mutex_);
    
    if (stored_chunks_.find(chunk_id) == stored_chunks_.end()) {
        return false;
    }
    
    std::string file_path = getChunkFilePath(chunk_id);
    std::vector<uint8_t> data = Utils::readFile(file_path);
    
    if (data.empty()) {
        Utils::logError("Failed to read chunk for integrity check: " + chunk_id);
        return false;
    }
    
    std::string actual_checksum = Utils::calculateSHA256(data);
    
    auto checksum_it = chunk_checksums_.find(chunk_id);
    if (checksum_it == chunk_checksums_.end()) {
        // Try to load from metadata
        std::string expected_checksum;
        bool is_encrypted, is_erasure_coded;
        if (!loadChunkMetadata(chunk_id, expected_checksum, is_encrypted, is_erasure_coded)) {
            Utils::logError("No checksum available for integrity check: " + chunk_id);
            return false;
        }
        return actual_checksum == expected_checksum;
    }
    
    return actual_checksum == checksum_it->second;
}

std::string ChunkStorage::getChunkChecksum(const std::string& chunk_id) {
    std::shared_lock<std::shared_mutex> lock(storage_mutex_);
    
    auto it = chunk_checksums_.find(chunk_id);
    if (it != chunk_checksums_.end()) {
        return it->second;
    }
    
    // Try to load from metadata
    std::string checksum;
    bool is_encrypted, is_erasure_coded;
    if (loadChunkMetadata(chunk_id, checksum, is_encrypted, is_erasure_coded)) {
        return checksum;
    }
    
    return "";
}

int64_t ChunkStorage::getTotalStorageUsed() const {
    std::shared_lock<std::shared_mutex> lock(storage_mutex_);
    
    int64_t total_size = 0;
    
    try {
        for (const std::string& chunk_id : stored_chunks_) {
            std::string file_path = getChunkFilePath(chunk_id);
            int64_t size = Utils::getFileSize(file_path);
            if (size > 0) {
                total_size += size;
            }
        }
    } catch (const std::exception& e) {
        Utils::logError("Error calculating storage usage: " + std::string(e.what()));
    }
    
    return total_size;
}

int64_t ChunkStorage::getAvailableStorage() const {
    struct statvfs stat;
    
    if (statvfs(storage_directory_.c_str(), &stat) != 0) {
        Utils::logError("Failed to get filesystem statistics");
        return 0;
    }
    
    return static_cast<int64_t>(stat.f_bavail) * stat.f_frsize;
}

int ChunkStorage::getChunkCount() const {
    std::shared_lock<std::shared_mutex> lock(storage_mutex_);
    return stored_chunks_.size();
}

std::vector<std::string> ChunkStorage::getAllChunkIds() const {
    std::shared_lock<std::shared_mutex> lock(storage_mutex_);
    return std::vector<std::string>(stored_chunks_.begin(), stored_chunks_.end());
}

void ChunkStorage::performGarbageCollection() {
    std::unique_lock<std::shared_mutex> lock(storage_mutex_);
    
    Utils::logInfo("Starting garbage collection");
    
    std::vector<std::string> chunks_to_remove;
    
    for (const std::string& chunk_id : stored_chunks_) {
        std::string file_path = getChunkFilePath(chunk_id);
        
        // Check if file exists
        if (!Utils::fileExists(file_path)) {
            chunks_to_remove.push_back(chunk_id);
            continue;
        }
        
        // Check integrity
        if (!verifyChunkIntegrity(chunk_id)) {
            Utils::logWarning("Removing corrupted chunk during GC: " + chunk_id);
            chunks_to_remove.push_back(chunk_id);
        }
    }
    
    // Remove invalid chunks
    for (const std::string& chunk_id : chunks_to_remove) {
        stored_chunks_.erase(chunk_id);
        chunk_checksums_.erase(chunk_id);
        
        // Try to delete files (may already be missing)
        Utils::deleteFile(getChunkFilePath(chunk_id));
        Utils::deleteFile(getChunkMetadataPath(chunk_id));
    }
    
    // Save updated index
    saveChecksumIndex();
    
    Utils::logInfo("Garbage collection completed. Removed " + 
                   std::to_string(chunks_to_remove.size()) + " chunks");
}

void ChunkStorage::rebuildChecksumIndex() {
    std::unique_lock<std::shared_mutex> lock(storage_mutex_);
    
    Utils::logInfo("Rebuilding checksum index");
    
    chunk_checksums_.clear();
    stored_chunks_.clear();
    
    try {
        // Scan storage directory
        std::filesystem::path storage_path(storage_directory_);
        
        for (const auto& entry : std::filesystem::directory_iterator(storage_path)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                
                // Check if it's a chunk file (not metadata)
                if (filename.find(".meta") == std::string::npos && 
                    filename != "checksums.json") {
                    
                    std::string chunk_id = filename;
                    std::vector<uint8_t> data = Utils::readFile(entry.path().string());
                    
                    if (!data.empty()) {
                        std::string checksum = Utils::calculateSHA256(data);
                        chunk_checksums_[chunk_id] = checksum;
                        stored_chunks_.insert(chunk_id);
                        
                        // Update metadata file
                        saveChunkMetadata(chunk_id, checksum, false, false);
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        Utils::logError("Error rebuilding checksum index: " + std::string(e.what()));
    }
    
    saveChecksumIndex();
    
    Utils::logInfo("Checksum index rebuilt. Found " + 
                   std::to_string(stored_chunks_.size()) + " chunks");
}

std::string ChunkStorage::getChunkFilePath(const std::string& chunk_id) const {
    return storage_directory_ + "/" + chunk_id;
}

std::string ChunkStorage::getChunkMetadataPath(const std::string& chunk_id) const {
    return storage_directory_ + "/" + chunk_id + ".meta";
}

bool ChunkStorage::saveChecksumIndex() {
    try {
        Json::Value root;
        
        for (const auto& pair : chunk_checksums_) {
            root[pair.first] = pair.second;
        }
        
        Json::StreamWriterBuilder builder;
        std::string json_string = Json::writeString(builder, root);
        
        std::ofstream file(checksum_index_file_);
        if (!file.is_open()) {
            Utils::logError("Failed to open checksum index file for writing");
            return false;
        }
        
        file << json_string;
        return file.good();
        
    } catch (const std::exception& e) {
        Utils::logError("Error saving checksum index: " + std::string(e.what()));
        return false;
    }
}

bool ChunkStorage::loadChecksumIndex() {
    if (!Utils::fileExists(checksum_index_file_)) {
        Utils::logInfo("Checksum index file not found, starting fresh");
        return true;
    }
    
    try {
        std::ifstream file(checksum_index_file_);
        if (!file.is_open()) {
            Utils::logError("Failed to open checksum index file for reading");
            return false;
        }
        
        Json::Value root;
        Json::CharReaderBuilder builder;
        std::string errors;
        
        if (!Json::parseFromStream(builder, file, &root, &errors)) {
            Utils::logError("Failed to parse checksum index JSON: " + errors);
            return false;
        }
        
        chunk_checksums_.clear();
        stored_chunks_.clear();
        
        for (const auto& member : root.getMemberNames()) {
            chunk_checksums_[member] = root[member].asString();
            stored_chunks_.insert(member);
        }
        
        Utils::logInfo("Loaded checksum index with " + 
                       std::to_string(chunk_checksums_.size()) + " entries");
        return true;
        
    } catch (const std::exception& e) {
        Utils::logError("Error loading checksum index: " + std::string(e.what()));
        return false;
    }
}

bool ChunkStorage::saveChunkMetadata(const std::string& chunk_id, 
                                    const std::string& checksum,
                                    bool is_encrypted,
                                    bool is_erasure_coded) {
    try {
        Json::Value metadata;
        metadata["chunk_id"] = chunk_id;
        metadata["checksum"] = checksum;
        metadata["is_encrypted"] = is_encrypted;
        metadata["is_erasure_coded"] = is_erasure_coded;
        metadata["created_time"] = static_cast<Json::Int64>(Utils::getCurrentTimestamp());
        
        Json::StreamWriterBuilder builder;
        std::string json_string = Json::writeString(builder, metadata);
        
        std::string metadata_path = getChunkMetadataPath(chunk_id);
        std::ofstream file(metadata_path);
        
        if (!file.is_open()) {
            Utils::logError("Failed to open metadata file for writing: " + metadata_path);
            return false;
        }
        
        file << json_string;
        return file.good();
        
    } catch (const std::exception& e) {
        Utils::logError("Error saving chunk metadata: " + std::string(e.what()));
        return false;
    }
}

bool ChunkStorage::loadChunkMetadata(const std::string& chunk_id,
                                    std::string& checksum,
                                    bool& is_encrypted,
                                    bool& is_erasure_coded) {
    std::string metadata_path = getChunkMetadataPath(chunk_id);
    
    if (!Utils::fileExists(metadata_path)) {
        return false;
    }
    
    try {
        std::ifstream file(metadata_path);
        if (!file.is_open()) {
            return false;
        }
        
        Json::Value metadata;
        Json::CharReaderBuilder builder;
        std::string errors;
        
        if (!Json::parseFromStream(builder, file, &metadata, &errors)) {
            Utils::logError("Failed to parse chunk metadata JSON: " + errors);
            return false;
        }
        
        checksum = metadata["checksum"].asString();
        is_encrypted = metadata["is_encrypted"].asBool();
        is_erasure_coded = metadata["is_erasure_coded"].asBool();
        
        return true;
        
    } catch (const std::exception& e) {
        Utils::logError("Error loading chunk metadata: " + std::string(e.what()));
        return false;
    }
}

void ChunkStorage::updateStorageStats() {
    stored_chunks_.clear();
    
    try {
        std::filesystem::path storage_path(storage_directory_);
        
        if (!std::filesystem::exists(storage_path)) {
            return;
        }
        
        for (const auto& entry : std::filesystem::directory_iterator(storage_path)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                
                // Check if it's a chunk file (not metadata or index)
                if (filename.find(".meta") == std::string::npos && 
                    filename != "checksums.json") {
                    stored_chunks_.insert(filename);
                }
            }
        }
        
    } catch (const std::exception& e) {
        Utils::logError("Error updating storage stats: " + std::string(e.what()));
    }
}

} // namespace dfs