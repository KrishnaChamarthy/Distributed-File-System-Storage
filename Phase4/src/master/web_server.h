#include "metadata_manager.h"
#include "utils.h"
#include <string>
#include <sstream>
#include <thread>
#include <atomic>
#include <memory>

namespace dfs {

// Simple HTTP server for the web dashboard
class WebServer {
public:
    WebServer(std::shared_ptr<MetadataManager> metadata_manager, int port = 8080);
    ~WebServer();
    
    void start();
    void stop();
    
private:
    std::shared_ptr<MetadataManager> metadata_manager_;
    int port_;
    std::atomic<bool> running_;
    std::thread server_thread_;
    
    // HTTP request handling
    void handleRequest(int client_socket);
    std::string generateResponse(const std::string& path);
    
    // Page generators
    std::string generateIndexPage();
    std::string generateFilesPage();
    std::string generateServersPage();
    std::string generateStatsPage();
    std::string generateApiResponse(const std::string& endpoint);
    
    // Helper methods
    std::string getHTTPResponse(const std::string& content, const std::string& content_type = "text/html");
    std::string getHTTPHeader(int status_code, const std::string& content_type, size_t content_length);
    std::string parseRequestPath(const std::string& request);
    
    // HTML/CSS templates
    std::string getHTMLHeader(const std::string& title);
    std::string getHTMLFooter();
    std::string getCSS();
    std::string getJavaScript();
    
    // JSON helpers
    std::string serializeFileList();
    std::string serializeServerList();
    std::string serializeStatistics();
};

} // namespace dfs