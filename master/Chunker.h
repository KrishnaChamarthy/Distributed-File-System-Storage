#ifndef CHUNKER_H
#define CHUNKER_H

#include <string>
#include <vector>
#include "../chunkserver/ChunkStorage.h"

using namespace std;

class Chunker {
    public:
        vector<string> chunkFile(const string& inputFilePath, ChunkStorage& storage);
    private:
        static const size_t CHUNK_SIZE = 4 * 1024 * 1024;
};

#endif