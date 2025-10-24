#include "utils.h"
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <random>
#include <algorithm>
#include <ctime>
#include <arpa/inet.h>
#include <sys/socket.h>

namespace dfs {

std::mt19937 Utils::rng_(std::chrono::steady_clock::now().time_since_epoch().count());

std::string Utils::generateChunkId() {
    return "chunk_" + getRandomString(32);
}

std::string Utils::generateFileId() {
    return "file_" + getRandomString(32);
}

std::string Utils::generateServerId() {
    return "server_" + getRandomString(16);
}

std::string Utils::calculateSHA256(const std::vector<uint8_t>& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(data.data(), data.size(), hash);
    
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return ss.str();
}

std::string Utils::calculateSHA256(const std::string& data) {
    std::vector<uint8_t> bytes(data.begin(), data.end());
    return calculateSHA256(bytes);
}

bool Utils::fileExists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

bool Utils::createDirectory(const std::string& path) {
    return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
}

std::vector<uint8_t> Utils::readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return {};
    }
    
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

bool Utils::writeFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    return file.good();
}

int64_t Utils::getFileSize(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return st.st_size;
    }
    return -1;
}

bool Utils::deleteFile(const std::string& path) {
    return unlink(path.c_str()) == 0;
}

std::vector<std::string> Utils::splitString(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::string Utils::joinStrings(const std::vector<std::string>& strings, const std::string& delimiter) {
    if (strings.empty()) return "";
    
    std::string result = strings[0];
    for (size_t i = 1; i < strings.size(); ++i) {
        result += delimiter + strings[i];
    }
    return result;
}

bool Utils::isValidIPAddress(const std::string& ip) {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr)) != 0;
}

bool Utils::isPortOpen(const std::string& host, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
        close(sock);
        return false;
    }
    
    bool result = connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0;
    close(sock);
    return result;
}

int64_t Utils::getCurrentTimestamp() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

std::string Utils::timestampToString(int64_t timestamp) {
    auto time_point = std::chrono::system_clock::from_time_t(timestamp / 1000);
    std::time_t time = std::chrono::system_clock::to_time_t(time_point);
    return std::ctime(&time);
}

int Utils::getRandomInt(int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(rng_);
}

std::string Utils::getRandomString(int length) {
    const std::string chars = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::string result;
    result.reserve(length);
    
    for (int i = 0; i < length; ++i) {
        result += chars[getRandomInt(0, chars.size() - 1)];
    }
    return result;
}

std::vector<int> Utils::getRandomPermutation(int size) {
    std::vector<int> perm(size);
    std::iota(perm.begin(), perm.end(), 0);
    std::shuffle(perm.begin(), perm.end(), rng_);
    return perm;
}

void Utils::logInfo(const std::string& message) {
    std::cout << "[INFO] " << timestampToString(getCurrentTimestamp()) << " " << message << std::endl;
}

void Utils::logWarning(const std::string& message) {
    std::cout << "[WARN] " << timestampToString(getCurrentTimestamp()) << " " << message << std::endl;
}

void Utils::logError(const std::string& message) {
    std::cerr << "[ERROR] " << timestampToString(getCurrentTimestamp()) << " " << message << std::endl;
}

void Utils::logDebug(const std::string& message) {
    #ifdef DEBUG
    std::cout << "[DEBUG] " << timestampToString(getCurrentTimestamp()) << " " << message << std::endl;
    #endif
}

Config& Config::getInstance() {
    static Config instance;
    return instance;
}

bool Config::loadFromFile(const std::string& configFile) {
    // Simple key-value configuration parser
    std::ifstream file(configFile);
    if (!file.is_open()) {
        Utils::logWarning("Could not open config file: " + configFile);
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        
        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        if (key == "replication_factor") {
            replication_factor_ = std::stoi(value);
        } else if (key == "chunk_size") {
            chunk_size_ = std::stoi(value);
        } else if (key == "data_directory") {
            data_directory_ = value;
        } else if (key == "master_address") {
            master_address_ = value;
        } else if (key == "master_port") {
            master_port_ = std::stoi(value);
        } else if (key == "encryption_enabled") {
            encryption_enabled_ = (value == "true" || value == "1");
        } else if (key == "erasure_coding_enabled") {
            erasure_coding_enabled_ = (value == "true" || value == "1");
        }
    }
    
    return true;
}

Metrics& Metrics::getInstance() {
    static Metrics instance;
    return instance;
}

void Metrics::recordUploadTime(int64_t milliseconds) {
    std::lock_guard<std::mutex> lock(timing_mutex_);
    upload_times_.push_back(milliseconds);
}

void Metrics::recordDownloadTime(int64_t milliseconds) {
    std::lock_guard<std::mutex> lock(timing_mutex_);
    download_times_.push_back(milliseconds);
}

void Metrics::recordReplicationTime(int64_t milliseconds) {
    std::lock_guard<std::mutex> lock(timing_mutex_);
    replication_times_.push_back(milliseconds);
}

double Metrics::getAverageUploadTime() const {
    std::lock_guard<std::mutex> lock(timing_mutex_);
    if (upload_times_.empty()) return 0.0;
    
    int64_t sum = 0;
    for (auto time : upload_times_) {
        sum += time;
    }
    return static_cast<double>(sum) / upload_times_.size();
}

double Metrics::getAverageDownloadTime() const {
    std::lock_guard<std::mutex> lock(timing_mutex_);
    if (download_times_.empty()) return 0.0;
    
    int64_t sum = 0;
    for (auto time : download_times_) {
        sum += time;
    }
    return static_cast<double>(sum) / download_times_.size();
}

std::string Metrics::toJSON() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"chunks_written\": " << chunks_written_ << ",\n";
    ss << "  \"chunks_read\": " << chunks_read_ << ",\n";
    ss << "  \"files_uploaded\": " << files_uploaded_ << ",\n";
    ss << "  \"files_downloaded\": " << files_downloaded_ << ",\n";
    ss << "  \"replication_tasks\": " << replication_tasks_ << ",\n";
    ss << "  \"average_upload_time_ms\": " << getAverageUploadTime() << ",\n";
    ss << "  \"average_download_time_ms\": " << getAverageDownloadTime() << ",\n";
    ss << "  \"disk_usage_bytes\": " << disk_usage_ << ",\n";
    ss << "  \"memory_usage_percent\": " << memory_usage_ << ",\n";
    ss << "  \"cpu_usage_percent\": " << cpu_usage_ << "\n";
    ss << "}";
    return ss.str();
}

} // namespace dfs