#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>

#include <grpcpp/grpcpp.h>
#include "../Proto/ChunkService.grpc.pb.h"

using namespace std;

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using chunkservice::ChunkService;
using chunkservice::UploadChunkRequest;
using chunkservice::UploadChunkResponse;
using chunkservice::DownloadChunkRequest;
using chunkservice::DownloadChunkResponse;

const string CHUNK_STORAGE_PATH = "./data/chunks";

class ChunkServiceImpl final : public ChunkService::Service {
    Status UploadChunk(ServerContext* context, const UploadChunkRequest* request,
                       UploadChunkResponse* response) override {
        const string& chunk_id = request->chunk_id();
        const string& data = request->data();

        cout << "Received UploadChunk request for chunk ID: " << chunk_id << endl;

        string chunk_path = CHUNK_STORAGE_PATH + "/" + chunk_id;
        ofstream chunk_file(chunk_path, ios::binary);

        if (!chunk_file) {
            cerr << "Failed to open file for writing: " << chunk_path << endl;
            response->set_success(false);
            response->set_message("Failed to open file on server.");
            return Status(grpc::StatusCode::INTERNAL, "Cannot write chunk to disk.");
        }

        chunk_file.write(data.c_str(), data.size());
        chunk_file.close();

        response->set_success(true);
        response->set_message("Chunk uploaded successfully.");
        return Status::OK;
    }

    Status DownloadChunk(ServerContext* context, const DownloadChunkRequest* request,
                         DownloadChunkResponse* response) override {
        const string& chunk_id = request->chunk_id();
        cout << "Received DownloadChunk request for chunk ID: " << chunk_id << endl;

        string chunk_path = CHUNK_STORAGE_PATH + "/" + chunk_id;
        ifstream chunk_file(chunk_path, ios::binary);

        if (!chunk_file) {
            cerr << "Chunk not found: " << chunk_path << endl;
            return Status(grpc::StatusCode::NOT_FOUND, "Chunk ID not found on server.");
        }
        
        string data((istreambuf_iterator<char>(chunk_file)),
                    istreambuf_iterator<char>());
        
        response->set_data(data);
        return Status::OK;
    }
};

void RunServer() {
    string server_address("0.0.0.0:50051");
    ChunkServiceImpl service;

    filesystem::create_directories(CHUNK_STORAGE_PATH);

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    unique_ptr<Server> server(builder.BuildAndStart());
    cout << "✅ Server listening on " << server_address << endl;

    server->Wait();
}

int main(int argc, char** argv) {
    RunServer();
    return 0;
}