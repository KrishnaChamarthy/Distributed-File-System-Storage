
#ifndef UPLOADER_H
#define UPLOADER_H

#include <string>
#include <vector>
#include <memory>
#include "../proto/file_system.grpc.pb.h"

using namespace std;

class Uploader {
public:
    Uploader(shared_ptr<::filesystem::MasterService::StubInterface> master_stub);
    bool uploadFile(const string& filePath);

private:
    shared_ptr<::filesystem::MasterService::StubInterface> master_stub_;
    static const size_t CHUNK_SIZE = 4 * 1024 * 1024;
};
#endif