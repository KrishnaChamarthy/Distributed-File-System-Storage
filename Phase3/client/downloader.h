#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <string>
#include <vector>
#include <memory>
#include "../proto/file_system.grpc.pb.h"

using namespace std;

class Downloader {
public:
    Downloader(shared_ptr<::filesystem::MasterService::StubInterface> master_stub);
    bool downloadFile(const string& filename, const string& outputPath);

private:
    shared_ptr<::filesystem::MasterService::StubInterface> master_stub_;
};
#endif