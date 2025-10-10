#include "downloader.h"
#include <iostream>
#include <fstream>
#include <grpcpp/grpcpp.h>

using namespace std;

Downloader::Downloader(shared_ptr<::filesystem::MasterService::StubInterface> master_stub)
    : master_stub_(master_stub) {}

bool Downloader::downloadFile(const string& filename, const string& outputPath) {
    ::filesystem::FileInfoRequest info_req;
    info_req.set_filename(filename);
    
    ::filesystem::FileInfoResponse info_res;
    grpc::ClientContext info_ctx;
    grpc::Status status = master_stub_->GetFileInfo(&info_ctx, info_req, &info_res);

    if (!status.ok()) {
        cerr << "Failed to get file info for " << filename << ": " << status.error_message() << endl;
        return false;
    }
    
    ofstream outputFile(outputPath, ios::binary);
    if (!outputFile) {
        cerr << "Error: Cannot create output file: " << outputPath << endl;
        return false;
    }

    int chunk_index = 1;
    for (const auto& chunk_id : info_res.chunk_ids()) {
        ::filesystem::ChunkLocationRequest loc_req;
        loc_req.set_chunk_id(chunk_id);
        
        ::filesystem::ChunkLocationResponse loc_res;
        grpc::ClientContext loc_ctx;
        status = master_stub_->GetChunkLocations(&loc_ctx, loc_req, &loc_res);

        if (!status.ok() || loc_res.chunk_server_addresses_size() == 0) {
            cerr << "Failed to get location for chunk " << chunk_id << ": " << status.error_message() << endl;
            return false;
        }

        string chunk_server_addr = loc_res.chunk_server_addresses(0);
        auto channel = grpc::CreateChannel(chunk_server_addr, grpc::InsecureChannelCredentials());
        auto chunk_stub = ::filesystem::ChunkService::NewStub(channel);

        ::filesystem::DownloadChunkRequest download_req;
        download_req.set_chunk_id(chunk_id);

        ::filesystem::DownloadChunkResponse download_res;
        grpc::ClientContext download_ctx;

        cout << "Downloading chunk " << chunk_index++ << " (" << chunk_id.substr(0, 8)
                  << "...) from " << chunk_server_addr << endl;
        
        status = chunk_stub->DownloadChunk(&download_ctx, download_req, &download_res);
        if (!status.ok()) {
            cerr << "Failed to download chunk: " << status.error_message() << endl;
            return false;
        }

        outputFile.write(download_res.data().c_str(), download_res.data().size());
    }
    return true;
}