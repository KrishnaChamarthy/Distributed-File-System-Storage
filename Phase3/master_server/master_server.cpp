#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <chrono>

#include <grpcpp/grpcpp.h>
#include "../proto/file_system.grpc.pb.h"

using namespace std;
using namespace grpc;

const string METADATA_FILE = "master_state.pb";

struct MasterState {
    mutex mtx;
    map<string, vector<string>> file_to_chunks;
    map<string, vector<string>> chunk_to_servers;
    map<string, chrono::system_clock::time_point> live_servers;
};

class MasterServiceImpl final : public ::filesystem::MasterService::Service {
public:
    MasterState state;

    MasterServiceImpl() {
        loadMetadata();
    }

    Status GetFileInfo(ServerContext* context, const ::filesystem::FileInfoRequest* request, ::filesystem::FileInfoResponse* response) override {
        lock_guard<mutex> lock(state.mtx);
        const auto& filename = request->filename();
        if (state.file_to_chunks.count(filename)) {
            for (const auto& chunk_id : state.file_to_chunks[filename]) {
                response->add_chunk_ids(chunk_id);
            }
        } else {
            return Status(StatusCode::NOT_FOUND, "File not found");
        }
        return Status::OK;
    }

    Status AllocateChunk(ServerContext* context, const ::filesystem::AllocateChunkRequest* request, ::filesystem::AllocateChunkResponse* response) override {
        lock_guard<mutex> lock(state.mtx);
        const auto& filename = request->filename();
        const auto& chunk_id = request->chunk_id();

        if (state.live_servers.empty()) {
            return Status(StatusCode::UNAVAILABLE, "No chunk servers available");
        }

        const string& server_address = state.live_servers.begin()->first;
        
        state.file_to_chunks[filename].push_back(chunk_id);
        state.chunk_to_servers[chunk_id].push_back(server_address);

        response->add_chunk_server_addresses(server_address);
        
        cout << "Allocated chunk " << chunk_id.substr(0, 8) << " for file " << filename << " to server " << server_address << endl;
        
        return Status::OK;
    }

    Status GetChunkLocations(ServerContext* context, const ::filesystem::ChunkLocationRequest* request, ::filesystem::ChunkLocationResponse* response) override {
        lock_guard<mutex> lock(state.mtx);
        const auto& chunk_id = request->chunk_id();

        if (state.chunk_to_servers.count(chunk_id)) {
            for (const auto& server_addr : state.chunk_to_servers[chunk_id]) {
                response->add_chunk_server_addresses(server_addr);
            }
        } else {
            return Status(StatusCode::NOT_FOUND, "Chunk not found");
        }
        return Status::OK;
    }

    Status Heartbeat(ServerContext* context, const ::filesystem::HeartbeatRequest* request, ::filesystem::HeartbeatResponse* response) override {
        lock_guard<mutex> lock(state.mtx);
        const auto& server_address = request->server_address();
        state.live_servers[server_address] = chrono::system_clock::now();
        return Status::OK;
    }

    void saveMetadata() {
        ::filesystem::MasterMetadata metadata;
        lock_guard<mutex> lock(state.mtx);

        for (const auto& pair : state.file_to_chunks) {
            ::filesystem::FileMetadata* f_meta = metadata.add_files();
            f_meta->set_filename(pair.first);
            for (const auto& chunk_id : pair.second) {
                f_meta->add_chunk_ids(chunk_id);
            }
        }

        for (const auto& pair : state.chunk_to_servers) {
            ::filesystem::ChunkMetadata* c_meta = metadata.add_chunks();
            c_meta->set_chunk_id(pair.first);
            for (const auto& addr : pair.second) {
                c_meta->add_server_addresses(addr);
            }
        }
        
        ofstream out(METADATA_FILE, ios::binary);
        metadata.SerializeToOstream(&out);
    }

    void loadMetadata() {
        ::filesystem::MasterMetadata metadata;
        ifstream in(METADATA_FILE, ios::binary);
        if (!in) {
            cout << "No existing metadata file found. Starting fresh." << endl;
            return;
        }
        if (metadata.ParseFromIstream(&in)) {
             lock_guard<mutex> lock(state.mtx);
            for (const auto& f_meta : metadata.files()) {
                for (const auto& chunk_id : f_meta.chunk_ids()) {
                    state.file_to_chunks[f_meta.filename()].push_back(chunk_id);
                }
            }
            for (const auto& c_meta : metadata.chunks()) {
                 for (const auto& addr : c_meta.server_addresses()) {
                    state.chunk_to_servers[c_meta.chunk_id()].push_back(addr);
                }
            }
            cout << "✅ Successfully loaded metadata from " << METADATA_FILE << endl;
        } else {
             cerr << "❌ Failed to parse metadata file." << endl;
        }
    }
};

void RunServer(MasterServiceImpl* service) {
    string server_address("0.0.0.0:50051");
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(service);
    unique_ptr<Server> server(builder.BuildAndStart());
    cout << "✅ Master server listening on " << server_address << endl;
    server->Wait();
}

void RunPersistenceThread(MasterServiceImpl* service) {
    while (true) {
        this_thread::sleep_for(chrono::seconds(15));
        cout << "💾 Persisting metadata to disk..." << endl;
        service->saveMetadata();
    }
}

int main() {
    MasterServiceImpl service;
    
    thread persistence_thread(RunPersistenceThread, &service);
    
    RunServer(&service);

    persistence_thread.join();
    return 0;
}