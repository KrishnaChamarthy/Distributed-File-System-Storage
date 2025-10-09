#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <string>
#include <vector>
#include <memory>
#include "../Proto/ChunkService.grpc.pb.h"

using namespace std;

class Downloader {
public:
    Downloader(shared_ptr<chunkservice::ChunkService::StubInterface> stub);
    bool assembleAndSaveFile(const vector<string>& chunkIDs, const string& outputFilePath);

private:
    shared_ptr<chunkservice::ChunkService::StubInterface> stub_;
};

#endif 