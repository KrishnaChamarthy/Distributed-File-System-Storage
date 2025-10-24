#include "file_system.pb.h"
#include "file_system.grpc.pb.h"
#include "metadata_manager.h"
#include "chunk_allocator.h"
#include "utils.h"
#include <grpcpp/grpcpp.h>
#include <memory>
#include <thread>
#include <atomic>

namespace dfs {

class MasterServer final : public FileService::Service, public ChunkManagement::Service {
public:
    MasterServer();
    ~MasterServer();
    
    // Start the server
    void start(const std::string& address, int port);
    void stop();
    
    // FileService implementation
    grpc::Status CreateFile(grpc::ServerContext* context,
                           const CreateFileRequest* request,
                           CreateFileResponse* response) override;
    
    grpc::Status DeleteFile(grpc::ServerContext* context,
                           const DeleteFileRequest* request,
                           DeleteFileResponse* response) override;
    
    grpc::Status ListFiles(grpc::ServerContext* context,
                          const ListFilesRequest* request,
                          ListFilesResponse* response) override;
    
    grpc::Status GetFileInfo(grpc::ServerContext* context,
                            const GetFileInfoRequest* request,
                            GetFileInfoResponse* response) override;
    
    grpc::Status AllocateChunks(grpc::ServerContext* context,
                               const AllocateChunksRequest* request,
                               AllocateChunksResponse* response) override;
    
    grpc::Status GetChunkLocations(grpc::ServerContext* context,
                                  const GetChunkLocationsRequest* request,
                                  GetChunkLocationsResponse* response) override;
    
    grpc::Status CompleteUpload(grpc::ServerContext* context,
                               const CompleteUploadRequest* request,
                               CompleteUploadResponse* response) override;
    
    // ChunkManagement implementation
    grpc::Status RegisterChunkServer(grpc::ServerContext* context,
                                    const RegisterChunkServerRequest* request,
                                    RegisterChunkServerResponse* response) override;
    
    grpc::Status SendHeartbeat(grpc::ServerContext* context,
                              const HeartbeatRequest* request,
                              HeartbeatResponse* response) override;
    
    grpc::Status ReplicateChunk(grpc::ServerContext* context,
                               const ReplicateChunkRequest* request,
                               ReplicateChunkResponse* response) override;
    
    grpc::Status DeleteChunk(grpc::ServerContext* context,
                            const DeleteChunkRequest* request,
                            DeleteChunkResponse* response) override;
    
    grpc::Status ReportChunkCorruption(grpc::ServerContext* context,
                                      const ChunkCorruptionRequest* request,
                                      ChunkCorruptionResponse* response) override;
    
private:
    std::unique_ptr<grpc::Server> server_;
    std::shared_ptr<MetadataManager> metadata_manager_;
    std::unique_ptr<ChunkAllocator> chunk_allocator_;
    
    std::atomic<bool> running_;
    std::thread heartbeat_monitor_thread_;
    std::thread rebalancing_thread_;
    std::thread metadata_persistence_thread_;
    
    // Background tasks
    void monitorHeartbeats();
    void performRebalancing();
    void persistMetadata();
    
    // Helper methods
    void convertFileMetadataToProto(const FileMetadata& metadata, FileInfo* proto_info);
    void convertChunkMetadataToProto(const ChunkMetadata& metadata, ChunkInfo* proto_info);
    void convertServerMetadataToProto(const ServerMetadata& metadata, ServerInfo* proto_info);
    
    bool validateFileName(const std::string& filename);
    void handleServerFailure(const std::string& server_id);
    void scheduleReplication(const std::string& chunk_id, const std::vector<std::string>& target_servers);
    
    // Metrics
    std::atomic<int64_t> total_requests_;
    std::atomic<int64_t> successful_requests_;
    std::atomic<int64_t> failed_requests_;
};

} // namespace dfs