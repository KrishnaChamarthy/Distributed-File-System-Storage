#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <filesystem>
#include <thread>
#include <chrono>

#include <grpcpp/grpcpp.h>
#include "../proto/file_system.grpc.pb.h"

using namespace std;
using namespace grpc;
using namespace std::filesystem;

string CHUNK_STORAGE_PATH = "./data/chunks_";

class ChunkServiceImpl final : public ::filesystem::ChunkService::Service {
    Status UploadChunk(ServerContext* context, const ::filesystem::UploadChunkRequest* request, ::filesystem::UploadChunkResponse* response) override {
        const auto& chunk_id = request->chunk_id();
        const auto& data = request->data();
        string chunk_path = CHUNK_STORAGE_PATH + "/" + chunk_id;
        ofstream chunk_file(chunk_path, ios::binary);
        if (!chunk_file) {
            return Status(StatusCode::INTERNAL, "Cannot write chunk to disk.");
        }
        chunk_file.write(data.c_str(), data.size());
        response->set_success(true);
        return Status::OK;
    }

    Status DownloadChunk(ServerContext* context, const ::filesystem::DownloadChunkRequest* request, ::filesystem::DownloadChunkResponse* response) override {
        const auto& chunk_id = request->chunk_id();
        string chunk_path = CHUNK_STORAGE_PATH + "/" + chunk_id;
        ifstream chunk_file(chunk_path, ios::binary);
        if (!chunk_file) {
            return Status(StatusCode::NOT_FOUND, "Chunk ID not found on server.");
        }
        string data((istreambuf_iterator<char>(chunk_file)), istreambuf_iterator<char>());
        response->set_data(data);
        return Status::OK;
    }
};

void SendHeartbeats(const string& my_address, const string& master_address) {
    auto channel = CreateChannel(master_address, InsecureChannelCredentials());
    auto stub = ::filesystem::MasterService::NewStub(channel);

    while (true) {
        ::filesystem::HeartbeatRequest request;
        request.set_server_address(my_address);
        ::filesystem::HeartbeatResponse response;
        ClientContext context;
        
        Status status = stub->Heartbeat(&context, request, &response);
        
        this_thread::sleep_for(chrono::seconds(5));
    }
}

void RunServer(const string& my_address, const string& master_address) {
    ChunkServiceImpl service;
    
    create_directories(CHUNK_STORAGE_PATH);
    
    ServerBuilder builder;
    builder.AddListeningPort(my_address, InsecureServerCredentials());
    builder.RegisterService(&service);
    
    unique_ptr<Server> server(builder.BuildAndStart());
    cout << "✅ Chunk server listening on " << my_address << endl;
    
    thread heartbeat_thread(SendHeartbeats, my_address, master_address);
    
    server->Wait();
    heartbeat_thread.join();
}

int main(int argc, char** argv) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <my_listen_address> <master_address>" << endl;
        cerr << "Example: " << argv[0] << " 0.0.0.0:60051 0.0.0.0:50051" << endl;
        return 1;
    }
    string my_address = argv[1];
    string master_address = argv[2];
    
    size_t colon_pos = my_address.find_last_of(':');
    CHUNK_STORAGE_PATH += my_address.substr(colon_pos + 1);

    RunServer(my_address, master_address);
    return 0;
}