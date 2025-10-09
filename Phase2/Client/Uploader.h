#ifndef UPLOADER_H
#define UPLOADER_H

#include <string>
#include <vector>
#include <memory>
#include "../Proto/ChunkService.grpc.pb.h"

using namespace std;

class Uploader {
public:
    Uploader(shared_ptr<chunkservice::ChunkService::StubInterface> stub);
    vector<string> chunkAndUploadFile(const string& filePath);

private:
    shared_ptr<chunkservice::ChunkService::StubInterface> stub_;
    static const size_t CHUNK_SIZE = 4 * 1024 * 1024; 
};

#endif 