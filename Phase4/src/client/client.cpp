#include "client.h"
#include "erasure_coding.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <iomanip>

namespace dfs {

// CacheManager implementation
CacheManager::CacheManager(size_t max_cache_size_mb) 
    : max_size_(max_cache_size_mb * 1024 * 1024),
      total_size_(0),
      cache_hits_(0),
      cache_misses_(0) {
    Utils::logInfo("CacheManager initialized with " + std::to_string(max_cache_size_mb) + "MB capacity");
}

CacheManager::~CacheManager() {
    clear();
}

bool CacheManager::put(const std::string& chunk_id, const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    // Check if already exists
    auto it = cache_.find(chunk_id);
    if (it != cache_.end()) {
        // Update existing entry
        total_size_ -= it->second.size;
        it->second.data = data;
        it->second.size = data.size();
        it->second.last_accessed = Utils::getCurrentTimestamp();
        total_size_ += data.size();
        return true;
    }
    
    // Evict if necessary
    while (total_size_ + data.size() > max_size_ && !cache_.empty()) {
        evictLRU();
    }
    
    // Add new entry
    CacheEntry entry;
    entry.data = data;
    entry.size = data.size();
    entry.last_accessed = Utils::getCurrentTimestamp();
    
    cache_[chunk_id] = entry;
    total_size_ += data.size();
    
    return true;
}

std::vector<uint8_t> CacheManager::get(const std::string& chunk_id) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    auto it = cache_.find(chunk_id);
    if (it != cache_.end()) {
        cache_hits_++;
        it->second.last_accessed = Utils::getCurrentTimestamp();
        return it->second.data;
    }
    
    cache_misses_++;
    return {};
}

bool CacheManager::contains(const std::string& chunk_id) const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    return cache_.find(chunk_id) != cache_.end();
}

void CacheManager::remove(const std::string& chunk_id) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    auto it = cache_.find(chunk_id);
    if (it != cache_.end()) {
        total_size_ -= it->second.size;
        cache_.erase(it);
    }
}

void CacheManager::clear() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    cache_.clear();
    total_size_ = 0;
}

double CacheManager::getHitRate() const {
    int64_t total_accesses = cache_hits_ + cache_misses_;
    return (total_accesses > 0) ? (static_cast<double>(cache_hits_) / total_accesses) : 0.0;
}

void CacheManager::evictLRU() {
    if (cache_.empty()) return;
    
    auto oldest = cache_.begin();
    for (auto it = cache_.begin(); it != cache_.end(); ++it) {
        if (it->second.last_accessed < oldest->second.last_accessed) {
            oldest = it;
        }
    }
    
    total_size_ -= oldest->second.size;
    cache_.erase(oldest);
}

// Uploader implementation
Uploader::Uploader(std::shared_ptr<FileService::Stub> file_service,
                   std::shared_ptr<CacheManager> cache_manager)
    : file_service_(file_service), cache_manager_(cache_manager) {
}

bool Uploader::uploadFile(const std::string& local_path, 
                         const std::string& remote_path,
                         bool enable_encryption,
                         bool enable_erasure_coding) {
    
    Utils::logInfo("Starting upload: " + local_path + " -> " + remote_path);
    
    // Read file
    std::vector<uint8_t> file_data = Utils::readFile(local_path);
    if (file_data.empty()) {
        Utils::logError("Failed to read file: " + local_path);
        return false;
    }
    
    int64_t file_size = file_data.size();
    Utils::logInfo("File size: " + std::to_string(file_size) + " bytes");
    
    // Create file on master
    CreateFileRequest create_request;
    create_request.set_filename(remote_path);
    create_request.set_file_size(file_size);
    create_request.set_enable_encryption(enable_encryption);
    create_request.set_enable_erasure_coding(enable_erasure_coding);
    
    CreateFileResponse create_response;
    grpc::ClientContext create_context;
    
    grpc::Status status = file_service_->CreateFile(&create_context, create_request, &create_response);
    
    if (!status.ok() || !create_response.success()) {
        Utils::logError("Failed to create file: " + 
                       (status.ok() ? create_response.message() : status.error_message()));
        return false;
    }
    
    std::string file_id = create_response.file_id();
    
    // Allocate chunks
    AllocateChunksRequest alloc_request;
    alloc_request.set_file_id(file_id);
    alloc_request.set_chunk_count((file_size + CHUNK_SIZE - 1) / CHUNK_SIZE);
    alloc_request.set_enable_erasure_coding(enable_erasure_coding);
    
    AllocateChunksResponse alloc_response;
    grpc::ClientContext alloc_context;
    
    status = file_service_->AllocateChunks(&alloc_context, alloc_request, &alloc_response);
    
    if (!status.ok() || !alloc_response.success()) {
        Utils::logError("Failed to allocate chunks: " + 
                       (status.ok() ? alloc_response.message() : status.error_message()));
        return false;
    }
    
    // Split file into chunks
    std::vector<std::vector<uint8_t>> chunks = splitFileIntoChunks(file_data);
    
    if (chunks.size() != static_cast<size_t>(alloc_response.allocated_chunks_size())) {
        Utils::logError("Chunk count mismatch");
        return false;
    }
    
    // Upload chunks
    int64_t uploaded_bytes = 0;
    std::vector<std::string> uploaded_chunk_ids;
    
    for (int i = 0; i < alloc_response.allocated_chunks_size(); ++i) {
        const ChunkInfo& chunk_info = alloc_response.allocated_chunks(i);
        
        if (i >= static_cast<int>(chunks.size())) {
            Utils::logError("Chunk index out of range");
            return false;
        }
        
        std::vector<uint8_t> chunk_data = chunks[i];
        
        // Encrypt chunk if needed
        if (enable_encryption) {
            KeyManager& key_manager = KeyManager::getInstance();
            std::string key_id = file_id + "_key";
            
            if (!key_manager.hasKey(key_id)) {
                Utils::logError("Encryption key not found for file: " + file_id);
                return false;
            }
            
            chunk_data = Crypto::encryptChunk(chunk_data, key_id);
            if (chunk_data.empty()) {
                Utils::logError("Failed to encrypt chunk");
                return false;
            }
        }
        
        // Upload chunk to all servers
        std::vector<std::string> server_addresses;
        for (int j = 0; j < chunk_info.server_addresses_size(); ++j) {
            server_addresses.push_back(chunk_info.server_addresses(j));
        }
        
        if (uploadChunk(chunk_info.chunk_id(), chunk_data, server_addresses, enable_encryption)) {
            uploaded_chunk_ids.push_back(chunk_info.chunk_id());
            uploaded_bytes += chunks[i].size();
            
            if (progress_callback_) {
                progress_callback_(uploaded_bytes, file_size);
            }
        } else {
            Utils::logError("Failed to upload chunk: " + chunk_info.chunk_id());
            return false;
        }
    }
    
    // Complete upload
    CompleteUploadRequest complete_request;
    complete_request.set_file_id(file_id);
    for (const std::string& chunk_id : uploaded_chunk_ids) {
        complete_request.add_uploaded_chunk_ids(chunk_id);
    }
    
    CompleteUploadResponse complete_response;
    grpc::ClientContext complete_context;
    
    status = file_service_->CompleteUpload(&complete_context, complete_request, &complete_response);
    
    if (!status.ok() || !complete_response.success()) {
        Utils::logError("Failed to complete upload: " + 
                       (status.ok() ? complete_response.message() : status.error_message()));
        return false;
    }
    
    Utils::logInfo("Upload completed successfully");
    return true;
}

bool Uploader::uploadChunk(const std::string& chunk_id,
                          const std::vector<uint8_t>& data,
                          const std::vector<std::string>& server_addresses,
                          bool is_encrypted) {
    
    if (server_addresses.empty()) {
        Utils::logError("No server addresses provided for chunk upload");
        return false;
    }
    
    bool success = false;
    
    // Try to upload to all servers
    for (const std::string& server_address : server_addresses) {
        try {
            auto channel = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
            auto stub = ChunkStorage::NewStub(channel);
            
            WriteChunkRequest request;
            request.set_chunk_id(chunk_id);
            request.set_data(std::string(data.begin(), data.end()));
            request.set_checksum(Utils::calculateSHA256(data));
            request.set_is_encrypted(is_encrypted);
            request.set_is_erasure_coded(false);
            
            WriteChunkResponse response;
            grpc::ClientContext context;
            
            grpc::Status status = stub->WriteChunk(&context, request, &response);
            
            if (status.ok() && response.success()) {
                success = true;
                Utils::logDebug("Successfully uploaded chunk " + chunk_id + " to " + server_address);
            } else {
                Utils::logWarning("Failed to upload chunk " + chunk_id + " to " + server_address + 
                                 ": " + (status.ok() ? response.message() : status.error_message()));
            }
            
        } catch (const std::exception& e) {
            Utils::logError("Exception uploading chunk to " + server_address + ": " + e.what());
        }
    }
    
    // Cache the chunk if upload was successful
    if (success && cache_manager_) {
        cache_manager_->put(chunk_id, data);
    }
    
    return success;
}

std::vector<std::vector<uint8_t>> Uploader::splitFileIntoChunks(const std::vector<uint8_t>& file_data) {
    std::vector<std::vector<uint8_t>> chunks;
    
    size_t chunk_size = CHUNK_SIZE;
    for (size_t i = 0; i < file_data.size(); i += chunk_size) {
        size_t actual_chunk_size = std::min(chunk_size, file_data.size() - i);
        
        std::vector<uint8_t> chunk(file_data.begin() + i, 
                                  file_data.begin() + i + actual_chunk_size);
        chunks.push_back(chunk);
    }
    
    return chunks;
}

// Downloader implementation
Downloader::Downloader(std::shared_ptr<FileService::Stub> file_service,
                       std::shared_ptr<CacheManager> cache_manager)
    : file_service_(file_service), cache_manager_(cache_manager) {
}

bool Downloader::downloadFile(const std::string& remote_path, 
                             const std::string& local_path) {
    
    Utils::logInfo("Starting download: " + remote_path + " -> " + local_path);
    
    // Get file info
    GetFileInfoRequest info_request;
    info_request.set_filename(remote_path);
    
    GetFileInfoResponse info_response;
    grpc::ClientContext info_context;
    
    grpc::Status status = file_service_->GetFileInfo(&info_context, info_request, &info_response);
    
    if (!status.ok() || !info_response.found()) {
        Utils::logError("File not found: " + remote_path);
        return false;
    }
    
    const FileInfo& file_info = info_response.file_info();
    int64_t file_size = file_info.size();
    
    Utils::logInfo("File size: " + std::to_string(file_size) + " bytes");
    
    // Download chunks
    std::vector<std::vector<uint8_t>> chunks;
    int64_t downloaded_bytes = 0;
    
    for (const ChunkInfo& chunk_info : file_info.chunks()) {
        std::vector<std::string> server_addresses;
        for (const std::string& address : chunk_info.server_addresses()) {
            server_addresses.push_back(address);
        }
        
        std::vector<uint8_t> chunk_data = downloadChunk(chunk_info.chunk_id(), server_addresses);
        
        if (chunk_data.empty()) {
            Utils::logError("Failed to download chunk: " + chunk_info.chunk_id());
            return false;
        }
        
        // Decrypt chunk if needed
        if (file_info.is_encrypted()) {
            KeyManager& key_manager = KeyManager::getInstance();
            if (key_manager.hasKey(file_info.encryption_key_id())) {
                chunk_data = Crypto::decryptChunk(chunk_data, file_info.encryption_key_id());
                if (chunk_data.empty()) {
                    Utils::logError("Failed to decrypt chunk");
                    return false;
                }
            } else {
                Utils::logError("Decryption key not found");
                return false;
            }
        }
        
        chunks.push_back(chunk_data);
        downloaded_bytes += chunk_data.size();
        
        if (progress_callback_) {
            progress_callback_(downloaded_bytes, file_size);
        }
    }
    
    // Assemble file
    std::vector<uint8_t> file_data = assembleChunks(chunks);
    
    // Write to local file
    if (!Utils::writeFile(local_path, file_data)) {
        Utils::logError("Failed to write file: " + local_path);
        return false;
    }
    
    Utils::logInfo("Download completed successfully");
    return true;
}

std::vector<uint8_t> Downloader::downloadChunk(const std::string& chunk_id,
                                               const std::vector<std::string>& server_addresses) {
    
    // Check cache first
    if (cache_manager_ && cache_manager_->contains(chunk_id)) {
        return cache_manager_->get(chunk_id);
    }
    
    // Try to download from any server
    for (const std::string& server_address : server_addresses) {
        try {
            auto channel = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
            auto stub = ChunkStorage::NewStub(channel);
            
            ReadChunkRequest request;
            request.set_chunk_id(chunk_id);
            request.set_verify_integrity(true);
            
            ReadChunkResponse response;
            grpc::ClientContext context;
            
            grpc::Status status = stub->ReadChunk(&context, request, &response);
            
            if (status.ok() && response.success()) {
                std::vector<uint8_t> data(response.data().begin(), response.data().end());
                
                // Verify checksum
                std::string actual_checksum = Utils::calculateSHA256(data);
                if (actual_checksum == response.checksum()) {
                    // Cache the chunk
                    if (cache_manager_) {
                        cache_manager_->put(chunk_id, data);
                    }
                    
                    Utils::logDebug("Successfully downloaded chunk " + chunk_id + " from " + server_address);
                    return data;
                } else {
                    Utils::logWarning("Checksum mismatch for chunk " + chunk_id + " from " + server_address);
                }
            } else {
                Utils::logWarning("Failed to download chunk " + chunk_id + " from " + server_address + 
                                 ": " + (status.ok() ? response.message() : status.error_message()));
            }
            
        } catch (const std::exception& e) {
            Utils::logError("Exception downloading chunk from " + server_address + ": " + e.what());
        }
    }
    
    return {}; // Failed to download from any server
}

std::vector<uint8_t> Downloader::assembleChunks(const std::vector<std::vector<uint8_t>>& chunks) {
    std::vector<uint8_t> assembled_data;
    
    for (const auto& chunk : chunks) {
        assembled_data.insert(assembled_data.end(), chunk.begin(), chunk.end());
    }
    
    return assembled_data;
}

// DFSClient implementation
DFSClient::DFSClient(const std::string& master_address, int master_port) 
    : verbose_logging_(false) {
    
    std::string address = master_address + ":" + std::to_string(master_port);
    channel_ = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
    file_service_ = FileService::NewStub(channel_);
    
    cache_manager_ = std::make_shared<CacheManager>(100); // 100MB cache
    uploader_ = std::make_unique<Uploader>(file_service_, cache_manager_);
    downloader_ = std::make_unique<Downloader>(file_service_, cache_manager_);
    
    // Set progress callbacks
    uploader_->setProgressCallback([this](int64_t current, int64_t total) {
        if (verbose_logging_) {
            printProgressBar(current, total, "Uploading");
        }
    });
    
    downloader_->setProgressCallback([this](int64_t current, int64_t total) {
        if (verbose_logging_) {
            printProgressBar(current, total, "Downloading");
        }
    });
    
    Utils::logInfo("DFSClient connected to master at " + address);
}

DFSClient::~DFSClient() {
    Utils::logInfo("DFSClient disconnected");
}

bool DFSClient::put(const std::string& local_file, const std::string& remote_file,
                   bool enable_encryption, bool enable_erasure_coding) {
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    bool success = uploader_->uploadFile(local_file, remote_file, enable_encryption, enable_erasure_coding);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    if (success) {
        int64_t file_size = Utils::getFileSize(local_file);
        std::cout << "Upload completed successfully\n";
        std::cout << "File size: " << formatFileSize(file_size) << "\n";
        std::cout << "Duration: " << formatDuration(duration.count()) << "\n";
        
        if (file_size > 0 && duration.count() > 0) {
            double speed = (file_size / 1024.0 / 1024.0) / (duration.count() / 1000.0);
            std::cout << "Speed: " << std::fixed << std::setprecision(2) << speed << " MB/s\n";
        }
    }
    
    return success;
}

bool DFSClient::get(const std::string& remote_file, const std::string& local_file) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    bool success = downloader_->downloadFile(remote_file, local_file);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    if (success) {
        int64_t file_size = Utils::getFileSize(local_file);
        std::cout << "Download completed successfully\n";
        std::cout << "File size: " << formatFileSize(file_size) << "\n";
        std::cout << "Duration: " << formatDuration(duration.count()) << "\n";
        
        if (file_size > 0 && duration.count() > 0) {
            double speed = (file_size / 1024.0 / 1024.0) / (duration.count() / 1000.0);
            std::cout << "Speed: " << std::fixed << std::setprecision(2) << speed << " MB/s\n";
        }
    }
    
    return success;
}

bool DFSClient::deleteFile(const std::string& remote_file) {
    DeleteFileRequest request;
    request.set_filename(remote_file);
    
    DeleteFileResponse response;
    grpc::ClientContext context;
    
    grpc::Status status = file_service_->DeleteFile(&context, request, &response);
    
    if (status.ok() && response.success()) {
        std::cout << "File deleted successfully: " << remote_file << std::endl;
        return true;
    } else {
        std::cout << "Failed to delete file: " << 
                    (status.ok() ? response.message() : status.error_message()) << std::endl;
        return false;
    }
}

bool DFSClient::listFiles(const std::string& path_prefix) {
    ListFilesRequest request;
    request.set_path_prefix(path_prefix);
    
    ListFilesResponse response;
    grpc::ClientContext context;
    
    grpc::Status status = file_service_->ListFiles(&context, request, &response);
    
    if (!status.ok()) {
        std::cout << "Failed to list files: " << status.error_message() << std::endl;
        return false;
    }
    
    if (response.files_size() == 0) {
        std::cout << "No files found" << std::endl;
        return true;
    }
    
    std::cout << std::left << std::setw(30) << "Filename" 
              << std::setw(15) << "Size" 
              << std::setw(20) << "Created" 
              << std::setw(10) << "Encrypted"
              << std::setw(10) << "EC" << std::endl;
    std::cout << std::string(85, '-') << std::endl;
    
    for (const FileInfo& file : response.files()) {
        std::cout << std::left << std::setw(30) << file.filename()
                  << std::setw(15) << formatFileSize(file.size())
                  << std::setw(20) << Utils::timestampToString(file.created_time())
                  << std::setw(10) << (file.is_encrypted() ? "Yes" : "No")
                  << std::setw(10) << (file.chunks_size() > 0 && file.chunks(0).is_erasure_coded() ? "Yes" : "No")
                  << std::endl;
    }
    
    return true;
}

bool DFSClient::getFileInfo(const std::string& remote_file) {
    GetFileInfoRequest request;
    request.set_filename(remote_file);
    
    GetFileInfoResponse response;
    grpc::ClientContext context;
    
    grpc::Status status = file_service_->GetFileInfo(&context, request, &response);
    
    if (!status.ok()) {
        std::cout << "Failed to get file info: " << status.error_message() << std::endl;
        return false;
    }
    
    if (!response.found()) {
        std::cout << "File not found: " << remote_file << std::endl;
        return false;
    }
    
    const FileInfo& file = response.file_info();
    
    std::cout << "File Information:" << std::endl;
    std::cout << "  Name: " << file.filename() << std::endl;
    std::cout << "  Size: " << formatFileSize(file.size()) << std::endl;
    std::cout << "  Created: " << Utils::timestampToString(file.created_time()) << std::endl;
    std::cout << "  Modified: " << Utils::timestampToString(file.modified_time()) << std::endl;
    std::cout << "  Encrypted: " << (file.is_encrypted() ? "Yes" : "No") << std::endl;
    std::cout << "  Erasure Coded: " << (file.chunks_size() > 0 && file.chunks(0).is_erasure_coded() ? "Yes" : "No") << std::endl;
    std::cout << "  Chunks: " << file.chunks_size() << std::endl;
    
    if (verbose_logging_) {
        std::cout << "\nChunk Details:" << std::endl;
        for (int i = 0; i < file.chunks_size(); ++i) {
            const ChunkInfo& chunk = file.chunks(i);
            std::cout << "  Chunk " << i << ": " << chunk.chunk_id() << std::endl;
            std::cout << "    Size: " << formatFileSize(chunk.size()) << std::endl;
            std::cout << "    Servers: ";
            for (const std::string& server : chunk.server_addresses()) {
                std::cout << server << " ";
            }
            std::cout << std::endl;
        }
    }
    
    return true;
}

void DFSClient::setCacheSize(size_t size_mb) {
    cache_manager_ = std::make_shared<CacheManager>(size_mb);
    uploader_ = std::make_unique<Uploader>(file_service_, cache_manager_);
    downloader_ = std::make_unique<Downloader>(file_service_, cache_manager_);
}

void DFSClient::printStatistics() {
    std::cout << "\nClient Statistics:" << std::endl;
    std::cout << "  Cache Size: " << cache_manager_->size() << " chunks" << std::endl;
    std::cout << "  Cache Usage: " << formatFileSize(cache_manager_->getTotalSize()) << std::endl;
    std::cout << "  Cache Hit Rate: " << std::fixed << std::setprecision(2) 
              << (cache_manager_->getHitRate() * 100) << "%" << std::endl;
}

void DFSClient::printProgressBar(int64_t current, int64_t total, const std::string& operation) {
    const int bar_width = 50;
    double progress = static_cast<double>(current) / total;
    int filled = static_cast<int>(progress * bar_width);
    
    std::cout << "\r" << operation << ": [";
    for (int i = 0; i < bar_width; ++i) {
        if (i < filled) std::cout << "=";
        else if (i == filled) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] " << std::fixed << std::setprecision(1) << (progress * 100) << "% ";
    std::cout << "(" << formatFileSize(current) << "/" << formatFileSize(total) << ")";
    std::cout.flush();
    
    if (current >= total) {
        std::cout << std::endl;
    }
}

std::string DFSClient::formatFileSize(int64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        unit++;
    }
    
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << size << " " << units[unit];
    return ss.str();
}

std::string DFSClient::formatDuration(int64_t milliseconds) {
    int64_t seconds = milliseconds / 1000;
    int64_t minutes = seconds / 60;
    int64_t hours = minutes / 60;
    
    std::stringstream ss;
    if (hours > 0) {
        ss << hours << "h ";
    }
    if (minutes > 0) {
        ss << (minutes % 60) << "m ";
    }
    ss << (seconds % 60) << "s";
    
    return ss.str();
}

} // namespace dfs