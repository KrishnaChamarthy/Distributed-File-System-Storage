#include <iostream>
#include <string>
#include <grpcpp/grpcpp.h>
#include <filesystem>

#include "uploader.h"
#include "downloader.h"
#include "../proto/file_system.grpc.pb.h"

using namespace std;

int main(int argc, char** argv) {
    if (argc < 3) {
        cerr << "Usage:\n"
                  << "  " << argv[0] << " upload <file_path>\n"
                  << "  " << argv[0] << " download <file_name_on_dfs>" << endl;
        return 1;
    }
    
    string master_address("localhost:50051");
    auto channel = grpc::CreateChannel(master_address, grpc::InsecureChannelCredentials());
    shared_ptr<::filesystem::MasterService::StubInterface> master_stub = ::filesystem::MasterService::NewStub(channel);

    string command = argv[1];
    string path = argv[2];

    if (command == "upload") {
        if (!std::filesystem::exists(path)) {
            cerr << "Error: File not found: " << path << endl;
            return 1;
        }
        Uploader uploader(master_stub);
        if (uploader.uploadFile(path)) {
            cout << "\n✅ Upload successful!" << endl;
        } else {
            cerr << "\n❌ Upload failed." << endl;
        }
    } else if (command == "download") {
        string output_path = "downloaded_" + path;
        Downloader downloader(master_stub);
        if (downloader.downloadFile(path, output_path)) {
            cout << "\n✅ Download successful!" << endl;
            cout << "File saved to: " << output_path << endl;
        } else {
            cerr << "\n❌ Download failed." << endl;
        }
    } else {
        cerr << "Unknown command: " << command << endl;
    }

    return 0;
}