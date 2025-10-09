#include "Downloader.h"
#include <iostream>
#include <fstream>
#include <grpcpp/grpcpp.h>

using namespace std;

using chunkservice::DownloadChunkRequest;
using chunkservice::DownloadChunkResponse;

Downloader::Downloader(shared_ptr<chunkservice::ChunkService::StubInterface> stub)
    : stub_(stub) {}

bool Downloader::assembleAndSaveFile(const vector<string>& chunkIDs, const string& outputFilePath) {
    ofstream outputFile(outputFilePath, ios::binary);
    if (!outputFile) {
        cerr << "Error: Cannot create output file: " << outputFilePath << endl;
        return false;
    }

    int chunk_index = 1;
    for (const auto& chunkID : chunkIDs) {
        DownloadChunkRequest request;
        request.set_chunk_id(chunkID);
        
        DownloadChunkResponse response;
        grpc::ClientContext context;

        cout << "Downloading chunk " << chunk_index++ << " (" << chunkID.substr(0, 10) << "...)" << endl;
        grpc::Status status = stub_->DownloadChunk(&context, request, &response);

        if (!status.ok()) {
            cerr << "DownloadChunk RPC failed: " << status.error_message() << endl;
            return false;
        }

        outputFile.write(response.data().c_str(), response.data().size());
    }
    return true;
}