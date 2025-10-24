#include "master_server.h"
#include "crypto.h"
#include <iostream>
#include <signal.h>

namespace dfs {

// Global pointer for signal handling
static MasterServer* g_master_server = nullptr;

void signalHandler(int signal) {
    if (g_master_server) {
        Utils::logInfo("Received signal " + std::to_string(signal) + ", shutting down...");
        g_master_server->stop();
    }
}

MasterServer::MasterServer() 
    : running_(false),
      total_requests_(0),
      successful_requests_(0),
      failed_requests_(0) {
    
    metadata_manager_ = std::make_shared<MetadataManager>();
    chunk_allocator_ = std::make_unique<ChunkAllocator>(metadata_manager_);
    
    // Try to load existing metadata
    metadata_manager_->loadMetadataFromFile("master_metadata.json");
    
    Utils::logInfo("MasterServer initialized");
}

MasterServer::~MasterServer() {
    stop();
    Utils::logInfo("MasterServer destroyed");
}

void MasterServer::start(const std::string& address, int port) {
    if (running_.load()) {
        Utils::logWarning("Server is already running");
        return;
    }
    
    std::string server_address = address + ":" + std::to_string(port);
    
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(static_cast<FileService::Service*>(this));
    builder.RegisterService(static_cast<ChunkManagement::Service*>(this));
    
    // Set max message size (for large chunks)
    builder.SetMaxReceiveMessageSize(64 * 1024 * 1024); // 64MB
    builder.SetMaxSendMessageSize(64 * 1024 * 1024);    // 64MB
    
    server_ = builder.BuildAndStart();
    
    if (!server_) {
        Utils::logError("Failed to start gRPC server");
        return;
    }
    
    running_.store(true);
    
    // Set up signal handling
    g_master_server = this;
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // Start background threads
    heartbeat_monitor_thread_ = std::thread(&MasterServer::monitorHeartbeats, this);
    rebalancing_thread_ = std::thread(&MasterServer::performRebalancing, this);
    metadata_persistence_thread_ = std::thread(&MasterServer::persistMetadata, this);
    
    Utils::logInfo("MasterServer started on " + server_address);
    
    // Wait for shutdown
    server_->Wait();
}

void MasterServer::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    if (server_) {
        server_->Shutdown();
    }
    
    // Wait for background threads to finish
    if (heartbeat_monitor_thread_.joinable()) {
        heartbeat_monitor_thread_.join();
    }
    if (rebalancing_thread_.joinable()) {
        rebalancing_thread_.join();
    }
    if (metadata_persistence_thread_.joinable()) {
        metadata_persistence_thread_.join();
    }
    
    // Save metadata before shutdown
    metadata_manager_->saveMetadataToFile("master_metadata.json");
    
    Utils::logInfo("MasterServer stopped");
}

grpc::Status MasterServer::CreateFile(grpc::ServerContext* context,
                                     const CreateFileRequest* request,
                                     CreateFileResponse* response) {
    total_requests_++;
    
    Utils::logInfo("CreateFile request for: " + request->filename());
    
    if (!validateFileName(request->filename())) {
        response->set_success(false);
        response->set_message("Invalid filename");
        failed_requests_++;
        return grpc::Status::OK;
    }
    
    // Check if file already exists
    FileMetadata existing_metadata;
    if (metadata_manager_->getFileMetadata(request->filename(), existing_metadata)) {
        response->set_success(false);
        response->set_message("File already exists");
        failed_requests_++;
        return grpc::Status::OK;
    }
    
    // Create file metadata
    FileMetadata metadata;
    metadata.file_id = Utils::generateFileId();
    metadata.filename = request->filename();
    metadata.size = request->file_size();
    metadata.created_time = Utils::getCurrentTimestamp();
    metadata.modified_time = metadata.created_time;
    metadata.is_encrypted = request->enable_encryption();
    metadata.is_erasure_coded = request->enable_erasure_coding();
    
    // Generate encryption key if needed
    if (metadata.is_encrypted) {
        KeyManager& key_manager = KeyManager::getInstance();
        metadata.encryption_key_id = metadata.file_id + "_key";
        std::string encryption_key = key_manager.generateKey();
        key_manager.storeKey(metadata.encryption_key_id, encryption_key);
    }
    
    // Create the file in metadata
    if (!metadata_manager_->createFile(request->filename(), metadata)) {
        response->set_success(false);
        response->set_message("Failed to create file metadata");
        failed_requests_++;
        return grpc::Status::OK;
    }
    
    response->set_success(true);
    response->set_file_id(metadata.file_id);
    response->set_message("File created successfully");
    
    successful_requests_++;
    return grpc::Status::OK;
}

grpc::Status MasterServer::DeleteFile(grpc::ServerContext* context,
                                     const DeleteFileRequest* request,
                                     DeleteFileResponse* response) {
    total_requests_++;
    
    Utils::logInfo("DeleteFile request for: " + request->filename());
    
    if (!metadata_manager_->deleteFile(request->filename())) {
        response->set_success(false);
        response->set_message("File not found");
        failed_requests_++;
        return grpc::Status::OK;
    }
    
    response->set_success(true);
    response->set_message("File deleted successfully");
    
    successful_requests_++;
    return grpc::Status::OK;
}

grpc::Status MasterServer::ListFiles(grpc::ServerContext* context,
                                    const ListFilesRequest* request,
                                    ListFilesResponse* response) {
    total_requests_++;
    
    auto files = metadata_manager_->listFiles(request->path_prefix());
    
    for (const auto& file_metadata : files) {
        FileInfo* file_info = response->add_files();
        convertFileMetadataToProto(file_metadata, file_info);
    }
    
    successful_requests_++;
    return grpc::Status::OK;
}

grpc::Status MasterServer::GetFileInfo(grpc::ServerContext* context,
                                      const GetFileInfoRequest* request,
                                      GetFileInfoResponse* response) {
    total_requests_++;
    
    FileMetadata metadata;
    if (!metadata_manager_->getFileMetadata(request->filename(), metadata)) {
        response->set_found(false);
        failed_requests_++;
        return grpc::Status::OK;
    }
    
    response->set_found(true);
    convertFileMetadataToProto(metadata, response->mutable_file_info());
    
    successful_requests_++;
    return grpc::Status::OK;
}

grpc::Status MasterServer::AllocateChunks(grpc::ServerContext* context,
                                         const AllocateChunksRequest* request,
                                         AllocateChunksResponse* response) {
    total_requests_++;
    
    Utils::logInfo("AllocateChunks request for file: " + request->file_id());
    
    // Get file metadata to determine size
    FileMetadata file_metadata;
    bool found = false;
    
    // Find file by ID
    auto all_files = metadata_manager_->listFiles();
    for (const auto& file : all_files) {
        if (file.file_id == request->file_id()) {
            file_metadata = file;
            found = true;
            break;
        }
    }
    
    if (!found) {
        response->set_success(false);
        response->set_message("File not found");
        failed_requests_++;
        return grpc::Status::OK;
    }
    
    // Allocate chunks
    auto allocated_chunks = chunk_allocator_->allocateChunks(
        request->file_id(),
        file_metadata.size,
        request->enable_erasure_coding()
    );
    
    if (allocated_chunks.empty()) {
        response->set_success(false);
        response->set_message("Failed to allocate chunks - no available servers");
        failed_requests_++;
        return grpc::Status::OK;
    }
    
    // Update file metadata with chunk IDs
    for (const auto& chunk_info : allocated_chunks) {
        file_metadata.chunk_ids.push_back(chunk_info.chunk_id);
    }
    metadata_manager_->updateFileMetadata(file_metadata.filename, file_metadata);
    
    // Prepare response
    response->set_success(true);
    response->set_message("Chunks allocated successfully");
    
    for (const auto& chunk_info : allocated_chunks) {
        ChunkInfo* proto_chunk = response->add_allocated_chunks();
        *proto_chunk = chunk_info; // Direct assignment should work for proto messages
    }
    
    successful_requests_++;
    return grpc::Status::OK;
}

grpc::Status MasterServer::GetChunkLocations(grpc::ServerContext* context,
                                           const GetChunkLocationsRequest* request,
                                           GetChunkLocationsResponse* response) {
    total_requests_++;
    
    for (const std::string& chunk_id : request->chunk_ids()) {
        ChunkMetadata metadata;
        if (metadata_manager_->getChunkMetadata(chunk_id, metadata)) {
            ChunkInfo* chunk_info = response->add_chunk_locations();
            convertChunkMetadataToProto(metadata, chunk_info);
        }
    }
    
    successful_requests_++;
    return grpc::Status::OK;
}

grpc::Status MasterServer::CompleteUpload(grpc::ServerContext* context,
                                         const CompleteUploadRequest* request,
                                         CompleteUploadResponse* response) {
    total_requests_++;
    
    Utils::logInfo("CompleteUpload for file: " + request->file_id());
    
    // Update file's modified time
    auto all_files = metadata_manager_->listFiles();
    for (const auto& file : all_files) {
        if (file.file_id == request->file_id()) {
            FileMetadata updated_metadata = file;
            updated_metadata.modified_time = Utils::getCurrentTimestamp();
            metadata_manager_->updateFileMetadata(file.filename, updated_metadata);
            break;
        }
    }
    
    response->set_success(true);
    response->set_message("Upload completed successfully");
    
    successful_requests_++;
    return grpc::Status::OK;
}

grpc::Status MasterServer::RegisterChunkServer(grpc::ServerContext* context,
                                              const RegisterChunkServerRequest* request,
                                              RegisterChunkServerResponse* response) {
    total_requests_++;
    
    Utils::logInfo("RegisterChunkServer: " + request->server_id() + 
                   " at " + request->address() + ":" + std::to_string(request->port()));
    
    ServerMetadata metadata;
    metadata.server_id = request->server_id();
    metadata.address = request->address();
    metadata.port = request->port();
    metadata.total_space = request->total_space();
    metadata.free_space = request->total_space(); // Initially all space is free
    metadata.chunk_count = 0;
    metadata.cpu_usage = 0.0;
    metadata.memory_usage = 0.0;
    metadata.is_healthy = true;
    metadata.last_heartbeat = Utils::getCurrentTimestamp();
    
    if (!metadata_manager_->registerServer(request->server_id(), metadata)) {
        response->set_success(false);
        response->set_message("Failed to register server");
        failed_requests_++;
        return grpc::Status::OK;
    }
    
    response->set_success(true);
    response->set_message("Server registered successfully");
    
    successful_requests_++;
    return grpc::Status::OK;
}

grpc::Status MasterServer::SendHeartbeat(grpc::ServerContext* context,
                                        const HeartbeatRequest* request,
                                        HeartbeatResponse* response) {
    // Don't count heartbeats in total requests (too frequent)
    
    ServerMetadata metadata;
    if (!metadata_manager_->getServerMetadata(request->server_id(), metadata)) {
        response->set_success(false);
        return grpc::Status::OK;
    }
    
    // Update server metadata
    metadata.free_space = request->free_space();
    metadata.chunk_count = request->chunk_count();
    metadata.cpu_usage = request->cpu_usage();
    metadata.memory_usage = request->memory_usage();
    metadata.last_heartbeat = Utils::getCurrentTimestamp();
    metadata.is_healthy = true;
    
    // Update stored chunks
    metadata.stored_chunks.clear();
    for (const std::string& chunk_id : request->stored_chunks()) {
        metadata.stored_chunks.insert(chunk_id);
    }
    
    metadata_manager_->updateServerMetadata(request->server_id(), metadata);
    
    // Check if rebalancing is needed
    if (chunk_allocator_->shouldRebalance()) {
        auto rebalancing_tasks = chunk_allocator_->generateRebalancingTasks();
        for (const auto& task : rebalancing_tasks) {
            ReplicationTask* proto_task = response->add_replication_tasks();
            proto_task->set_chunk_id(task.chunk_id);
            proto_task->set_source_server(task.source_server);
            proto_task->set_target_server(task.target_server);
            proto_task->set_is_urgent(task.is_urgent);
        }
    }
    
    response->set_success(true);
    return grpc::Status::OK;
}

grpc::Status MasterServer::ReplicateChunk(grpc::ServerContext* context,
                                         const ReplicateChunkRequest* request,
                                         ReplicateChunkResponse* response) {
    total_requests_++;
    
    Utils::logInfo("ReplicateChunk: " + request->chunk_id() + 
                   " from " + request->source_server() + 
                   " to " + request->target_server());
    
    // Add target server to chunk's location list
    ChunkMetadata metadata;
    if (metadata_manager_->getChunkMetadata(request->chunk_id(), metadata)) {
        if (std::find(metadata.server_locations.begin(), 
                     metadata.server_locations.end(), 
                     request->target_server()) == metadata.server_locations.end()) {
            metadata.server_locations.push_back(request->target_server());
            metadata_manager_->updateChunkLocations(request->chunk_id(), metadata.server_locations);
        }
    }
    
    response->set_success(true);
    response->set_message("Replication task acknowledged");
    
    successful_requests_++;
    return grpc::Status::OK;
}

grpc::Status MasterServer::DeleteChunk(grpc::ServerContext* context,
                                      const DeleteChunkRequest* request,
                                      DeleteChunkResponse* response) {
    total_requests_++;
    
    Utils::logInfo("DeleteChunk: " + request->chunk_id());
    
    if (!metadata_manager_->removeChunk(request->chunk_id())) {
        response->set_success(false);
        response->set_message("Chunk not found");
        failed_requests_++;
        return grpc::Status::OK;
    }
    
    response->set_success(true);
    response->set_message("Chunk deleted successfully");
    
    successful_requests_++;
    return grpc::Status::OK;
}

grpc::Status MasterServer::ReportChunkCorruption(grpc::ServerContext* context,
                                                const ChunkCorruptionRequest* request,
                                                ChunkCorruptionResponse* response) {
    total_requests_++;
    
    Utils::logError("Chunk corruption reported: " + request->chunk_id() + 
                    " on server " + request->server_id() + 
                    " - " + request->error_details());
    
    // Remove corrupted chunk from server
    metadata_manager_->removeChunkFromServer(request->chunk_id(), request->server_id());
    
    // Schedule re-replication if needed
    ChunkMetadata metadata;
    if (metadata_manager_->getChunkMetadata(request->chunk_id(), metadata)) {
        int replication_factor = Config::getInstance().getReplicationFactor();
        if (static_cast<int>(metadata.server_locations.size()) < replication_factor) {
            auto new_servers = chunk_allocator_->reallocateChunk(
                request->chunk_id(), {request->server_id()});
            
            if (!new_servers.empty()) {
                scheduleReplication(request->chunk_id(), new_servers);
            }
        }
    }
    
    response->set_acknowledged(true);
    
    successful_requests_++;
    return grpc::Status::OK;
}

void MasterServer::monitorHeartbeats() {
    const int CHECK_INTERVAL_MS = 10000; // Check every 10 seconds
    const int HEARTBEAT_TIMEOUT_MS = Config::getInstance().getHeartbeatTimeout();
    
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(CHECK_INTERVAL_MS));
        
        if (!running_.load()) break;
        
        auto servers = metadata_manager_->getAllServers();
        int64_t current_time = Utils::getCurrentTimestamp();
        
        for (const auto& server : servers) {
            if (server.is_healthy && 
                (current_time - server.last_heartbeat) > HEARTBEAT_TIMEOUT_MS) {
                Utils::logWarning("Server missed heartbeat: " + server.server_id);
                handleServerFailure(server.server_id);
            }
        }
    }
}

void MasterServer::performRebalancing() {
    const int REBALANCE_INTERVAL_MS = 60000; // Check every minute
    
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(REBALANCE_INTERVAL_MS));
        
        if (!running_.load()) break;
        
        if (chunk_allocator_->shouldRebalance()) {
            Utils::logInfo("Performing cluster rebalancing");
            auto tasks = chunk_allocator_->generateRebalancingTasks();
            
            // Tasks will be sent to chunk servers via heartbeat responses
            Utils::logInfo("Generated " + std::to_string(tasks.size()) + " rebalancing tasks");
        }
    }
}

void MasterServer::persistMetadata() {
    const int PERSISTENCE_INTERVAL_MS = 30000; // Save every 30 seconds
    
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(PERSISTENCE_INTERVAL_MS));
        
        if (!running_.load()) break;
        
        metadata_manager_->saveMetadataToFile("master_metadata.json");
        metadata_manager_->cleanupOrphanedChunks();
        metadata_manager_->cleanupDeadServers();
    }
}

void MasterServer::convertFileMetadataToProto(const FileMetadata& metadata, FileInfo* proto_info) {
    proto_info->set_filename(metadata.filename);
    proto_info->set_size(metadata.size);
    proto_info->set_created_time(metadata.created_time);
    proto_info->set_modified_time(metadata.modified_time);
    proto_info->set_is_encrypted(metadata.is_encrypted);
    proto_info->set_encryption_key_id(metadata.encryption_key_id);
    
    // Add chunk information
    for (const std::string& chunk_id : metadata.chunk_ids) {
        ChunkMetadata chunk_metadata;
        if (metadata_manager_->getChunkMetadata(chunk_id, chunk_metadata)) {
            ChunkInfo* chunk_info = proto_info->add_chunks();
            convertChunkMetadataToProto(chunk_metadata, chunk_info);
        }
    }
}

void MasterServer::convertChunkMetadataToProto(const ChunkMetadata& metadata, ChunkInfo* proto_info) {
    proto_info->set_chunk_id(metadata.chunk_id);
    proto_info->set_size(metadata.size);
    proto_info->set_checksum(metadata.checksum);
    proto_info->set_is_erasure_coded(metadata.is_erasure_coded);
    
    for (const std::string& server_id : metadata.server_locations) {
        ServerMetadata server_metadata;
        if (metadata_manager_->getServerMetadata(server_id, server_metadata)) {
            std::string address = server_metadata.address + ":" + std::to_string(server_metadata.port);
            proto_info->add_server_addresses(address);
        }
    }
}

void MasterServer::convertServerMetadataToProto(const ServerMetadata& metadata, ServerInfo* proto_info) {
    proto_info->set_server_id(metadata.server_id);
    proto_info->set_address(metadata.address);
    proto_info->set_port(metadata.port);
    proto_info->set_free_space(metadata.free_space);
    proto_info->set_chunk_count(metadata.chunk_count);
    proto_info->set_cpu_usage(metadata.cpu_usage);
    proto_info->set_memory_usage(metadata.memory_usage);
    proto_info->set_is_healthy(metadata.is_healthy);
}

bool MasterServer::validateFileName(const std::string& filename) {
    if (filename.empty() || filename.length() > 255) {
        return false;
    }
    
    // Check for invalid characters
    const std::string invalid_chars = "<>:\"|?*\0";
    return filename.find_first_of(invalid_chars) == std::string::npos;
}

void MasterServer::handleServerFailure(const std::string& server_id) {
    Utils::logWarning("Handling server failure: " + server_id);
    
    metadata_manager_->markServerUnhealthy(server_id);
    
    // Get all chunks stored on the failed server
    auto chunks = metadata_manager_->getChunksForServer(server_id);
    
    for (const std::string& chunk_id : chunks) {
        // Remove failed server from chunk locations
        metadata_manager_->removeChunkFromServer(chunk_id, server_id);
        
        // Check if we need to create new replicas
        ChunkMetadata metadata;
        if (metadata_manager_->getChunkMetadata(chunk_id, metadata)) {
            int target_replicas = metadata.is_erasure_coded ? 1 : 
                                 Config::getInstance().getReplicationFactor();
            
            if (static_cast<int>(metadata.server_locations.size()) < target_replicas) {
                auto new_servers = chunk_allocator_->reallocateChunk(chunk_id, {server_id});
                if (!new_servers.empty()) {
                    scheduleReplication(chunk_id, new_servers);
                }
            }
        }
    }
}

void MasterServer::scheduleReplication(const std::string& chunk_id, 
                                     const std::vector<std::string>& target_servers) {
    Utils::logInfo("Scheduled replication for chunk " + chunk_id + 
                   " to " + std::to_string(target_servers.size()) + " servers");
    
    // In this implementation, replication tasks are sent via heartbeat responses
    // The actual scheduling is handled by the heartbeat mechanism
}

} // namespace dfs

// Main function
int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <address> <port>" << std::endl;
        return 1;
    }
    
    std::string address = argv[1];
    int port = std::stoi(argv[2]);
    
    dfs::MasterServer server;
    server.start(address, port);
    
    return 0;
}