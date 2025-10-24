#include "file_system.pb.h"
#include "file_system.grpc.pb.h"
#include "chunk_storage.h"
#include "utils.h"
#include <grpcpp/grpcpp.h>
#include <memory>
#include <thread>
#include <atomic>

namespace dfs {

class ChunkServer final : public ChunkStorage::Service {
public:
    ChunkServer(const std::string& server_id, const std::string& storage_directory);
    ~ChunkServer();
    
    // Start the server
    void start(const std::string& address, int port, 
              const std::string& master_address, int master_port);
    void stop();
    
    // ChunkStorage service implementation
    grpc::Status WriteChunk(grpc::ServerContext* context,
                           const WriteChunkRequest* request,
                           WriteChunkResponse* response) override;
    
    grpc::Status ReadChunk(grpc::ServerContext* context,
                          const ReadChunkRequest* request,
                          ReadChunkResponse* response) override;
    
    grpc::Status CheckChunkIntegrity(grpc::ServerContext* context,
                                    const CheckIntegrityRequest* request,
                                    CheckIntegrityResponse* response) override;
    
    grpc::Status CopyChunk(grpc::ServerContext* context,
                          const CopyChunkRequest* request,
                          CopyChunkResponse* response) override;
    
private:
    std::string server_id_;
    std::string server_address_;
    int server_port_;
    std::string master_address_;
    int master_port_;
    
    std::unique_ptr<grpc::Server> server_;
    std::unique_ptr<dfs::ChunkStorage> storage_;
    std::unique_ptr<ChunkManagement::Stub> master_stub_;
    
    std::atomic<bool> running_;
    std::thread heartbeat_thread_;
    std::thread replication_thread_;
    std::thread maintenance_thread_;
    
    // Replication tasks queue
    std::queue<ReplicationTask> replication_queue_;
    std::mutex replication_mutex_;
    std::condition_variable replication_cv_;
    
    // Background tasks
    void sendHeartbeats();
    void processReplicationTasks();
    void performMaintenance();
    
    // Helper methods
    bool registerWithMaster();
    void handleReplicationTask(const ReplicationTask& task);
    bool copyChunkFromServer(const std::string& chunk_id, const std::string& source_server);
    void updateSystemMetrics();
    
    // System metrics
    double getCpuUsage();
    double getMemoryUsage();
    int64_t getTotalSpace();
    int64_t getFreeSpace();
    
    // Metrics
    std::atomic<int64_t> bytes_written_;
    std::atomic<int64_t> bytes_read_;
    std::atomic<int64_t> chunks_written_;
    std::atomic<int64_t> chunks_read_;
};

} // namespace dfs