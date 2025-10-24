#include "chunk_server.h"
#include "crypto.h"
#include <iostream>
#include <fstream>
#include <signal.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <queue>

namespace dfs {

// Global pointer for signal handling
static ChunkServer* g_chunk_server = nullptr;

void chunkServerSignalHandler(int signal) {
    if (g_chunk_server) {
        Utils::logInfo("Received signal " + std::to_string(signal) + ", shutting down...");
        g_chunk_server->stop();
    }
}

ChunkServer::ChunkServer(const std::string& server_id, const std::string& storage_directory)
    : server_id_(server_id),
      running_(false),
      bytes_written_(0),
      bytes_read_(0),
      chunks_written_(0),
      chunks_read_(0) {
    
    storage_ = std::make_unique<dfs::ChunkStorage>(storage_directory);
    
    Utils::logInfo("ChunkServer " + server_id_ + " initialized with storage at " + storage_directory);
}

ChunkServer::~ChunkServer() {
    stop();
    Utils::logInfo("ChunkServer " + server_id_ + " destroyed");
}

void ChunkServer::start(const std::string& address, int port, 
                       const std::string& master_address, int master_port) {
    if (running_.load()) {
        Utils::logWarning("Server is already running");
        return;
    }
    
    server_address_ = address;
    server_port_ = port;
    master_address_ = master_address;
    master_port_ = master_port;
    
    std::string server_address = address + ":" + std::to_string(port);
    
    // Setup gRPC server
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(this);
    
    // Set max message size (for large chunks)
    builder.SetMaxReceiveMessageSize(64 * 1024 * 1024); // 64MB
    builder.SetMaxSendMessageSize(64 * 1024 * 1024);    // 64MB
    
    server_ = builder.BuildAndStart();
    
    if (!server_) {
        Utils::logError("Failed to start gRPC server");
        return;
    }
    
    // Setup master connection
    std::string master_addr = master_address + ":" + std::to_string(master_port);
    auto channel = grpc::CreateChannel(master_addr, grpc::InsecureChannelCredentials());
    master_stub_ = ChunkManagement::NewStub(channel);
    
    // Register with master
    if (!registerWithMaster()) {
        Utils::logError("Failed to register with master");
        server_->Shutdown();
        return;
    }
    
    running_.store(true);
    
    // Set up signal handling
    g_chunk_server = this;
    signal(SIGINT, chunkServerSignalHandler);
    signal(SIGTERM, chunkServerSignalHandler);
    
    // Start background threads
    heartbeat_thread_ = std::thread(&ChunkServer::sendHeartbeats, this);
    replication_thread_ = std::thread(&ChunkServer::processReplicationTasks, this);
    maintenance_thread_ = std::thread(&ChunkServer::performMaintenance, this);
    
    Utils::logInfo("ChunkServer " + server_id_ + " started on " + server_address);
    
    // Wait for shutdown
    server_->Wait();
}

void ChunkServer::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    if (server_) {
        server_->Shutdown();
    }
    
    // Notify replication thread
    replication_cv_.notify_all();
    
    // Wait for background threads to finish
    if (heartbeat_thread_.joinable()) {
        heartbeat_thread_.join();
    }
    if (replication_thread_.joinable()) {
        replication_thread_.join();
    }
    if (maintenance_thread_.joinable()) {
        maintenance_thread_.join();
    }
    
    Utils::logInfo("ChunkServer " + server_id_ + " stopped");
}

grpc::Status ChunkServer::WriteChunk(grpc::ServerContext* context,
                                    const WriteChunkRequest* request,
                                    WriteChunkResponse* response) {
    const std::string& chunk_id = request->chunk_id();
    
    Utils::logDebug("WriteChunk request for: " + chunk_id);
    
    // Convert protobuf bytes to vector
    std::vector<uint8_t> data(request->data().begin(), request->data().end());
    
    // Verify checksum if provided
    if (!request->checksum().empty()) {
        std::string actual_checksum = Utils::calculateSHA256(data);
        if (actual_checksum != request->checksum()) {
            response->set_success(false);
            response->set_message("Checksum mismatch");
            Utils::logError("Checksum mismatch for chunk " + chunk_id);
            return grpc::Status::OK;
        }
    }
    
    // Handle encryption
    std::vector<uint8_t> final_data = data;
    if (request->is_encrypted()) {
        // Data is already encrypted by client, just store it
        Utils::logDebug("Storing encrypted chunk: " + chunk_id);
    }
    
    // Write chunk to storage
    bool success = storage_->writeChunk(chunk_id, final_data, 
                                       request->is_encrypted(), 
                                       request->is_erasure_coded());
    
    if (success) {
        response->set_success(true);
        response->set_stored_checksum(Utils::calculateSHA256(final_data));
        response->set_message("Chunk written successfully");
        
        // Update metrics
        bytes_written_ += data.size();
        chunks_written_++;
        
        Utils::logInfo("Successfully wrote chunk " + chunk_id + 
                      " (" + std::to_string(data.size()) + " bytes)");
    } else {
        response->set_success(false);
        response->set_message("Failed to write chunk to storage");
        Utils::logError("Failed to write chunk " + chunk_id);
    }
    
    return grpc::Status::OK;
}

grpc::Status ChunkServer::ReadChunk(grpc::ServerContext* context,
                                   const ReadChunkRequest* request,
                                   ReadChunkResponse* response) {
    const std::string& chunk_id = request->chunk_id();
    
    Utils::logDebug("ReadChunk request for: " + chunk_id);
    
    // Read chunk from storage
    std::vector<uint8_t> data = storage_->readChunk(chunk_id);
    
    if (data.empty()) {
        response->set_success(false);
        response->set_message("Chunk not found or corrupted");
        Utils::logWarning("Failed to read chunk " + chunk_id);
        return grpc::Status::OK;
    }
    
    // Verify integrity if requested
    if (request->verify_integrity()) {
        if (!storage_->verifyChunkIntegrity(chunk_id)) {
            response->set_success(false);
            response->set_message("Chunk integrity verification failed");
            Utils::logError("Integrity verification failed for chunk " + chunk_id);
            return grpc::Status::OK;
        }
    }
    
    // Set response data
    response->set_success(true);
    response->set_data(std::string(data.begin(), data.end()));
    response->set_checksum(Utils::calculateSHA256(data));
    response->set_message("Chunk read successfully");
    
    // Update metrics
    bytes_read_ += data.size();
    chunks_read_++;
    
    Utils::logDebug("Successfully read chunk " + chunk_id + 
                   " (" + std::to_string(data.size()) + " bytes)");
    
    return grpc::Status::OK;
}

grpc::Status ChunkServer::CheckChunkIntegrity(grpc::ServerContext* context,
                                             const CheckIntegrityRequest* request,
                                             CheckIntegrityResponse* response) {
    const std::string& chunk_id = request->chunk_id();
    
    bool is_valid = storage_->verifyChunkIntegrity(chunk_id);
    std::string checksum = storage_->getChunkChecksum(chunk_id);
    
    response->set_is_valid(is_valid);
    response->set_checksum(checksum);
    
    Utils::logDebug("Integrity check for chunk " + chunk_id + ": " + 
                   (is_valid ? "VALID" : "INVALID"));
    
    return grpc::Status::OK;
}

grpc::Status ChunkServer::CopyChunk(grpc::ServerContext* context,
                                   const CopyChunkRequest* request,
                                   CopyChunkResponse* response) {
    const std::string& chunk_id = request->chunk_id();
    const std::string& source_server = request->source_server();
    
    Utils::logInfo("CopyChunk request: " + chunk_id + " from " + source_server);
    
    bool success = copyChunkFromServer(chunk_id, source_server);
    
    if (success) {
        response->set_success(true);
        response->set_message("Chunk copied successfully");
        Utils::logInfo("Successfully copied chunk " + chunk_id + " from " + source_server);
    } else {
        response->set_success(false);
        response->set_message("Failed to copy chunk");
        Utils::logError("Failed to copy chunk " + chunk_id + " from " + source_server);
    }
    
    return grpc::Status::OK;
}

void ChunkServer::sendHeartbeats() {
    const int heartbeat_interval = Config::getInstance().getHeartbeatInterval();
    
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(heartbeat_interval));
        
        if (!running_.load()) break;
        
        HeartbeatRequest request;
        request.set_server_id(server_id_);
        request.set_free_space(getFreeSpace());
        request.set_chunk_count(storage_->getChunkCount());
        request.set_cpu_usage(getCpuUsage());
        request.set_memory_usage(getMemoryUsage());
        
        // Add list of stored chunks
        auto chunk_ids = storage_->getAllChunkIds();
        for (const std::string& chunk_id : chunk_ids) {
            request.add_stored_chunks(chunk_id);
        }
        
        HeartbeatResponse response;
        grpc::ClientContext context;
        
        grpc::Status status = master_stub_->SendHeartbeat(&context, request, &response);
        
        if (status.ok() && response.success()) {
            // Process any replication tasks
            for (const ReplicationTask& task : response.replication_tasks()) {
                std::lock_guard<std::mutex> lock(replication_mutex_);
                replication_queue_.push(task);
                replication_cv_.notify_one();
            }
            
            // Process chunks to delete
            for (const std::string& chunk_id : response.chunks_to_delete()) {
                if (storage_->chunkExists(chunk_id)) {
                    storage_->deleteChunk(chunk_id);
                    Utils::logInfo("Deleted chunk as requested by master: " + chunk_id);
                }
            }
        } else {
            Utils::logWarning("Heartbeat failed: " + status.error_message());
        }
    }
}

void ChunkServer::processReplicationTasks() {
    while (running_.load()) {
        std::unique_lock<std::mutex> lock(replication_mutex_);
        
        replication_cv_.wait(lock, [this] { 
            return !replication_queue_.empty() || !running_.load(); 
        });
        
        if (!running_.load()) break;
        
        while (!replication_queue_.empty()) {
            ReplicationTask task = replication_queue_.front();
            replication_queue_.pop();
            lock.unlock();
            
            handleReplicationTask(task);
            
            lock.lock();
        }
    }
}

void ChunkServer::performMaintenance() {
    const int maintenance_interval = 300000; // 5 minutes
    
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(maintenance_interval));
        
        if (!running_.load()) break;
        
        Utils::logInfo("Performing maintenance tasks");
        
        // Garbage collection
        storage_->performGarbageCollection();
        
        // Update system metrics
        updateSystemMetrics();
        
        // Log statistics
        Utils::logInfo("Storage stats - Chunks: " + std::to_string(storage_->getChunkCount()) +
                      ", Used: " + std::to_string(storage_->getTotalStorageUsed()) + " bytes" +
                      ", Available: " + std::to_string(storage_->getAvailableStorage()) + " bytes");
    }
}

bool ChunkServer::registerWithMaster() {
    RegisterChunkServerRequest request;
    request.set_server_id(server_id_);
    request.set_address(server_address_);
    request.set_port(server_port_);
    request.set_total_space(getTotalSpace());
    
    RegisterChunkServerResponse response;
    grpc::ClientContext context;
    
    grpc::Status status = master_stub_->RegisterChunkServer(&context, request, &response);
    
    if (status.ok() && response.success()) {
        Utils::logInfo("Successfully registered with master");
        return true;
    } else {
        Utils::logError("Failed to register with master: " + 
                       (status.ok() ? response.message() : status.error_message()));
        return false;
    }
}

void ChunkServer::handleReplicationTask(const ReplicationTask& task) {
    Utils::logInfo("Processing replication task: " + task.chunk_id() + 
                   " from " + task.source_server() + " to " + task.target_server());
    
    if (task.target_server() == server_id_) {
        // We are the target, copy chunk from source
        copyChunkFromServer(task.chunk_id(), task.source_server());
    } else if (task.source_server() == server_id_) {
        // We are the source, the target will copy from us
        Utils::logDebug("Serving as source for replication task: " + task.chunk_id());
    }
}

bool ChunkServer::copyChunkFromServer(const std::string& chunk_id, const std::string& source_server) {
    try {
        // Parse source server address
        auto parts = Utils::splitString(source_server, ':');
        if (parts.size() != 2) {
            Utils::logError("Invalid source server address: " + source_server);
            return false;
        }
        
        std::string source_address = parts[0] + ":" + parts[1];
        
        // Create connection to source server
        auto channel = grpc::CreateChannel(source_address, grpc::InsecureChannelCredentials());
        auto stub = dfs::ChunkStorage::NewStub(channel);
        
        // Request chunk from source
        ReadChunkRequest request;
        request.set_chunk_id(chunk_id);
        request.set_verify_integrity(true);
        
        ReadChunkResponse response;
        grpc::ClientContext context;
        
        grpc::Status status = stub->ReadChunk(&context, request, &response);
        
        if (!status.ok() || !response.success()) {
            Utils::logError("Failed to read chunk from source: " + 
                           (status.ok() ? response.message() : status.error_message()));
            return false;
        }
        
        // Write chunk to local storage
        std::vector<uint8_t> data(response.data().begin(), response.data().end());
        
        bool success = storage_->writeChunk(chunk_id, data, false, false);
        
        if (success) {
            Utils::logInfo("Successfully copied chunk " + chunk_id + " from " + source_server);
        } else {
            Utils::logError("Failed to write copied chunk " + chunk_id);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        Utils::logError("Exception during chunk copy: " + std::string(e.what()));
        return false;
    }
}

void ChunkServer::updateSystemMetrics() {
    // Update metrics object
    Metrics& metrics = Metrics::getInstance();
    metrics.updateDiskUsage(storage_->getTotalStorageUsed());
    metrics.updateMemoryUsage(getMemoryUsage());
    metrics.updateCpuUsage(getCpuUsage());
}

double ChunkServer::getCpuUsage() {
    // Simple CPU usage calculation
    // In a real implementation, you'd use system calls to get actual CPU usage
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> dis(10.0, 80.0);
    
    return dis(gen); // Return random value for demonstration
}

double ChunkServer::getMemoryUsage() {
    // Simple memory usage calculation
    // In a real implementation, you'd use system calls to get actual memory usage
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> dis(20.0, 70.0);
    
    return dis(gen); // Return random value for demonstration
}

int64_t ChunkServer::getTotalSpace() {
    struct statvfs stat;
    
    if (statvfs(storage_->getAllChunkIds().empty() ? "." : 
                storage_->getAllChunkIds()[0].c_str(), &stat) != 0) {
        return 1000LL * 1024 * 1024 * 1024; // Default 1TB
    }
    
    return static_cast<int64_t>(stat.f_blocks) * stat.f_frsize;
}

int64_t ChunkServer::getFreeSpace() {
    return storage_->getAvailableStorage();
}

} // namespace dfs

// Main function
int main(int argc, char** argv) {
    if (argc != 6) {
        std::cerr << "Usage: " << argv[0] << " <server_id> <address> <port> <master_address> <master_port>" << std::endl;
        return 1;
    }
    
    std::string server_id = argv[1];
    std::string address = argv[2];
    int port = std::stoi(argv[3]);
    std::string master_address = argv[4];
    int master_port = std::stoi(argv[5]);
    
    // Create storage directory
    std::string storage_dir = "./data/chunks_" + std::to_string(port);
    
    dfs::ChunkServer server(server_id, storage_dir);
    server.start(address, port, master_address, master_port);
    
    return 0;
}