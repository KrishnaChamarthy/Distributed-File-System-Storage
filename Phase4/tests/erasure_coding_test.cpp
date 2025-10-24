#include "test_framework.h"
#include "../src/common/erasure_coding.h"

namespace dfs {
namespace test {

class ErasureCodingTest : public DFSTestBase {
protected:
    void SetUp() override {
        DFSTestBase::SetUp();
        erasure_coding_ = std::make_unique<ErasureCoding>(4, 2); // 4 data + 2 parity
    }
    
    std::unique_ptr<ErasureCoding> erasure_coding_;
};

TEST_F(ErasureCodingTest, BasicEncoding) {
    std::vector<uint8_t> data = TestDataGenerator::generateSequential(1024);
    
    auto encoded = erasure_coding_->encode(data);
    ASSERT_TRUE(encoded.has_value());
    ASSERT_EQ(encoded->size(), 6); // 4 data + 2 parity blocks
    
    // Verify that we have the correct number of blocks
    for (const auto& block : *encoded) {
        ASSERT_EQ(block.size(), 256); // 1024 / 4 = 256 bytes per block
    }
}

TEST_F(ErasureCodingTest, BasicDecoding) {
    std::vector<uint8_t> original_data = TestDataGenerator::generateRandom(2048);
    
    auto encoded = erasure_coding_->encode(original_data);
    ASSERT_TRUE(encoded.has_value());
    
    auto decoded = erasure_coding_->decode(*encoded, original_data.size());
    ASSERT_TRUE(decoded.has_value());
    ASSERT_EQ(*decoded, original_data);
}

TEST_F(ErasureCodingTest, RecoveryFromSingleFailure) {
    std::vector<uint8_t> original_data = TestDataGenerator::generatePattern(1024, 0xAB);
    
    auto encoded = erasure_coding_->encode(original_data);
    ASSERT_TRUE(encoded.has_value());
    
    // Simulate failure of first data block
    auto corrupted_blocks = *encoded;
    corrupted_blocks[0].clear(); // Mark as missing
    
    auto recovered = erasure_coding_->decode(corrupted_blocks, original_data.size());
    ASSERT_TRUE(recovered.has_value());
    ASSERT_EQ(*recovered, original_data);
}

TEST_F(ErasureCodingTest, RecoveryFromDoubleFailure) {
    std::vector<uint8_t> original_data = TestDataGenerator::generateRandom(4096);
    
    auto encoded = erasure_coding_->encode(original_data);
    ASSERT_TRUE(encoded.has_value());
    
    // Simulate failure of two blocks (one data, one parity)
    auto corrupted_blocks = *encoded;
    corrupted_blocks[1].clear(); // Data block
    corrupted_blocks[4].clear(); // Parity block
    
    auto recovered = erasure_coding_->decode(corrupted_blocks, original_data.size());
    ASSERT_TRUE(recovered.has_value());
    ASSERT_EQ(*recovered, original_data);
}

TEST_F(ErasureCodingTest, CannotRecoverFromTripleFailure) {
    std::vector<uint8_t> original_data = TestDataGenerator::generateRandom(1024);
    
    auto encoded = erasure_coding_->encode(original_data);
    ASSERT_TRUE(encoded.has_value());
    
    // Simulate failure of three blocks (exceeds recovery capability)
    auto corrupted_blocks = *encoded;
    corrupted_blocks[0].clear();
    corrupted_blocks[1].clear();
    corrupted_blocks[2].clear();
    
    auto recovered = erasure_coding_->decode(corrupted_blocks, original_data.size());
    ASSERT_FALSE(recovered.has_value());
}

TEST_F(ErasureCodingTest, VariousDataSizes) {
    std::vector<size_t> test_sizes = {100, 1000, 10000, 100000, 1000000};
    
    for (size_t size : test_sizes) {
        std::vector<uint8_t> data = TestDataGenerator::generateRandom(size);
        
        auto encoded = erasure_coding_->encode(data);
        ASSERT_TRUE(encoded.has_value()) << "Failed to encode data of size " << size;
        
        auto decoded = erasure_coding_->decode(*encoded, size);
        ASSERT_TRUE(decoded.has_value()) << "Failed to decode data of size " << size;
        ASSERT_EQ(*decoded, data) << "Decoded data doesn't match original for size " << size;
    }
}

TEST_F(ErasureCodingTest, ZeroData) {
    std::vector<uint8_t> zero_data = TestDataGenerator::generateZeros(1024);
    
    auto encoded = erasure_coding_->encode(zero_data);
    ASSERT_TRUE(encoded.has_value());
    
    auto decoded = erasure_coding_->decode(*encoded, zero_data.size());
    ASSERT_TRUE(decoded.has_value());
    ASSERT_EQ(*decoded, zero_data);
}

TEST_F(ErasureCodingTest, CompressibleData) {
    std::vector<uint8_t> compressible_data = TestDataGenerator::generateCompressible(8192);
    
    auto encoded = erasure_coding_->encode(compressible_data);
    ASSERT_TRUE(encoded.has_value());
    
    // Test recovery with missing block
    auto corrupted_blocks = *encoded;
    corrupted_blocks[2].clear();
    
    auto recovered = erasure_coding_->decode(corrupted_blocks, compressible_data.size());
    ASSERT_TRUE(recovered.has_value());
    ASSERT_EQ(*recovered, compressible_data);
}

PERFORMANCE_TEST(ErasureCodingPerformance, 50)
    std::vector<uint8_t> data = TestDataGenerator::generateRandom(64 * 1024); // 64KB
    
    auto encoded = erasure_coding_->encode(data);
    ASSERT_TRUE(encoded.has_value());
    
    auto decoded = erasure_coding_->decode(*encoded, data.size());
    ASSERT_TRUE(decoded.has_value());
    ASSERT_EQ(*decoded, data);
END_PERFORMANCE_TEST()

class ErasureCodedChunkManagerTest : public DFSTestBase {
protected:
    void SetUp() override {
        DFSTestBase::SetUp();
        manager_ = std::make_unique<ErasureCodedChunkManager>(4, 2);
    }
    
    std::unique_ptr<ErasureCodedChunkManager> manager_;
};

TEST_F(ErasureCodedChunkManagerTest, ChunkCreationAndRetrieval) {
    std::vector<uint8_t> data = TestDataGenerator::generateRandom(256 * 1024); // 256KB
    std::string chunk_id = "test_chunk_001";
    
    auto chunks = manager_->createChunks(chunk_id, data);
    ASSERT_TRUE(chunks.has_value());
    ASSERT_EQ(chunks->size(), 6); // 4 data + 2 parity
    
    // Test retrieval with all chunks available
    auto retrieved = manager_->reconstructData(chunk_id, *chunks, data.size());
    ASSERT_TRUE(retrieved.has_value());
    ASSERT_EQ(*retrieved, data);
}

TEST_F(ErasureCodedChunkManagerTest, FaultTolerance) {
    std::vector<uint8_t> data = TestDataGenerator::generateRandom(128 * 1024);
    std::string chunk_id = "fault_test_chunk";
    
    auto chunks = manager_->createChunks(chunk_id, data);
    ASSERT_TRUE(chunks.has_value());
    
    // Remove some chunks to simulate failures
    auto partial_chunks = *chunks;
    partial_chunks.erase(partial_chunks.begin() + 1); // Remove one data chunk
    partial_chunks.erase(partial_chunks.begin() + 4); // Remove one parity chunk (adjusted index)
    
    auto retrieved = manager_->reconstructData(chunk_id, partial_chunks, data.size());
    ASSERT_TRUE(retrieved.has_value());
    ASSERT_EQ(*retrieved, data);
}

} // namespace test
} // namespace dfs