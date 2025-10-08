#include <iostream>
#include <string>
#include <filesystem>
#include <sstream>
#include <fstream>

#include "master/Chunker.h"
#include "master/Assembler.h"
#include "chunkserver/ChunkStorage.h"
#include "utils/picosha2.h"

using namespace std;

string getFileHash(const string& filepath) {
    ifstream file(filepath, ios::binary);
    if (!file) return "";
    stringstream buffer;
    buffer << file.rdbuf();
    return picosha2::hash256_hex_string(buffer.str());
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <path_to_file>" << endl;
        return 1;
    }

    const string originalFile = argv[1];

    if (!filesystem::exists(originalFile)) {
        cerr << "Error: Input file does not exist: " << originalFile << endl;
        return 1;
    }

    filesystem::path inputPath(originalFile);
    string reconstructedFile = inputPath.stem().string() + "_reconstructed" + inputPath.extension().string();

    cout << "Input file: " << originalFile << endl;
    cout << "Output file will be: " << reconstructedFile << endl;

    ChunkStorage chunkStorage;
    Chunker chunker;
    Assembler assembler;

    cout << "\n--- Master: Starting Chunker ---" << endl;
    vector<string> chunkIDs = chunker.chunkFile(originalFile, chunkStorage);
    if (chunkIDs.empty()) {
        cerr << "Chunking failed." << endl;
        return 1;
    }

    cout << "\n--- Master: Starting Assembler ---" << endl;
    if (!assembler.assembleFile(chunkIDs, reconstructedFile, chunkStorage)) {
        cerr << "Assembling failed." << endl;
        return 1;
    }

    cout << "\n--- Verifying Integrity ---" << endl;
    string originalHash = getFileHash(originalFile);
    string reconstructedHash = getFileHash(reconstructedFile);

    cout << "Original file hash:      " << originalHash << endl;
    cout << "Reconstructed file hash: " << reconstructedHash << endl;

    if (originalHash == reconstructedHash && !originalHash.empty()) {
        cout << "\nSUCCESS: Reconstructed file is identical to the original." << endl;
    } else {
        cout << "\nFAILURE: Files do not match!" << endl;
    }

    return 0;
}