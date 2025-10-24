#include "test_framework.h"
#include "../src/master/metadata_manager.h"

namespace dfs {
namespace test {

class MetadataManagerTest : public DFSTestBase {
protected:
    void SetUp() override {
        DFSTestBase::SetUp();
        metadata_manager_ = std::make_unique<MetadataManager>();
    }
    
    void TearDown() override {
        metadata_manager_.reset();
        DFSTestBase::TearDown();
    }
    
    std::unique_ptr<MetadataManager> metadata_manager_;
};

TEST_F(MetadataManagerTest, AddAndGetFile) {
    FileMetadata file_metadata;
    file_metadata.filename = "test_file.txt";
    file_metadata.size = 1024;
    file_metadata.created_time = Utils::getCurrentTimestamp();
    file_metadata.is_encrypted = true;
    file_metadata.is_erasure_coded = false;
    file_metadata.chunk_ids = {"chunk1", "chunk2", "chunk3"};
    
    ASSERT_TRUE(metadata_manager_->addFile(file_metadata));
    
    auto retrieved = metadata_manager_->getFile("test_file.txt");
    ASSERT_TRUE(retrieved.has_value());
    ASSERT_EQ(retrieved->filename, file_metadata.filename);
    ASSERT_EQ(retrieved->size, file_metadata.size);
    ASSERT_EQ(retrieved->is_encrypted, file_metadata.is_encrypted);
    ASSERT_EQ(retrieved->chunk_ids, file_metadata.chunk_ids);
}

TEST_F(MetadataManagerTest, RemoveFile) {
    FileMetadata file_metadata;
    file_metadata.filename = "to_be_removed.txt";
    file_metadata.size = 512;
    file_metadata.created_time = Utils::getCurrentTimestamp();
    
    ASSERT_TRUE(metadata_manager_->addFile(file_metadata));
    ASSERT_TRUE(metadata_manager_->getFile("to_be_removed.txt").has_value());
    
    ASSERT_TRUE(metadata_manager_->removeFile("to_be_removed.txt"));
    ASSERT_FALSE(metadata_manager_->getFile("to_be_removed.txt").has_value());
}

TEST_F(MetadataManagerTest, ListFiles) {
    std::vector<FileMetadata> test_files;
    
    for (int i = 0; i < 5; ++i) {
        FileMetadata file;
        file.filename = "file_" + std::to_string(i) + ".txt";
        file.size = 1024 * (i + 1);
        file.created_time = Utils::getCurrentTimestamp();
        test_files.push_back(file);
        
        ASSERT_TRUE(metadata_manager_->addFile(file));
    }
    
    auto listed_files = metadata_manager_->listFiles();
    ASSERT_EQ(listed_files.size(), 5);
    
    // Check that all files are present
    for (const auto& test_file : test_files) {
        bool found = false;
        for (const auto& listed_file : listed_files) {
            if (listed_file.filename == test_file.filename) {
                found = true;
                ASSERT_EQ(listed_file.size, test_file.size);
                break;
            }
        }
        ASSERT_TRUE(found) << "File " << test_file.filename << " not found in list";
    }
}

TEST_F(MetadataManagerTest, AddAndGetChunk) {
    ChunkMetadata chunk_metadata;
    chunk_metadata.chunk_id = "test_chunk_001";
    chunk_metadata.size = 64 * 1024 * 1024; // 64MB
    chunk_metadata.checksum = "abc123def456";
    chunk_metadata.server_locations = {"server1", "server2", "server3"};
    chunk_metadata.created_time = Utils::getCurrentTimestamp();
    
    ASSERT_TRUE(metadata_manager_->addChunk(chunk_metadata));
    
    auto retrieved = metadata_manager_->getChunk("test_chunk_001");
    ASSERT_TRUE(retrieved.has_value());
    ASSERT_EQ(retrieved->chunk_id, chunk_metadata.chunk_id);
    ASSERT_EQ(retrieved->size, chunk_metadata.size);
    ASSERT_EQ(retrieved->checksum, chunk_metadata.checksum);
    ASSERT_EQ(retrieved->server_locations, chunk_metadata.server_locations);
}

TEST_F(MetadataManagerTest, ServerManagement) {
    ServerMetadata server1;
    server1.server_id = "chunk_server_1";
    server1.address = "192.168.1.10";
    server1.port = 60051;
    server1.is_healthy = true;
    server1.free_space = 1000000000; // 1GB
    server1.total_space = 2000000000; // 2GB
    server1.last_heartbeat = Utils::getCurrentTimestamp();
    
    ServerMetadata server2;
    server2.server_id = "chunk_server_2";
    server2.address = "192.168.1.11";
    server2.port = 60052;
    server2.is_healthy = false;
    server2.free_space = 500000000;
    server2.total_space = 1000000000;
    server2.last_heartbeat = Utils::getCurrentTimestamp() - 120; // 2 minutes ago
    
    ASSERT_TRUE(metadata_manager_->addServer(server1));
    ASSERT_TRUE(metadata_manager_->addServer(server2));
    
    auto servers = metadata_manager_->getAllServers();
    ASSERT_EQ(servers.size(), 2);
    
    // Update server status
    ASSERT_TRUE(metadata_manager_->updateServerStatus("chunk_server_2", true));
    
    servers = metadata_manager_->getAllServers();
    bool server2_found = false;
    for (const auto& server : servers) {
        if (server.server_id == "chunk_server_2") {
            ASSERT_TRUE(server.is_healthy);
            server2_found = true;
            break;
        }
    }
    ASSERT_TRUE(server2_found);
}

TEST_F(MetadataManagerTest, Statistics) {
    // Add some test data
    for (int i = 0; i < 10; ++i) {
        FileMetadata file;
        file.filename = "stats_file_" + std::to_string(i) + ".txt";
        file.size = (i + 1) * 1024;
        file.chunk_ids = {"chunk_" + std::to_string(i * 2), "chunk_" + std::to_string(i * 2 + 1)};
        metadata_manager_->addFile(file);
    }
    
    for (int i = 0; i < 5; ++i) {
        ServerMetadata server;
        server.server_id = "stats_server_" + std::to_string(i);
        server.address = "192.168.1." + std::to_string(10 + i);
        server.port = 60051 + i;
        server.is_healthy = (i < 4); // 4 healthy, 1 unhealthy
        server.free_space = (i + 1) * 1000000000ULL;
        server.total_space = (i + 2) * 1000000000ULL;
        metadata_manager_->addServer(server);
    }
    
    auto stats = metadata_manager_->getStatistics();
    ASSERT_EQ(stats.total_files, 10);
    ASSERT_EQ(stats.total_servers, 5);
    ASSERT_EQ(stats.healthy_servers, 4);
    ASSERT_GT(stats.total_storage_available, 0);
}

TEST_F(MetadataManagerTest, ConcurrentAccess) {
    const int num_threads = 10;
    const int operations_per_thread = 100;
    
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, operations_per_thread, &success_count]() {
            for (int i = 0; i < operations_per_thread; ++i) {
                FileMetadata file;
                file.filename = "thread_" + std::to_string(t) + "_file_" + std::to_string(i) + ".txt";
                file.size = 1024;
                file.created_time = Utils::getCurrentTimestamp();
                
                if (metadata_manager_->addFile(file)) {
                    auto retrieved = metadata_manager_->getFile(file.filename);
                    if (retrieved.has_value() && retrieved->filename == file.filename) {
                        success_count++;
                    }
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    ASSERT_EQ(success_count.load(), num_threads * operations_per_thread);
    
    auto all_files = metadata_manager_->listFiles();
    ASSERT_EQ(all_files.size(), num_threads * operations_per_thread);
}

PERFORMANCE_TEST(MetadataOperationsPerformance, 1000)
    FileMetadata file;
    file.filename = "perf_file_" + std::to_string(rand()) + ".txt";
    file.size = 1024;
    file.created_time = Utils::getCurrentTimestamp();
    
    ASSERT_TRUE(metadata_manager_->addFile(file));
    
    auto retrieved = metadata_manager_->getFile(file.filename);
    ASSERT_TRUE(retrieved.has_value());
    
    ASSERT_TRUE(metadata_manager_->removeFile(file.filename));
END_PERFORMANCE_TEST()

} // namespace test
} // namespace dfs