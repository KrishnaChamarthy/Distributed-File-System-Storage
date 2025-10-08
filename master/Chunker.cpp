#include "Chunker.h" 
#include <iostream>
#include <fstream>
#include <iomanip>
#include "../utils/picosha2.h"

using namespace std;

string computeSHA256(const vector<char>& data){
    return picosha2::hash256_hex_string(data);
}

vector<string> Chunker::chunkFile(const string& inputFilePath, ChunkStorage& storage){
    ifstream inputFile(inputFilePath, ios::binary);

    if (!inputFile) {
        cerr << "Error: Cannot open input file: " << inputFilePath << endl;
        return {};
    }

    inputFile.seekg(0, ios::end);
    streampos fileSize = inputFile.tellg();
    inputFile.seekg(0, ios::beg);
    
    cout << "File size: " << fileSize << " bytes" << endl;

    vector<string> chunkIds;
    vector<char> buffer(CHUNK_SIZE);
    streampos totalBytesProcessed = 0;

    while(!inputFile.eof()){
        inputFile.read(buffer.data(), CHUNK_SIZE);
        streamsize bytesRead = inputFile.gcount();

        if (bytesRead == 0) break;

        buffer.resize(bytesRead);
        string chunkId = computeSHA256(buffer);
        chunkIds.push_back(chunkId);

        if (!storage.saveChunk(chunkId, buffer)) {
            cerr << "Error: Failed to save chunk with ID: " << chunkId << endl;
            return {};
        }

        totalBytesProcessed += bytesRead;
        double percentage = (static_cast<double>(totalBytesProcessed) / static_cast<double>(fileSize)) * 100.0;
        
        cout << "Chunk " << chunkIds.size() << " processed (" 
             << totalBytesProcessed << "/" << fileSize << " bytes) - "
             << fixed << setprecision(1) << percentage << "% complete" << endl;

        buffer.resize(CHUNK_SIZE);
    }

    inputFile.close();
    cout << "\nFile successfully chunked into " << chunkIds.size() << " chunks (100.0% complete)." << endl;
    return chunkIds;
}



