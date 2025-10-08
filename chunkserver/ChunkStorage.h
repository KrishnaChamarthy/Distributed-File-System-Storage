#ifndef CHUNK_STORAGE_H
#define CHUNK_STORAGE_H

#include <string>
#include <vector>
using namespace std;

class ChunkStorage {
    public: 
        ChunkStorage(const string& storage_path = "./data/chunks");

        bool saveChunk(const string& chunkId, const vector<char>& data);

        vector<char> getChunk(const string& chunkId);

    private:
        string storage_path_;
};

#endif