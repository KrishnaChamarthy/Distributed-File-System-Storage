#include "web_server.h"
#include <iostream>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <json/json.h>

namespace dfs {

WebServer::WebServer(std::shared_ptr<MetadataManager> metadata_manager, int port)
    : metadata_manager_(metadata_manager), port_(port), running_(false) {
}

WebServer::~WebServer() {
    stop();
}

void WebServer::start() {
    if (running_.load()) {
        Utils::logWarning("Web server is already running");
        return;
    }
    
    running_.store(true);
    server_thread_ = std::thread([this]() {
        int server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket < 0) {
            Utils::logError("Failed to create socket for web server");
            return;
        }
        
        // Allow socket reuse
        int opt = 1;
        setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port_);
        
        if (bind(server_socket, (struct sockaddr*)&address, sizeof(address)) < 0) {
            Utils::logError("Failed to bind web server socket");
            close(server_socket);
            return;
        }
        
        if (listen(server_socket, 10) < 0) {
            Utils::logError("Failed to listen on web server socket");
            close(server_socket);
            return;
        }
        
        Utils::logInfo("Web dashboard started on http://localhost:" + std::to_string(port_));
        
        while (running_.load()) {
            struct sockaddr_in client_address;
            socklen_t client_len = sizeof(client_address);
            
            int client_socket = accept(server_socket, (struct sockaddr*)&client_address, &client_len);
            if (client_socket < 0) {
                if (running_.load()) {
                    Utils::logError("Failed to accept client connection");
                }
                continue;
            }
            
            // Handle request in a separate thread
            std::thread([this, client_socket]() {
                handleRequest(client_socket);
                close(client_socket);
            }).detach();
        }
        
        close(server_socket);
    });
    
    Utils::logInfo("Web server started on port " + std::to_string(port_));
}

void WebServer::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    
    Utils::logInfo("Web server stopped");
}

void WebServer::handleRequest(int client_socket) {
    char buffer[4096] = {0};
    ssize_t bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);
    
    if (bytes_read <= 0) {
        return;
    }
    
    std::string request(buffer, bytes_read);
    std::string path = parseRequestPath(request);
    
    Utils::logDebug("Web request: " + path);
    
    std::string response = generateResponse(path);
    
    ssize_t bytes_sent = send(client_socket, response.c_str(), response.length(), 0);
    if (bytes_sent < 0) {
        Utils::logError("Failed to send HTTP response");
    }
}

std::string WebServer::generateResponse(const std::string& path) {
    if (path == "/" || path == "/index.html") {
        return getHTTPResponse(generateIndexPage());
    } else if (path == "/files") {
        return getHTTPResponse(generateFilesPage());
    } else if (path == "/servers") {
        return getHTTPResponse(generateServersPage());
    } else if (path == "/stats") {
        return getHTTPResponse(generateStatsPage());
    } else if (path.substr(0, 5) == "/api/") {
        return generateApiResponse(path.substr(5));
    } else if (path == "/style.css") {
        return getHTTPResponse(getCSS(), "text/css");
    } else if (path == "/script.js") {
        return getHTTPResponse(getJavaScript(), "application/javascript");
    } else {
        return getHTTPHeader(404, "text/html", 0) + "\r\n";
    }
}

std::string WebServer::generateIndexPage() {
    auto stats = metadata_manager_->getStatistics();
    
    std::stringstream html;
    html << getHTMLHeader("DFS Dashboard");
    
    html << R"(
<div class="container">
    <h1>Distributed File System Dashboard</h1>
    
    <div class="stats-grid">
        <div class="stat-card">
            <h3>Total Files</h3>
            <div class="stat-value">)" << stats.total_files << R"(</div>
        </div>
        <div class="stat-card">
            <h3>Total Chunks</h3>
            <div class="stat-value">)" << stats.total_chunks << R"(</div>
        </div>
        <div class="stat-card">
            <h3>Total Servers</h3>
            <div class="stat-value">)" << stats.total_servers << R"(</div>
        </div>
        <div class="stat-card">
            <h3>Healthy Servers</h3>
            <div class="stat-value">)" << stats.healthy_servers << R"(</div>
        </div>
    </div>
    
    <div class="stats-grid">
        <div class="stat-card">
            <h3>Storage Used</h3>
            <div class="stat-value">)" << (stats.total_storage_used / (1024*1024*1024)) << R"( GB</div>
        </div>
        <div class="stat-card">
            <h3>Storage Available</h3>
            <div class="stat-value">)" << (stats.total_storage_available / (1024*1024*1024)) << R"( GB</div>
        </div>
        <div class="stat-card">
            <h3>Avg Replication</h3>
            <div class="stat-value">)" << std::fixed << std::setprecision(1) << stats.average_replication_factor << R"(</div>
        </div>
        <div class="stat-card">
            <h3>System Health</h3>
            <div class="stat-value">)" << ((stats.healthy_servers * 100 / std::max(1, stats.total_servers))) << R"(%</div>
        </div>
    </div>
    
    <div class="navigation">
        <a href="/files" class="nav-button">Browse Files</a>
        <a href="/servers" class="nav-button">View Servers</a>
        <a href="/stats" class="nav-button">Detailed Stats</a>
    </div>
</div>
)";
    
    html << getHTMLFooter();
    return html.str();
}

std::string WebServer::generateFilesPage() {
    auto files = metadata_manager_->listFiles();
    
    std::stringstream html;
    html << getHTMLHeader("Files - DFS Dashboard");
    
    html << R"(
<div class="container">
    <h1>Files in DFS</h1>
    <a href="/" class="back-link">&larr; Back to Dashboard</a>
    
    <div class="table-container">
        <table>
            <thead>
                <tr>
                    <th>Filename</th>
                    <th>Size</th>
                    <th>Created</th>
                    <th>Chunks</th>
                    <th>Encrypted</th>
                    <th>EC</th>
                </tr>
            </thead>
            <tbody>
)";
    
    for (const auto& file : files) {
        html << "<tr>";
        html << "<td>" << file.filename << "</td>";
        html << "<td>" << (file.size / 1024) << " KB</td>";
        html << "<td>" << Utils::timestampToString(file.created_time) << "</td>";
        html << "<td>" << file.chunk_ids.size() << "</td>";
        html << "<td>" << (file.is_encrypted ? "Yes" : "No") << "</td>";
        html << "<td>" << (file.is_erasure_coded ? "Yes" : "No") << "</td>";
        html << "</tr>";
    }
    
    html << R"(
            </tbody>
        </table>
    </div>
</div>
)";
    
    html << getHTMLFooter();
    return html.str();
}

std::string WebServer::generateServersPage() {
    auto servers = metadata_manager_->getAllServers();
    
    std::stringstream html;
    html << getHTMLHeader("Servers - DFS Dashboard");
    
    html << R"(
<div class="container">
    <h1>Chunk Servers</h1>
    <a href="/" class="back-link">&larr; Back to Dashboard</a>
    
    <div class="table-container">
        <table>
            <thead>
                <tr>
                    <th>Server ID</th>
                    <th>Address</th>
                    <th>Status</th>
                    <th>Chunks</th>
                    <th>Free Space</th>
                    <th>CPU</th>
                    <th>Memory</th>
                    <th>Last Heartbeat</th>
                </tr>
            </thead>
            <tbody>
)";
    
    for (const auto& server : servers) {
        html << "<tr>";
        html << "<td>" << server.server_id << "</td>";
        html << "<td>" << server.address << ":" << server.port << "</td>";
        html << "<td class=\"" << (server.is_healthy ? "status-healthy" : "status-unhealthy") << "\">";
        html << (server.is_healthy ? "Healthy" : "Unhealthy") << "</td>";
        html << "<td>" << server.chunk_count << "</td>";
        html << "<td>" << (server.free_space / (1024*1024*1024)) << " GB</td>";
        html << "<td>" << std::fixed << std::setprecision(1) << server.cpu_usage << "%</td>";
        html << "<td>" << std::fixed << std::setprecision(1) << server.memory_usage << "%</td>";
        html << "<td>" << Utils::timestampToString(server.last_heartbeat) << "</td>";
        html << "</tr>";
    }
    
    html << R"(
            </tbody>
        </table>
    </div>
</div>
)";
    
    html << getHTMLFooter();
    return html.str();
}

std::string WebServer::generateStatsPage() {
    std::stringstream html;
    html << getHTMLHeader("Statistics - DFS Dashboard");
    
    html << R"(
<div class="container">
    <h1>System Statistics</h1>
    <a href="/" class="back-link">&larr; Back to Dashboard</a>
    
    <div id="stats-content">
        Loading statistics...
    </div>
</div>

<script>
    // Auto-refresh statistics every 5 seconds
    function refreshStats() {
        fetch('/api/stats')
            .then(response => response.json())
            .then(data => {
                document.getElementById('stats-content').innerHTML = 
                    '<pre>' + JSON.stringify(data, null, 2) + '</pre>';
            })
            .catch(error => {
                console.error('Error fetching stats:', error);
            });
    }
    
    refreshStats();
    setInterval(refreshStats, 5000);
</script>
)";
    
    html << getHTMLFooter();
    return html.str();
}

std::string WebServer::generateApiResponse(const std::string& endpoint) {
    std::string json_response;
    
    if (endpoint == "files") {
        json_response = serializeFileList();
    } else if (endpoint == "servers") {
        json_response = serializeServerList();
    } else if (endpoint == "stats") {
        json_response = serializeStatistics();
    } else {
        return getHTTPHeader(404, "application/json", 0) + "\r\n";
    }
    
    return getHTTPResponse(json_response, "application/json");
}

std::string WebServer::getHTTPResponse(const std::string& content, const std::string& content_type) {
    return getHTTPHeader(200, content_type, content.length()) + "\r\n" + content;
}

std::string WebServer::getHTTPHeader(int status_code, const std::string& content_type, size_t content_length) {
    std::stringstream header;
    header << "HTTP/1.1 " << status_code;
    
    switch (status_code) {
        case 200: header << " OK"; break;
        case 404: header << " Not Found"; break;
        case 500: header << " Internal Server Error"; break;
    }
    
    header << "\r\n";
    header << "Content-Type: " << content_type << "\r\n";
    header << "Content-Length: " << content_length << "\r\n";
    header << "Connection: close\r\n";
    
    return header.str();
}

std::string WebServer::parseRequestPath(const std::string& request) {
    std::istringstream iss(request);
    std::string method, path, version;
    iss >> method >> path >> version;
    
    // Remove query parameters
    size_t query_pos = path.find('?');
    if (query_pos != std::string::npos) {
        path = path.substr(0, query_pos);
    }
    
    return path;
}

std::string WebServer::getHTMLHeader(const std::string& title) {
    return R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>)" + title + R"(</title>
    <link rel="stylesheet" href="/style.css">
</head>
<body>
)";
}

std::string WebServer::getHTMLFooter() {
    return R"(
    <script src="/script.js"></script>
</body>
</html>
)";
}

std::string WebServer::getCSS() {
    return R"(
body {
    font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
    margin: 0;
    padding: 0;
    background-color: #f5f5f5;
    color: #333;
}

.container {
    max-width: 1200px;
    margin: 0 auto;
    padding: 20px;
}

h1 {
    color: #2c3e50;
    text-align: center;
    margin-bottom: 30px;
}

.stats-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
    gap: 20px;
    margin-bottom: 30px;
}

.stat-card {
    background: white;
    padding: 20px;
    border-radius: 8px;
    box-shadow: 0 2px 4px rgba(0,0,0,0.1);
    text-align: center;
}

.stat-card h3 {
    margin: 0 0 10px 0;
    color: #7f8c8d;
    font-size: 14px;
    text-transform: uppercase;
}

.stat-value {
    font-size: 32px;
    font-weight: bold;
    color: #2c3e50;
}

.navigation {
    display: flex;
    justify-content: center;
    gap: 20px;
    margin-top: 30px;
}

.nav-button {
    padding: 12px 24px;
    background-color: #3498db;
    color: white;
    text-decoration: none;
    border-radius: 6px;
    transition: background-color 0.3s;
}

.nav-button:hover {
    background-color: #2980b9;
}

.back-link {
    display: inline-block;
    margin-bottom: 20px;
    color: #3498db;
    text-decoration: none;
}

.back-link:hover {
    text-decoration: underline;
}

.table-container {
    background: white;
    border-radius: 8px;
    overflow: hidden;
    box-shadow: 0 2px 4px rgba(0,0,0,0.1);
}

table {
    width: 100%;
    border-collapse: collapse;
}

th, td {
    padding: 12px;
    text-align: left;
    border-bottom: 1px solid #eee;
}

th {
    background-color: #f8f9fa;
    font-weight: 600;
    color: #2c3e50;
}

tr:hover {
    background-color: #f8f9fa;
}

.status-healthy {
    color: #27ae60;
    font-weight: bold;
}

.status-unhealthy {
    color: #e74c3c;
    font-weight: bold;
}

pre {
    background: #f8f9fa;
    padding: 20px;
    border-radius: 4px;
    overflow-x: auto;
}
)";
}

std::string WebServer::getJavaScript() {
    return R"(
// Auto-refresh functionality
function autoRefresh() {
    if (window.location.pathname === '/servers' || window.location.pathname === '/files') {
        setTimeout(() => {
            window.location.reload();
        }, 30000); // Refresh every 30 seconds
    }
}

document.addEventListener('DOMContentLoaded', autoRefresh);
)";
}

std::string WebServer::serializeFileList() {
    auto files = metadata_manager_->listFiles();
    
    Json::Value json_files(Json::arrayValue);
    for (const auto& file : files) {
        Json::Value json_file;
        json_file["filename"] = file.filename;
        json_file["size"] = static_cast<Json::Int64>(file.size);
        json_file["created_time"] = static_cast<Json::Int64>(file.created_time);
        json_file["chunk_count"] = static_cast<int>(file.chunk_ids.size());
        json_file["is_encrypted"] = file.is_encrypted;
        json_file["is_erasure_coded"] = file.is_erasure_coded;
        json_files.append(json_file);
    }
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, json_files);
}

std::string WebServer::serializeServerList() {
    auto servers = metadata_manager_->getAllServers();
    
    Json::Value json_servers(Json::arrayValue);
    for (const auto& server : servers) {
        Json::Value json_server;
        json_server["server_id"] = server.server_id;
        json_server["address"] = server.address;
        json_server["port"] = server.port;
        json_server["is_healthy"] = server.is_healthy;
        json_server["chunk_count"] = server.chunk_count;
        json_server["free_space"] = static_cast<Json::Int64>(server.free_space);
        json_server["total_space"] = static_cast<Json::Int64>(server.total_space);
        json_server["cpu_usage"] = server.cpu_usage;
        json_server["memory_usage"] = server.memory_usage;
        json_server["last_heartbeat"] = static_cast<Json::Int64>(server.last_heartbeat);
        json_servers.append(json_server);
    }
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, json_servers);
}

std::string WebServer::serializeStatistics() {
    auto stats = metadata_manager_->getStatistics();
    auto metrics = Metrics::getInstance().toJSON();
    
    Json::Value json_stats;
    json_stats["total_files"] = stats.total_files;
    json_stats["total_chunks"] = stats.total_chunks;
    json_stats["total_servers"] = stats.total_servers;
    json_stats["healthy_servers"] = stats.healthy_servers;
    json_stats["total_storage_used"] = static_cast<Json::Int64>(stats.total_storage_used);
    json_stats["total_storage_available"] = static_cast<Json::Int64>(stats.total_storage_available);
    json_stats["average_replication_factor"] = stats.average_replication_factor;
    
    // Parse and include system metrics
    Json::CharReaderBuilder reader_builder;
    Json::Value metrics_json;
    std::istringstream metrics_stream(metrics);
    std::string errors;
    
    if (Json::parseFromStream(reader_builder, metrics_stream, &metrics_json, &errors)) {
        json_stats["metrics"] = metrics_json;
    }
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, json_stats);
}

} // namespace dfs

// Main function for standalone web server
int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
        return 1;
    }
    
    int port = std::stoi(argv[1]);
    
    // Create a dummy metadata manager for standalone testing
    auto metadata_manager = std::make_shared<dfs::MetadataManager>();
    
    dfs::WebServer web_server(metadata_manager, port);
    web_server.start();
    
    // Keep running until interrupted
    std::cout << "Press Ctrl+C to stop the web server..." << std::endl;
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}