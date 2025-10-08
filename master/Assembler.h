#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <string>
#include <vector>
#include "../chunkserver/ChunkStorage.h"

using namespace std;

class Assembler{
    public:
        bool assembleFile(const vector<string>& chunkIds, const string& outputFilePath, ChunkStorage& storage);

};

#endif