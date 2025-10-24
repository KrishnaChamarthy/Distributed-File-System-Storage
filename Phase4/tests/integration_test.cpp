#include "test_framework.h"
#include "../src/client/client.h"

namespace dfs {
namespace test {

class IntegrationTest : public DFSTestBase {
protected:
    void SetUp() override {
        DFSTestBase::SetUp();
        
        // Start test cluster
        env_ = DFSTestEnvironment::getInstance();
        if (env_) {
            env_->startTestCluster(3);
            
            // Wait for cluster to be ready
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
        
        // Create DFS client
        client_ = std::make_unique<DFSClient>("localhost:50051");
    }
    
    void TearDown() override {
        client_.reset();
        
        if (env_) {
            env_->stopTestCluster();
        }
        
        DFSTestBase::TearDown();
    }
    
    DFSTestEnvironment* env_;
    std::unique_ptr<DFSClient> client_;
};

TEST_F(IntegrationTest, BasicFileOperations) {
    std::string filename = "integration_test_file.txt";
    std::string content = "This is integration test content for the DFS system.";
    
    // Create a temporary file
    std::string temp_file = createTempFile(content);
    
    // Upload file
    UploadOptions upload_opts;
    upload_opts.encrypt = false;
    upload_opts.use_erasure_coding = false;
    upload_opts.replication_factor = 3;
    
    auto upload_result = client_->putFile(temp_file, filename, upload_opts);
    ASSERT_DFS_OK(upload_result);
    
    // Download file
    std::string download_path = test_dir_ + "/downloaded_" + filename;
    DownloadOptions download_opts;
    
    auto download_result = client_->getFile(filename, download_path, download_opts);
    ASSERT_DFS_OK(download_result);
    
    // Verify content
    std::ifstream downloaded_file(download_path);
    std::string downloaded_content((std::istreambuf_iterator<char>(downloaded_file)),
                                   std::istreambuf_iterator<char>());
    
    ASSERT_EQ(downloaded_content, content);
}

TEST_F(IntegrationTest, EncryptedFileOperations) {
    std::string filename = "encrypted_test_file.txt";
    std::string content = generateRandomData(10240); // 10KB random data
    std::string password = "secure_test_password_123";
    
    std::string temp_file = createTempFile(content);
    
    // Upload with encryption
    UploadOptions upload_opts;
    upload_opts.encrypt = true;
    upload_opts.password = password;
    upload_opts.use_erasure_coding = false;
    upload_opts.replication_factor = 2;
    
    auto upload_result = client_->putFile(temp_file, filename, upload_opts);
    ASSERT_DFS_OK(upload_result);
    
    // Download with correct password
    std::string download_path = test_dir_ + "/decrypted_" + filename;
    DownloadOptions download_opts;
    download_opts.password = password;
    
    auto download_result = client_->getFile(filename, download_path, download_opts);
    ASSERT_DFS_OK(download_result);
    
    // Verify content
    std::ifstream downloaded_file(download_path);
    std::string downloaded_content((std::istreambuf_iterator<char>(downloaded_file)),
                                   std::istreambuf_iterator<char>());
    
    ASSERT_EQ(downloaded_content, content);
    
    // Try download with wrong password
    std::string wrong_download_path = test_dir_ + "/wrong_password_" + filename;
    DownloadOptions wrong_opts;
    wrong_opts.password = "wrong_password";
    
    auto wrong_result = client_->getFile(filename, wrong_download_path, wrong_opts);
    ASSERT_DFS_ERROR(wrong_result);
}

TEST_F(IntegrationTest, ErasureCodedFileOperations) {
    std::string filename = "erasure_coded_file.bin";
    std::vector<uint8_t> binary_data = TestDataGenerator::generateRandom(256 * 1024); // 256KB
    
    // Create binary file
    std::string temp_file = test_dir_ + "/temp_binary.bin";
    std::ofstream file(temp_file, std::ios::binary);
    file.write(reinterpret_cast<const char*>(binary_data.data()), binary_data.size());
    file.close();
    
    // Upload with erasure coding
    UploadOptions upload_opts;
    upload_opts.encrypt = false;
    upload_opts.use_erasure_coding = true;
    upload_opts.replication_factor = 1; // No additional replication needed with EC
    
    auto upload_result = client_->putFile(temp_file, filename, upload_opts);
    ASSERT_DFS_OK(upload_result);
    
    // Download
    std::string download_path = test_dir_ + "/downloaded_binary.bin";
    DownloadOptions download_opts;
    
    auto download_result = client_->getFile(filename, download_path, download_opts);
    ASSERT_DFS_OK(download_result);
    
    // Verify binary content
    std::ifstream downloaded_file(download_path, std::ios::binary);
    std::vector<uint8_t> downloaded_data((std::istreambuf_iterator<char>(downloaded_file)),
                                         std::istreambuf_iterator<char>());
    
    ASSERT_EQ(downloaded_data, binary_data);
}

TEST_F(IntegrationTest, LargeFileOperations) {
    std::string filename = "large_test_file.dat";
    size_t large_size = 10 * 1024 * 1024; // 10MB
    std::vector<uint8_t> large_data = TestDataGenerator::generateSequential(large_size);
    
    // Create large file
    std::string temp_file = test_dir_ + "/large_temp.dat";
    std::ofstream file(temp_file, std::ios::binary);
    file.write(reinterpret_cast<const char*>(large_data.data()), large_data.size());
    file.close();
    
    PerformanceTimer timer;
    timer.start();
    
    // Upload large file
    UploadOptions upload_opts;
    upload_opts.encrypt = false;
    upload_opts.use_erasure_coding = true;
    upload_opts.replication_factor = 2;
    
    auto upload_result = client_->putFile(temp_file, filename, upload_opts);
    
    timer.stop();
    std::cout << "Upload of 10MB took: " << timer.getElapsedSeconds() << " seconds" << std::endl;
    
    ASSERT_DFS_OK(upload_result);
    
    timer.start();
    
    // Download large file
    std::string download_path = test_dir_ + "/downloaded_large.dat";
    DownloadOptions download_opts;
    
    auto download_result = client_->getFile(filename, download_path, download_opts);
    
    timer.stop();
    std::cout << "Download of 10MB took: " << timer.getElapsedSeconds() << " seconds" << std::endl;
    
    ASSERT_DFS_OK(download_result);
    
    // Verify size (content verification would be too slow for integration test)
    std::ifstream downloaded_file(download_path, std::ios::binary | std::ios::ate);
    size_t downloaded_size = downloaded_file.tellg();
    ASSERT_EQ(downloaded_size, large_size);
}

TEST_F(IntegrationTest, ConcurrentOperations) {
    const int num_concurrent_operations = 10;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    for (int i = 0; i < num_concurrent_operations; ++i) {
        threads.emplace_back([this, i, &success_count]() {
            std::string filename = "concurrent_file_" + std::to_string(i) + ".txt";
            std::string content = "Concurrent test content for file " + std::to_string(i);
            
            std::string temp_file = createTempFile(content);
            
            UploadOptions upload_opts;
            upload_opts.encrypt = false;
            upload_opts.use_erasure_coding = false;
            upload_opts.replication_factor = 2;
            
            auto upload_result = client_->putFile(temp_file, filename, upload_opts);
            if (upload_result.ok()) {
                std::string download_path = test_dir_ + "/concurrent_download_" + std::to_string(i) + ".txt";
                DownloadOptions download_opts;
                
                auto download_result = client_->getFile(filename, download_path, download_opts);
                if (download_result.ok()) {
                    success_count++;
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    ASSERT_EQ(success_count.load(), num_concurrent_operations);
}

TEST_F(IntegrationTest, FileListingAndDeletion) {
    // Upload several files
    std::vector<std::string> test_files = {
        "list_test_1.txt",
        "list_test_2.txt", 
        "list_test_3.txt"
    };
    
    for (const auto& filename : test_files) {
        std::string content = "Content for " + filename;
        std::string temp_file = createTempFile(content);
        
        UploadOptions opts;
        opts.replication_factor = 2;
        
        auto result = client_->putFile(temp_file, filename, opts);
        ASSERT_DFS_OK(result);
    }
    
    // List files
    auto list_result = client_->listFiles();
    ASSERT_DFS_OK(list_result);
    
    std::vector<std::string> listed_files = list_result.value();
    
    // Verify all test files are listed
    for (const auto& test_file : test_files) {
        bool found = std::find(listed_files.begin(), listed_files.end(), test_file) != listed_files.end();
        ASSERT_TRUE(found) << "File " << test_file << " not found in listing";
    }
    
    // Delete files
    for (const auto& filename : test_files) {
        auto delete_result = client_->deleteFile(filename);
        ASSERT_DFS_OK(delete_result);
    }
    
    // Verify files are deleted
    auto final_list = client_->listFiles();
    ASSERT_DFS_OK(final_list);
    
    std::vector<std::string> final_files = final_list.value();
    for (const auto& test_file : test_files) {
        bool found = std::find(final_files.begin(), final_files.end(), test_file) != final_files.end();
        ASSERT_FALSE(found) << "File " << test_file << " still exists after deletion";
    }
}

class FaultToleranceTest : public IntegrationTest {
protected:
    void SetUp() override {
        IntegrationTest::SetUp();
        fault_injector_ = &env_->getFaultInjector();
    }
    
    FaultInjector* fault_injector_;
};

TEST_F(FaultToleranceTest, ChunkServerFailure) {
    std::string filename = "fault_tolerance_test.txt";
    std::string content = "This file should survive chunk server failures.";
    std::string temp_file = createTempFile(content);
    
    // Upload with replication
    UploadOptions upload_opts;
    upload_opts.replication_factor = 3;
    
    auto upload_result = client_->putFile(temp_file, filename, upload_opts);
    ASSERT_DFS_OK(upload_result);
    
    // Simulate chunk server failure
    fault_injector_->injectFault(FaultInjector::FaultType::SERVER_CRASH, "chunk-server-1", 1.0);
    
    // File should still be downloadable
    std::string download_path = test_dir_ + "/fault_tolerant_download.txt";
    DownloadOptions download_opts;
    
    auto download_result = client_->getFile(filename, download_path, download_opts);
    ASSERT_DFS_OK(download_result);
    
    // Verify content
    std::ifstream downloaded_file(download_path);
    std::string downloaded_content((std::istreambuf_iterator<char>(downloaded_file)),
                                   std::istreambuf_iterator<char>());
    
    ASSERT_EQ(downloaded_content, content);
    
    // Clean up fault injection
    fault_injector_->removeFault(FaultInjector::FaultType::SERVER_CRASH, "chunk-server-1");
}

class PerformanceIntegrationTest : public IntegrationTest {
protected:
    void SetUp() override {
        IntegrationTest::SetUp();
        metrics_ = &env_->getMetrics();
    }
    
    PerformanceMetrics* metrics_;
};

TEST_F(PerformanceIntegrationTest, ThroughputMeasurement) {
    const size_t num_files = 20;
    const size_t file_size = 1024 * 1024; // 1MB each
    
    std::vector<std::string> filenames;
    std::vector<std::string> temp_files;
    
    // Prepare test files
    for (size_t i = 0; i < num_files; ++i) {
        std::string filename = "throughput_test_" + std::to_string(i) + ".dat";
        filenames.push_back(filename);
        
        std::vector<uint8_t> data = TestDataGenerator::generateRandom(file_size);
        std::string temp_file = test_dir_ + "/throughput_temp_" + std::to_string(i) + ".dat";
        
        std::ofstream file(temp_file, std::ios::binary);
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        file.close();
        
        temp_files.push_back(temp_file);
    }
    
    PerformanceTimer upload_timer;
    upload_timer.start();
    
    // Upload all files
    for (size_t i = 0; i < num_files; ++i) {
        UploadOptions opts;
        opts.replication_factor = 2;
        
        auto result = client_->putFile(temp_files[i], filenames[i], opts);
        ASSERT_DFS_OK(result);
    }
    
    upload_timer.stop();
    
    double upload_throughput = (num_files * file_size) / (1024.0 * 1024.0) / upload_timer.getElapsedSeconds();
    std::cout << "Upload throughput: " << upload_throughput << " MB/s" << std::endl;
    
    PerformanceTimer download_timer;
    download_timer.start();
    
    // Download all files
    for (size_t i = 0; i < num_files; ++i) {
        std::string download_path = test_dir_ + "/throughput_download_" + std::to_string(i) + ".dat";
        DownloadOptions opts;
        
        auto result = client_->getFile(filenames[i], download_path, opts);
        ASSERT_DFS_OK(result);
    }
    
    download_timer.stop();
    
    double download_throughput = (num_files * file_size) / (1024.0 * 1024.0) / download_timer.getElapsedSeconds();
    std::cout << "Download throughput: " << download_throughput << " MB/s" << std::endl;
    
    // Record metrics
    metrics_->recordThroughput("upload", num_files * file_size, upload_timer.getElapsedSeconds());
    metrics_->recordThroughput("download", num_files * file_size, download_timer.getElapsedSeconds());
}

} // namespace test
} // namespace dfs