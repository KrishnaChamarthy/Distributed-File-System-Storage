#include "uploader.h"
#include "../utils/picosha2.h"
#include <iostream>
#include <fstream>
#include <grpcpp/grpcpp.h>
#include <filesystem>

using namespace std;

Uploader::Uploader(shared_ptr<::filesystem::MasterService::StubInterface> master_stub)
    : master_stub_(master_stub) {}

string computeSHA256(const vector<char>& data) {
    return picosha2::hash256_hex_string(data);
}

bool Uploader::uploadFile(const string& filePath) {
    ifstream inputFile(filePath, ios::binary);
    if (!inputFile) {
        cerr << "Error: Cannot open input file: " << filePath << endl;
        return false;
    }

    string filename = std::filesystem::path(filePath).filename().string();
    vector<char> buffer(CHUNK_SIZE);
    int chunk_index = 1;

    while (!inputFile.eof()) {
        inputFile.read(buffer.data(), CHUNK_SIZE);
        streamsize bytesRead = inputFile.gcount();
        if (bytesRead == 0) break;

        buffer.resize(bytesRead);
        string chunk_id = computeSHA256(buffer);

        ::filesystem::AllocateChunkRequest alloc_req;
        alloc_req.set_filename(filename);
        alloc_req.set_chunk_id(chunk_id);
        
        ::filesystem::AllocateChunkResponse alloc_res;
        grpc::ClientContext alloc_ctx;
        grpc::Status status = master_stub_->AllocateChunk(&alloc_ctx, alloc_req, &alloc_res);

        if (!status.ok() || alloc_res.chunk_server_addresses_size() == 0) {
            cerr << "Failed to allocate chunk " << chunk_index << ": " << status.error_message() << endl;
            return false;
        }

        string chunk_server_addr = alloc_res.chunk_server_addresses(0);
        auto channel = grpc::CreateChannel(chunk_server_addr, grpc::InsecureChannelCredentials());
        auto chunk_stub = ::filesystem::ChunkService::NewStub(channel);

        ::filesystem::UploadChunkRequest upload_req;
        upload_req.set_chunk_id(chunk_id);
        upload_req.set_data(buffer.data(), buffer.size());

        ::filesystem::UploadChunkResponse upload_res;
        grpc::ClientContext upload_ctx;
        
        cout << "Uploading chunk " << chunk_index++ << " (" << chunk_id.substr(0, 8) 
                  << "...) to " << chunk_server_addr << endl;

        status = chunk_stub->UploadChunk(&upload_ctx, upload_req, &upload_res);
        if (!status.ok()) {
            cerr << "Failed to upload chunk: " << status.error_message() << endl;
            return false;
        }
    }
    return true;
}