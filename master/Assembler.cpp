#include "Assembler.h"
#include <iostream>
#include <fstream>
#include <iomanip>

using namespace std;

bool Assembler::assembleFile(const vector<string>& chunkIds, const string& outputFilePath, ChunkStorage& storage){
    ofstream outputFile(outputFilePath, ios::binary);

    if (!outputFile) {
        cerr << "Error: Cannot create output file: " << outputFilePath << endl;
        return false;
    }

    cout << "Total chunks to assemble: " << chunkIds.size() << endl;
    
    size_t chunksProcessed = 0;
    size_t totalBytesWritten = 0;

    for (const auto& chunkId: chunkIds){
        vector<char> chunkData = storage.getChunk(chunkId);

        if (chunkData.empty()){
            cerr << "Error: Failed to retrieve chunk: " << chunkId << endl;
            return false;
        }

        outputFile.write(chunkData.data(), chunkData.size());
        totalBytesWritten += chunkData.size();
        chunksProcessed++;
        
        double percentage = (static_cast<double>(chunksProcessed) / static_cast<double>(chunkIds.size())) * 100.0;
        
        cout << "Chunk " << chunksProcessed << "/" << chunkIds.size() << " assembled (" 
             << totalBytesWritten << " bytes written) - "
             << fixed << setprecision(1) << percentage << "% complete" << endl;
    }

    outputFile.close();
    cout << "\nFile successfully assembled from " << chunkIds.size() << " chunks to: " << outputFilePath << " (100.0% complete)" << endl;
    cout << "Total bytes written: " << totalBytesWritten << endl;
    return true;
}