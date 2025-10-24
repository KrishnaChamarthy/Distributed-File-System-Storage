#include "crypto.h"
#include "utils.h"
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <fstream>
#include <iostream>
#include <algorithm>

namespace dfs {

KeyManager& KeyManager::getInstance() {
    static KeyManager instance;
    return instance;
}

std::string KeyManager::generateKey() {
    return Crypto::generateRandomKey();
}

void KeyManager::storeKey(const std::string& keyId, const std::string& key) {
    std::lock_guard<std::mutex> lock(keys_mutex_);
    keys_[keyId] = key;
}

std::string KeyManager::getKey(const std::string& keyId) const {
    std::lock_guard<std::mutex> lock(keys_mutex_);
    auto it = keys_.find(keyId);
    return (it != keys_.end()) ? it->second : "";
}

bool KeyManager::hasKey(const std::string& keyId) const {
    std::lock_guard<std::mutex> lock(keys_mutex_);
    return keys_.find(keyId) != keys_.end();
}

bool KeyManager::saveKeysToFile(const std::string& masterPassword) {
    std::lock_guard<std::mutex> lock(keys_mutex_);
    
    // Serialize keys to JSON
    std::string json = "{\n";
    bool first = true;
    for (const auto& pair : keys_) {
        if (!first) json += ",\n";
        json += "  \"" + pair.first + "\": \"" + pair.second + "\"";
        first = false;
    }
    json += "\n}";
    
    // Encrypt the JSON with master password
    std::string salt = Crypto::generateRandomSalt();
    std::string derivedKey = Crypto::deriveKeyFromPassword(masterPassword, salt);
    
    std::vector<uint8_t> plaintext(json.begin(), json.end());
    std::vector<uint8_t> encrypted = Crypto::encrypt(plaintext, derivedKey);
    
    // Save salt + encrypted data
    std::ofstream file("keys.dat", std::ios::binary);
    if (!file.is_open()) return false;
    
    file.write(salt.c_str(), salt.size());
    file.write(reinterpret_cast<const char*>(encrypted.data()), encrypted.size());
    
    return file.good();
}

bool KeyManager::loadKeysFromFile(const std::string& masterPassword) {
    std::ifstream file("keys.dat", std::ios::binary);
    if (!file.is_open()) return false;
    
    // Read salt
    std::string salt(Crypto::SALT_SIZE, '\0');
    file.read(&salt[0], Crypto::SALT_SIZE);
    
    // Read encrypted data
    file.seekg(0, std::ios::end);
    size_t totalSize = file.tellg();
    size_t encryptedSize = totalSize - Crypto::SALT_SIZE;
    file.seekg(Crypto::SALT_SIZE);
    
    std::vector<uint8_t> encrypted(encryptedSize);
    file.read(reinterpret_cast<char*>(encrypted.data()), encryptedSize);
    
    // Decrypt
    std::string derivedKey = Crypto::deriveKeyFromPassword(masterPassword, salt);
    std::vector<uint8_t> decrypted = Crypto::decrypt(encrypted, derivedKey);
    
    if (decrypted.empty()) return false;
    
    // Parse JSON (simple parser for key-value pairs)
    std::string json(decrypted.begin(), decrypted.end());
    std::lock_guard<std::mutex> lock(keys_mutex_);
    keys_.clear();
    
    // Simple JSON parsing
    size_t pos = 0;
    while ((pos = json.find("\"", pos)) != std::string::npos) {
        size_t keyStart = pos + 1;
        size_t keyEnd = json.find("\"", keyStart);
        if (keyEnd == std::string::npos) break;
        
        std::string key = json.substr(keyStart, keyEnd - keyStart);
        
        pos = json.find("\"", keyEnd + 1);
        if (pos == std::string::npos) break;
        
        size_t valueStart = pos + 1;
        size_t valueEnd = json.find("\"", valueStart);
        if (valueEnd == std::string::npos) break;
        
        std::string value = json.substr(valueStart, valueEnd - valueStart);
        keys_[key] = value;
        
        pos = valueEnd + 1;
    }
    
    return true;
}

std::vector<uint8_t> Crypto::encrypt(const std::vector<uint8_t>& plaintext, const std::string& key) {
    if (key.size() != KEY_SIZE) {
        Utils::logError("Invalid key size for encryption");
        return {};
    }
    
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};
    
    // Generate random IV
    std::vector<uint8_t> iv = generateRandomIV();
    
    // Initialize encryption
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, 
                          reinterpret_cast<const unsigned char*>(key.c_str()), 
                          iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    
    // Encrypt data
    std::vector<uint8_t> ciphertext(plaintext.size() + TAG_SIZE);
    int len;
    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, 
                         plaintext.data(), plaintext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    
    int ciphertext_len = len;
    
    // Finalize encryption
    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    ciphertext_len += len;
    
    // Get authentication tag
    std::vector<uint8_t> tag(TAG_SIZE);
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_SIZE, tag.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    
    EVP_CIPHER_CTX_free(ctx);
    
    // Format: IV + ciphertext + tag
    std::vector<uint8_t> result;
    result.reserve(IV_SIZE + ciphertext_len + TAG_SIZE);
    result.insert(result.end(), iv.begin(), iv.end());
    result.insert(result.end(), ciphertext.begin(), ciphertext.begin() + ciphertext_len);
    result.insert(result.end(), tag.begin(), tag.end());
    
    return result;
}

std::vector<uint8_t> Crypto::decrypt(const std::vector<uint8_t>& ciphertext, const std::string& key) {
    if (key.size() != KEY_SIZE || ciphertext.size() < IV_SIZE + TAG_SIZE) {
        Utils::logError("Invalid key size or ciphertext size for decryption");
        return {};
    }
    
    // Extract components
    std::vector<uint8_t> iv(ciphertext.begin(), ciphertext.begin() + IV_SIZE);
    std::vector<uint8_t> encrypted(ciphertext.begin() + IV_SIZE, 
                                  ciphertext.end() - TAG_SIZE);
    std::vector<uint8_t> tag(ciphertext.end() - TAG_SIZE, ciphertext.end());
    
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};
    
    // Initialize decryption
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr,
                          reinterpret_cast<const unsigned char*>(key.c_str()),
                          iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    
    // Set expected tag
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, TAG_SIZE, tag.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    
    // Decrypt data
    std::vector<uint8_t> plaintext(encrypted.size());
    int len;
    if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, 
                         encrypted.data(), encrypted.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    
    int plaintext_len = len;
    
    // Finalize decryption
    int ret = EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len);
    EVP_CIPHER_CTX_free(ctx);
    
    if (ret <= 0) {
        Utils::logError("Decryption failed - authentication tag verification failed");
        return {};
    }
    
    plaintext_len += len;
    plaintext.resize(plaintext_len);
    
    return plaintext;
}

std::string Crypto::generateRandomKey() {
    std::vector<uint8_t> key(KEY_SIZE);
    if (RAND_bytes(key.data(), KEY_SIZE) != 1) {
        Utils::logError("Failed to generate random key");
        return "";
    }
    
    // Convert to hex string
    std::stringstream ss;
    for (auto byte : key) {
        ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}

std::vector<uint8_t> Crypto::generateRandomIV() {
    std::vector<uint8_t> iv(IV_SIZE);
    if (RAND_bytes(iv.data(), IV_SIZE) != 1) {
        Utils::logError("Failed to generate random IV");
    }
    return iv;
}

std::string Crypto::deriveKeyFromPassword(const std::string& password, const std::string& salt) {
    std::vector<uint8_t> key(KEY_SIZE);
    
    if (PKCS5_PBKDF2_HMAC(password.c_str(), password.length(),
                          reinterpret_cast<const unsigned char*>(salt.c_str()), salt.length(),
                          10000, // iterations
                          EVP_sha256(),
                          KEY_SIZE, key.data()) != 1) {
        Utils::logError("Failed to derive key from password");
        return "";
    }
    
    // Convert to hex string
    std::stringstream ss;
    for (auto byte : key) {
        ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}

std::string Crypto::generateRandomSalt() {
    std::vector<uint8_t> salt(SALT_SIZE);
    if (RAND_bytes(salt.data(), SALT_SIZE) != 1) {
        Utils::logError("Failed to generate random salt");
        return "";
    }
    return std::string(salt.begin(), salt.end());
}

std::vector<uint8_t> Crypto::encryptChunk(const std::vector<uint8_t>& chunkData, const std::string& keyId) {
    KeyManager& keyManager = KeyManager::getInstance();
    std::string key = keyManager.getKey(keyId);
    
    if (key.empty()) {
        Utils::logError("Encryption key not found: " + keyId);
        return {};
    }
    
    return encrypt(chunkData, key);
}

std::vector<uint8_t> Crypto::decryptChunk(const std::vector<uint8_t>& encryptedData, const std::string& keyId) {
    KeyManager& keyManager = KeyManager::getInstance();
    std::string key = keyManager.getKey(keyId);
    
    if (key.empty()) {
        Utils::logError("Decryption key not found: " + keyId);
        return {};
    }
    
    return decrypt(encryptedData, key);
}

std::string Crypto::signData(const std::vector<uint8_t>& data, const std::string& privateKey) {
    // Simplified signature implementation using HMAC-SHA256
    // In production, use RSA or ECDSA signatures
    
    unsigned char signature[EVP_MAX_MD_SIZE];
    unsigned int signatureLen;
    
    HMAC(EVP_sha256(), 
         privateKey.c_str(), privateKey.length(),
         data.data(), data.size(),
         signature, &signatureLen);
    
    // Convert to hex string
    std::stringstream ss;
    for (unsigned int i = 0; i < signatureLen; ++i) {
        ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(signature[i]);
    }
    return ss.str();
}

bool Crypto::verifySignature(const std::vector<uint8_t>& data, const std::string& signature, const std::string& publicKey) {
    std::string computedSignature = signData(data, publicKey);
    return computedSignature == signature;
}

} // namespace dfs