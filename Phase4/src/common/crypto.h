#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace dfs {

// Encryption key management
class KeyManager {
public:
    static KeyManager& getInstance();
    
    // Generate new encryption key
    std::string generateKey();
    
    // Store key with ID
    void storeKey(const std::string& keyId, const std::string& key);
    
    // Retrieve key by ID
    std::string getKey(const std::string& keyId) const;
    
    // Check if key exists
    bool hasKey(const std::string& keyId) const;
    
    // Save keys to encrypted file
    bool saveKeysToFile(const std::string& masterPassword);
    
    // Load keys from encrypted file
    bool loadKeysFromFile(const std::string& masterPassword);
    
private:
    KeyManager() = default;
    std::map<std::string, std::string> keys_;
    mutable std::mutex keys_mutex_;
};

// AES encryption wrapper
class Crypto {
public:
    // AES-256-GCM encryption
    static std::vector<uint8_t> encrypt(const std::vector<uint8_t>& plaintext, 
                                       const std::string& key);
    
    // AES-256-GCM decryption
    static std::vector<uint8_t> decrypt(const std::vector<uint8_t>& ciphertext, 
                                       const std::string& key);
    
    // Generate random key (32 bytes for AES-256)
    static std::string generateRandomKey();
    
    // Generate random IV (12 bytes for GCM)
    static std::vector<uint8_t> generateRandomIV();
    
    // Key derivation from password (PBKDF2)
    static std::string deriveKeyFromPassword(const std::string& password, 
                                           const std::string& salt);
    
    // Generate random salt
    static std::string generateRandomSalt();
    
    // Encrypt chunk data
    static std::vector<uint8_t> encryptChunk(const std::vector<uint8_t>& chunkData, 
                                           const std::string& keyId);
    
    // Decrypt chunk data
    static std::vector<uint8_t> decryptChunk(const std::vector<uint8_t>& encryptedData, 
                                           const std::string& keyId);
    
    // Digital signatures for integrity
    static std::string signData(const std::vector<uint8_t>& data, 
                               const std::string& privateKey);
    
    static bool verifySignature(const std::vector<uint8_t>& data, 
                               const std::string& signature, 
                               const std::string& publicKey);
    
private:
    static const int KEY_SIZE = 32;  // 256 bits
    static const int IV_SIZE = 12;   // 96 bits for GCM
    static const int TAG_SIZE = 16;  // 128 bits for GCM tag
    static const int SALT_SIZE = 16; // 128 bits
};

} // namespace dfs