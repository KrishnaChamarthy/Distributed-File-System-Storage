#include <iostream>
#include <string>
#include <grpcpp/grpcpp.h>
#include <filesystem>
#include <fstream>

#include "Uploader.h"
#include "Downloader.h"
#include "../Proto/ChunkService.grpc.pb.h"

using namespace std;

void saveChunkList(const string& fileName, const vector<string>& chunkIDs) {
    filesystem::create_directories("downloads");
    filesystem::path filePath(fileName);
    string recipeFileName = "downloads/" + filePath.filename().string() + ".recipe";
    ofstream file(recipeFileName);
    for (const auto& id : chunkIDs) {
        file << id << endl;
    }
    cout << "Recipe saved to: " << recipeFileName << endl;
}

vector<string> loadChunkList(const string& recipeFile) {
    vector<string> chunkIDs;
    ifstream file(recipeFile);
    string id;
    while (getline(file, id)) {
        chunkIDs.push_back(id);
    }
    return chunkIDs;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        cerr << "Usage:\n"
             << "  " << argv[0] << " upload <file_path>\n"
             << "  " << argv[0] << " download <recipe_file_path>" << endl;
        cerr << "\nNote: Files should be in 'uploads/' folder for upload" << endl;
        cerr << "      Recipe files should be in 'downloads/' folder for download" << endl;
        return 1;
    }

    string command = argv[1];
    string filePath = argv[2];
    
    string server_address("localhost:50051");
    auto channel = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());

    if (command == "upload") {
        string uploadPath = "uploads/" + filePath;
        if (!filesystem::exists(uploadPath)) {
            cerr << "Error: File not found: " << uploadPath << endl;
            cerr << "Please place files in the 'uploads/' folder" << endl;
            return 1;
        }
        auto stub = chunkservice::ChunkService::NewStub(channel);
        shared_ptr<chunkservice::ChunkService::StubInterface> shared_stub(stub.release());
        Uploader uploader(shared_stub);
        cout << "Starting upload for: " << uploadPath << endl;
        auto chunkIDs = uploader.chunkAndUploadFile(uploadPath);
        if (!chunkIDs.empty()) {
            saveChunkList(filePath, chunkIDs);
            cout << "\n✅ Upload successful!" << endl;
        } else {
            cerr << "\n❌ Upload failed." << endl;
        }
    } else if (command == "download") {
        string recipePath = "downloads/" + filePath;
        if (!filesystem::exists(recipePath)) {
            cerr << "Error: Recipe file not found: " << recipePath << endl;
            cerr << "Recipe files should be in the 'downloads/' folder" << endl;
            return 1;
        }
        auto chunkIDs = loadChunkList(recipePath);
        filesystem::path recipeFile(filePath);
        string originalFileName = recipeFile.stem().string();
        string downloadPath = "downloads/downloaded_" + originalFileName;

        auto stub = chunkservice::ChunkService::NewStub(channel);
        shared_ptr<chunkservice::ChunkService::StubInterface> shared_stub(stub.release());
        Downloader downloader(shared_stub);
        cout << "Starting download using recipe: " << recipePath << endl;
        bool success = downloader.assembleAndSaveFile(chunkIDs, downloadPath);
        if (success) {
            cout << "\n✅ Download successful!" << endl;
            cout << "File saved to: " << downloadPath << endl;
        } else {
            cerr << "\n❌ Download failed." << endl;
        }
    } else {
        cerr << "Unknown command: " << command << endl;
        return 1;
    }

    return 0;
}