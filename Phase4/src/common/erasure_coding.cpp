#include "erasure_coding.h"
#include "utils.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace dfs {

// Static member initialization
uint8_t ErasureCoding::gf_log_table_[256];
uint8_t ErasureCoding::gf_exp_table_[256];
bool ErasureCoding::tables_initialized_ = false;

ErasureCoding::ErasureCoding(int dataBlocks, int parityBlocks) 
    : data_blocks_(dataBlocks), parity_blocks_(parityBlocks) {
    if (!tables_initialized_) {
        initializeTables();
        tables_initialized_ = true;
    }
}

ErasureCoding::~ErasureCoding() = default;

void ErasureCoding::initializeTables() {
    // Initialize Galois field tables for GF(2^8)
    // Primitive polynomial: x^8 + x^4 + x^3 + x^2 + 1 (0x11D)
    
    gf_exp_table_[0] = 1;
    gf_log_table_[0] = 0; // undefined, but we'll set it to 0
    
    int x = 1;
    for (int i = 1; i < 255; ++i) {
        x <<= 1;
        if (x & 0x100) {
            x ^= 0x11D; // primitive polynomial
        }
        gf_exp_table_[i] = x;
        gf_log_table_[x] = i;
    }
    
    gf_exp_table_[255] = gf_exp_table_[0];
    gf_log_table_[1] = 0;
}

uint8_t ErasureCoding::gf_add(uint8_t a, uint8_t b) {
    return a ^ b; // Addition in GF(2^8) is XOR
}

uint8_t ErasureCoding::gf_multiply(uint8_t a, uint8_t b) {
    if (a == 0 || b == 0) return 0;
    
    int log_a = gf_log_table_[a];
    int log_b = gf_log_table_[b];
    int log_result = (log_a + log_b) % 255;
    
    return gf_exp_table_[log_result];
}

uint8_t ErasureCoding::gf_divide(uint8_t a, uint8_t b) {
    if (a == 0) return 0;
    if (b == 0) throw std::runtime_error("Division by zero in GF(256)");
    
    int log_a = gf_log_table_[a];
    int log_b = gf_log_table_[b];
    int log_result = (log_a - log_b + 255) % 255;
    
    return gf_exp_table_[log_result];
}

uint8_t ErasureCoding::gf_power(uint8_t base, int exp) {
    if (base == 0) return 0;
    if (exp == 0) return 1;
    
    int log_base = gf_log_table_[base];
    int log_result = (log_base * exp) % 255;
    
    return gf_exp_table_[log_result];
}

std::vector<std::vector<uint8_t>> ErasureCoding::createVandermondeMatrix(int rows, int cols) {
    std::vector<std::vector<uint8_t>> matrix(rows, std::vector<uint8_t>(cols));
    
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            matrix[i][j] = gf_power(i + 1, j);
        }
    }
    
    return matrix;
}

std::vector<std::vector<uint8_t>> ErasureCoding::invertMatrix(const std::vector<std::vector<uint8_t>>& matrix) {
    int n = matrix.size();
    if (n == 0 || matrix[0].size() != n) {
        throw std::runtime_error("Matrix must be square for inversion");
    }
    
    // Create augmented matrix [A|I]
    std::vector<std::vector<uint8_t>> augmented(n, std::vector<uint8_t>(2 * n));
    
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            augmented[i][j] = matrix[i][j];
            augmented[i][j + n] = (i == j) ? 1 : 0;
        }
    }
    
    // Gaussian elimination
    for (int i = 0; i < n; ++i) {
        // Find pivot
        int pivot = i;
        for (int k = i + 1; k < n; ++k) {
            if (augmented[k][i] != 0) {
                pivot = k;
                break;
            }
        }
        
        if (augmented[pivot][i] == 0) {
            throw std::runtime_error("Matrix is not invertible");
        }
        
        // Swap rows if needed
        if (pivot != i) {
            std::swap(augmented[i], augmented[pivot]);
        }
        
        // Scale row to make diagonal element 1
        uint8_t diagonal = augmented[i][i];
        for (int j = 0; j < 2 * n; ++j) {
            if (augmented[i][j] != 0) {
                augmented[i][j] = gf_divide(augmented[i][j], diagonal);
            }
        }
        
        // Eliminate column
        for (int k = 0; k < n; ++k) {
            if (k != i && augmented[k][i] != 0) {
                uint8_t factor = augmented[k][i];
                for (int j = 0; j < 2 * n; ++j) {
                    augmented[k][j] = gf_add(augmented[k][j], 
                                           gf_multiply(factor, augmented[i][j]));
                }
            }
        }
    }
    
    // Extract inverse matrix
    std::vector<std::vector<uint8_t>> inverse(n, std::vector<uint8_t>(n));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            inverse[i][j] = augmented[i][j + n];
        }
    }
    
    return inverse;
}

std::vector<uint8_t> ErasureCoding::matrixVectorMultiply(const std::vector<std::vector<uint8_t>>& matrix,
                                                        const std::vector<uint8_t>& vector) {
    int rows = matrix.size();
    int cols = matrix[0].size();
    
    if (cols != static_cast<int>(vector.size())) {
        throw std::runtime_error("Matrix-vector dimension mismatch");
    }
    
    std::vector<uint8_t> result(rows, 0);
    
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            result[i] = gf_add(result[i], gf_multiply(matrix[i][j], vector[j]));
        }
    }
    
    return result;
}

std::vector<std::vector<uint8_t>> ErasureCoding::encode(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return {};
    }
    
    // Calculate block size
    int block_size = (data.size() + data_blocks_ - 1) / data_blocks_;
    
    // Pad data and split into blocks
    std::vector<uint8_t> padded_data = data;
    while (padded_data.size() < static_cast<size_t>(data_blocks_ * block_size)) {
        padded_data.push_back(0);
    }
    
    std::vector<std::vector<uint8_t>> data_blocks(data_blocks_, std::vector<uint8_t>(block_size));
    for (int i = 0; i < data_blocks_; ++i) {
        std::copy(padded_data.begin() + i * block_size,
                  padded_data.begin() + (i + 1) * block_size,
                  data_blocks[i].begin());
    }
    
    // Create encoding matrix (Vandermonde)
    auto encoding_matrix = createVandermondeMatrix(data_blocks_ + parity_blocks_, data_blocks_);
    
    // Generate all blocks (data + parity)
    std::vector<std::vector<uint8_t>> all_blocks(data_blocks_ + parity_blocks_);
    
    // Copy data blocks
    for (int i = 0; i < data_blocks_; ++i) {
        all_blocks[i] = data_blocks[i];
    }
    
    // Generate parity blocks
    for (int i = data_blocks_; i < data_blocks_ + parity_blocks_; ++i) {
        all_blocks[i].resize(block_size);
        for (int j = 0; j < block_size; ++j) {
            std::vector<uint8_t> column(data_blocks_);
            for (int k = 0; k < data_blocks_; ++k) {
                column[k] = data_blocks[k][j];
            }
            
            std::vector<uint8_t> encoded_column = matrixVectorMultiply(
                std::vector<std::vector<uint8_t>>(
                    encoding_matrix.begin() + i,
                    encoding_matrix.begin() + i + 1
                ), column);
            
            all_blocks[i][j] = encoded_column[0];
        }
    }
    
    return all_blocks;
}

std::vector<uint8_t> ErasureCoding::decode(const std::vector<std::vector<uint8_t>>& blocks,
                                          const std::vector<bool>& availability) {
    if (blocks.size() != static_cast<size_t>(data_blocks_ + parity_blocks_) ||
        availability.size() != static_cast<size_t>(data_blocks_ + parity_blocks_)) {
        throw std::runtime_error("Invalid block or availability vector size");
    }
    
    // Count available blocks
    int available_count = 0;
    std::vector<int> available_indices;
    for (int i = 0; i < static_cast<int>(availability.size()); ++i) {
        if (availability[i]) {
            available_count++;
            available_indices.push_back(i);
        }
    }
    
    if (available_count < data_blocks_) {
        throw std::runtime_error("Not enough blocks available for decoding");
    }
    
    // If we have all data blocks, just concatenate them
    bool have_all_data = true;
    for (int i = 0; i < data_blocks_; ++i) {
        if (!availability[i]) {
            have_all_data = false;
            break;
        }
    }
    
    if (have_all_data) {
        std::vector<uint8_t> result;
        for (int i = 0; i < data_blocks_; ++i) {
            result.insert(result.end(), blocks[i].begin(), blocks[i].end());
        }
        return result;
    }
    
    // Use first data_blocks_ available blocks for decoding
    available_indices.resize(data_blocks_);
    
    // Create decoding matrix
    auto encoding_matrix = createVandermondeMatrix(data_blocks_ + parity_blocks_, data_blocks_);
    std::vector<std::vector<uint8_t>> decoding_matrix(data_blocks_);
    
    for (int i = 0; i < data_blocks_; ++i) {
        decoding_matrix[i] = encoding_matrix[available_indices[i]];
    }
    
    auto inverse_matrix = invertMatrix(decoding_matrix);
    
    // Decode block by block
    int block_size = blocks[available_indices[0]].size();
    std::vector<std::vector<uint8_t>> decoded_blocks(data_blocks_, std::vector<uint8_t>(block_size));
    
    for (int j = 0; j < block_size; ++j) {
        std::vector<uint8_t> available_symbols(data_blocks_);
        for (int i = 0; i < data_blocks_; ++i) {
            available_symbols[i] = blocks[available_indices[i]][j];
        }
        
        std::vector<uint8_t> decoded_symbols = matrixVectorMultiply(inverse_matrix, available_symbols);
        
        for (int i = 0; i < data_blocks_; ++i) {
            decoded_blocks[i][j] = decoded_symbols[i];
        }
    }
    
    // Concatenate decoded blocks
    std::vector<uint8_t> result;
    for (int i = 0; i < data_blocks_; ++i) {
        result.insert(result.end(), decoded_blocks[i].begin(), decoded_blocks[i].end());
    }
    
    return result;
}

bool ErasureCoding::canDecode(const std::vector<bool>& availability) const {
    int available_count = 0;
    for (bool avail : availability) {
        if (avail) available_count++;
    }
    return available_count >= data_blocks_;
}

// ErasureCodedChunkManager implementation

ErasureCodedChunkManager::ErasureCodedChunkManager(int dataBlocks, int parityBlocks)
    : erasure_coder_(dataBlocks, parityBlocks) {
}

ErasureCodedChunkManager::CodeGroup ErasureCodedChunkManager::encodeChunk(
    const std::string& chunkId, const std::vector<uint8_t>& data) {
    
    CodeGroup group;
    group.group_id = chunkId + "_group";
    group.data_blocks = erasure_coder_.getDataBlocks();
    group.parity_blocks = erasure_coder_.getParityBlocks();
    group.original_size = data.size();
    
    // Encode the data
    auto encoded_blocks = erasure_coder_.encode(data);
    
    // Create coded chunks
    for (int i = 0; i < static_cast<int>(encoded_blocks.size()); ++i) {
        CodedChunk chunk;
        chunk.chunk_id = chunkId + "_block_" + std::to_string(i);
        chunk.block_index = i;
        chunk.is_parity = (i >= group.data_blocks);
        chunk.data = encoded_blocks[i];
        chunk.checksum = Utils::calculateSHA256(chunk.data);
        
        group.blocks.push_back(chunk);
    }
    
    return group;
}

std::vector<uint8_t> ErasureCodedChunkManager::decodeChunk(const CodeGroup& group) {
    if (group.blocks.empty()) {
        throw std::runtime_error("No blocks available for decoding");
    }
    
    // Sort blocks by index
    auto sorted_blocks = group.blocks;
    std::sort(sorted_blocks.begin(), sorted_blocks.end(),
              [](const CodedChunk& a, const CodedChunk& b) {
                  return a.block_index < b.block_index;
              });
    
    // Prepare blocks and availability for decoder
    int total_blocks = group.data_blocks + group.parity_blocks;
    std::vector<std::vector<uint8_t>> blocks(total_blocks);
    std::vector<bool> availability(total_blocks, false);
    
    for (const auto& chunk : sorted_blocks) {
        if (chunk.block_index >= 0 && chunk.block_index < total_blocks) {
            blocks[chunk.block_index] = chunk.data;
            availability[chunk.block_index] = true;
        }
    }
    
    // Decode
    auto decoded_data = erasure_coder_.decode(blocks, availability);
    
    // Remove padding
    return removePadding(decoded_data, group.original_size);
}

bool ErasureCodedChunkManager::canDecodeChunk(const CodeGroup& group) const {
    std::vector<bool> availability(group.data_blocks + group.parity_blocks, false);
    
    for (const auto& chunk : group.blocks) {
        if (chunk.block_index >= 0 && 
            chunk.block_index < static_cast<int>(availability.size())) {
            availability[chunk.block_index] = true;
        }
    }
    
    return erasure_coder_.canDecode(availability);
}

std::vector<ErasureCodedChunkManager::CodedChunk> ErasureCodedChunkManager::repairMissingBlocks(
    const CodeGroup& group, const std::vector<int>& missingIndices) {
    
    // First decode the original data
    auto original_data = decodeChunk(group);
    
    // Re-encode to get all blocks
    auto full_group = encodeChunk(group.group_id, original_data);
    
    // Return only the missing blocks
    std::vector<CodedChunk> repaired_blocks;
    for (int index : missingIndices) {
        if (index >= 0 && index < static_cast<int>(full_group.blocks.size())) {
            repaired_blocks.push_back(full_group.blocks[index]);
        }
    }
    
    return repaired_blocks;
}

std::vector<uint8_t> ErasureCodedChunkManager::padData(const std::vector<uint8_t>& data, int blockSize) {
    std::vector<uint8_t> padded = data;
    int data_blocks = erasure_coder_.getDataBlocks();
    size_t target_size = data_blocks * blockSize;
    
    while (padded.size() < target_size) {
        padded.push_back(0);
    }
    
    return padded;
}

std::vector<uint8_t> ErasureCodedChunkManager::removePadding(const std::vector<uint8_t>& data, 
                                                           int64_t originalSize) {
    if (originalSize < 0 || originalSize > static_cast<int64_t>(data.size())) {
        return data;
    }
    
    return std::vector<uint8_t>(data.begin(), data.begin() + originalSize);
}

} // namespace dfs