#include "test_framework.h"
#include "../src/common/crypto.h"

namespace dfs {
namespace test {

class CryptoTest : public DFSTestBase {
protected:
    void SetUp() override {
        DFSTestBase::SetUp();
        crypto_ = std::make_unique<Crypto>();
    }
    
    std::unique_ptr<Crypto> crypto_;
};

TEST_F(CryptoTest, KeyGeneration) {
    std::string password = "test_password_123";
    std::vector<uint8_t> salt = generateRandomBytes(16);
    
    auto key = crypto_->deriveKey(password, salt);
    ASSERT_EQ(key.size(), 32); // AES-256 key size
    
    // Same password and salt should generate same key
    auto key2 = crypto_->deriveKey(password, salt);
    ASSERT_EQ(key, key2);
    
    // Different password should generate different key
    auto key3 = crypto_->deriveKey("different_password", salt);
    ASSERT_NE(key, key3);
}

TEST_F(CryptoTest, EncryptionDecryption) {
    std::string password = "secure_password_456";
    std::string plaintext = "This is a test message for encryption.";
    
    auto encrypted = crypto_->encrypt(plaintext, password);
    ASSERT_TRUE(encrypted.has_value());
    ASSERT_NE(encrypted->encrypted_data, plaintext);
    
    auto decrypted = crypto_->decrypt(*encrypted, password);
    ASSERT_TRUE(decrypted.has_value());
    ASSERT_EQ(*decrypted, plaintext);
}

TEST_F(CryptoTest, WrongPasswordDecryption) {
    std::string password = "correct_password";
    std::string wrong_password = "wrong_password";
    std::string plaintext = "Secret message";
    
    auto encrypted = crypto_->encrypt(plaintext, password);
    ASSERT_TRUE(encrypted.has_value());
    
    auto decrypted = crypto_->decrypt(*encrypted, wrong_password);
    ASSERT_FALSE(decrypted.has_value());
}

TEST_F(CryptoTest, LargeDataEncryption) {
    std::string password = "large_data_password";
    std::string large_data = generateRandomData(1024 * 1024); // 1MB
    
    PerformanceTimer timer;
    timer.start();
    
    auto encrypted = crypto_->encrypt(large_data, password);
    
    timer.stop();
    std::cout << "Encryption of 1MB took: " << timer.getElapsedMilliseconds() << " ms" << std::endl;
    
    ASSERT_TRUE(encrypted.has_value());
    
    timer.start();
    auto decrypted = crypto_->decrypt(*encrypted, password);
    timer.stop();
    
    std::cout << "Decryption of 1MB took: " << timer.getElapsedMilliseconds() << " ms" << std::endl;
    
    ASSERT_TRUE(decrypted.has_value());
    ASSERT_EQ(*decrypted, large_data);
}

TEST_F(CryptoTest, HashingConsistency) {
    std::string data = "Test data for hashing";
    
    auto hash1 = Utils::calculateSHA256(data);
    auto hash2 = Utils::calculateSHA256(data);
    
    ASSERT_EQ(hash1, hash2);
    ASSERT_EQ(hash1.length(), 64); // SHA-256 hex string length
    
    // Different data should produce different hash
    auto hash3 = Utils::calculateSHA256(data + "x");
    ASSERT_NE(hash1, hash3);
}

PERFORMANCE_TEST(EncryptionPerformance, 100)
    std::string password = "performance_test_password";
    std::string data = generateRandomData(64 * 1024); // 64KB chunks
    
    auto encrypted = crypto_->encrypt(data, password);
    ASSERT_TRUE(encrypted.has_value());
    
    auto decrypted = crypto_->decrypt(*encrypted, password);
    ASSERT_TRUE(decrypted.has_value());
    ASSERT_EQ(*decrypted, data);
END_PERFORMANCE_TEST()

} // namespace test
} // namespace dfs