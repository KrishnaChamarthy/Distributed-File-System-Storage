#include "Uploader.h"
#include "../utils/picosha2.h"
#include <iostream>
#include <fstream>
#include <grpcpp/grpcpp.h>

using namespace std;

using chunkservice::UploadChunkRequest;
using chunkservice::UploadChunkResponse;

Uploader::Uploader(shared_ptr<chunkservice::ChunkService::StubInterface> stub)
    : stub_(stub) {}

string computeSHA256(const vector<char>& data) {
    return picosha2::hash256_hex_string(data);
}

vector<string> Uploader::chunkAndUploadFile(const string& filePath) {
    ifstream inputFile(filePath, ios::binary);
    if (!inputFile) {
        cerr << "Error: Cannot open input file: " << filePath << endl;
        return {};
    }

    vector<string> chunkIDs;
    vector<char> buffer(CHUNK_SIZE);
    int chunk_index = 1;

    while (!inputFile.eof()) {
        inputFile.read(buffer.data(), CHUNK_SIZE);
        streamsize bytesRead = inputFile.gcount();
        if (bytesRead == 0) break;

        buffer.resize(bytesRead);
        string chunkID = computeSHA256(buffer);
        chunkIDs.push_back(chunkID);

        UploadChunkRequest request;
        request.set_chunk_id(chunkID);
        request.set_data(buffer.data(), buffer.size());

        UploadChunkResponse response;
        grpc::ClientContext context;

        cout << "Uploading chunk " << chunk_index++ << " (" << chunkID.substr(0, 10) << "...)" << endl;
        grpc::Status status = stub_->UploadChunk(&context, request, &response);

        if (!status.ok()) {
            cerr << "UploadChunk RPC failed: " << status.error_message() << endl;
            return {}; 
        }
    }
    return chunkIDs;
}