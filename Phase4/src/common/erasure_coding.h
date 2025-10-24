#pragma once

#include <vector>
#include <string>
#include <memory>

namespace dfs {

// Reed-Solomon erasure coding implementation
class ErasureCoding {
public:
    ErasureCoding(int dataBlocks, int parityBlocks);
    ~ErasureCoding();
    
    // Encode data into data blocks + parity blocks
    std::vector<std::vector<uint8_t>> encode(const std::vector<uint8_t>& data);
    
    // Decode data from available blocks (requires at least dataBlocks)
    std::vector<uint8_t> decode(const std::vector<std::vector<uint8_t>>& blocks,
                               const std::vector<bool>& availability);
    
    // Check if we have enough blocks to decode
    bool canDecode(const std::vector<bool>& availability) const;
    
    // Get total number of blocks (data + parity)
    int getTotalBlocks() const { return data_blocks_ + parity_blocks_; }
    
    // Get number of data blocks
    int getDataBlocks() const { return data_blocks_; }
    
    // Get number of parity blocks
    int getParityBlocks() const { return parity_blocks_; }
    
private:
    int data_blocks_;
    int parity_blocks_;
    
    // Galois field operations
    uint8_t gf_add(uint8_t a, uint8_t b);
    uint8_t gf_multiply(uint8_t a, uint8_t b);
    uint8_t gf_divide(uint8_t a, uint8_t b);
    uint8_t gf_power(uint8_t base, int exp);
    
    // Matrix operations in GF(256)
    std::vector<std::vector<uint8_t>> createVandermondeMatrix(int rows, int cols);
    std::vector<std::vector<uint8_t>> invertMatrix(const std::vector<std::vector<uint8_t>>& matrix);
    std::vector<uint8_t> matrixVectorMultiply(const std::vector<std::vector<uint8_t>>& matrix,
                                             const std::vector<uint8_t>& vector);
    
    // Precomputed tables for Galois field operations
    static uint8_t gf_log_table_[256];
    static uint8_t gf_exp_table_[256];
    static bool tables_initialized_;
    
    void initializeTables();
};

// Chunk manager with erasure coding support
class ErasureCodedChunkManager {
public:
    struct CodedChunk {
        std::string chunk_id;
        int block_index;
        bool is_parity;
        std::vector<uint8_t> data;
        std::string checksum;
    };
    
    struct CodeGroup {
        std::string group_id;
        std::vector<CodedChunk> blocks;
        int data_blocks;
        int parity_blocks;
        int64_t original_size;
    };
    
    ErasureCodedChunkManager(int dataBlocks = 4, int parityBlocks = 2);
    
    // Encode a chunk into multiple coded blocks
    CodeGroup encodeChunk(const std::string& chunkId, const std::vector<uint8_t>& data);
    
    // Decode a chunk from available coded blocks
    std::vector<uint8_t> decodeChunk(const CodeGroup& group);
    
    // Check if we can decode with available blocks
    bool canDecodeChunk(const CodeGroup& group) const;
    
    // Get minimum blocks needed for decoding
    int getMinimumBlocksNeeded() const { return erasure_coder_.getDataBlocks(); }
    
    // Repair missing blocks
    std::vector<CodedChunk> repairMissingBlocks(const CodeGroup& group,
                                               const std::vector<int>& missingIndices);
    
private:
    ErasureCoding erasure_coder_;
    
    // Pad data to make it divisible by data_blocks
    std::vector<uint8_t> padData(const std::vector<uint8_t>& data, int blockSize);
    
    // Remove padding from decoded data
    std::vector<uint8_t> removePadding(const std::vector<uint8_t>& data, int64_t originalSize);
};

} // namespace dfs