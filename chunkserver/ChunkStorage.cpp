#include "ChunkStorage.h"
#include <iostream>
#include <fstream>
#include <iterator>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;

ChunkStorage::ChunkStorage(const string& storage_path) : storage_path_(storage_path) {
    if (!fs::exists(storage_path_)) {
        fs::create_directories(storage_path_);
    }
}

bool ChunkStorage::saveChunk(const string& chunkId, const vector<char>& data){
    string chunkPath = storage_path_ + "/" + chunkId;
    ofstream chunkFile(chunkPath, ios::binary);

    if (!chunkFile) {
        cerr << "Error opening file for writing: " << chunkPath << endl;
        return false;
    }

    chunkFile.write(data.data(), data.size());
    return true;
}

vector<char> ChunkStorage::getChunk(const string& chunkId){
    string chunkPath = storage_path_ + "/" + chunkId;
    ifstream chunkFile(chunkPath, ios::binary);

    if (!chunkFile) {
        cerr << "Error opening file for reading: " << chunkPath << endl;
        return {};
    }

    vector<char> data((istreambuf_iterator<char>(chunkFile)), istreambuf_iterator<char>());
    return data;
}
