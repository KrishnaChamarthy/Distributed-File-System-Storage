#include "cli.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <iomanip>

namespace dfs {

CLI::CLI(std::unique_ptr<DFSClient> client) 
    : client_(std::move(client)), running_(true) {
    
    // Register command handlers
    commands_["put"] = &CLI::handlePut;
    commands_["get"] = &CLI::handleGet;
    commands_["delete"] = &CLI::handleDelete;
    commands_["rm"] = &CLI::handleDelete;
    commands_["list"] = &CLI::handleList;
    commands_["ls"] = &CLI::handleList;
    commands_["info"] = &CLI::handleInfo;
    commands_["stats"] = &CLI::handleStats;
    commands_["help"] = &CLI::handleHelp;
    commands_["?"] = &CLI::handleHelp;
    commands_["quit"] = &CLI::handleQuit;
    commands_["exit"] = &CLI::handleQuit;
    commands_["verbose"] = &CLI::handleVerbose;
    commands_["cache"] = &CLI::handleCache;
}

CLI::~CLI() = default;

void CLI::run() {
    printBanner();
    
    while (running_) {
        printPrompt();
        
        std::string input;
        if (!std::getline(std::cin, input)) {
            break; // EOF
        }
        
        // Skip empty lines
        if (input.empty()) {
            continue;
        }
        
        auto args = parseCommand(input);
        if (args.empty()) {
            continue;
        }
        
        std::string command = args[0];
        args.erase(args.begin());
        
        auto it = commands_.find(command);
        if (it != commands_.end()) {
            try {
                (this->*(it->second))(args);
            } catch (const std::exception& e) {
                std::cout << "Error executing command: " << e.what() << std::endl;
            }
        } else {
            std::cout << "Unknown command: " << command << std::endl;
            std::cout << "Type 'help' for available commands." << std::endl;
        }
    }
    
    std::cout << "Goodbye!" << std::endl;
}

void CLI::handlePut(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cout << "Usage: put <local_file> <remote_file> [options]" << std::endl;
        std::cout << "Options:" << std::endl;
        std::cout << "  --no-encryption   Disable encryption" << std::endl;
        std::cout << "  --erasure-coding  Enable erasure coding" << std::endl;
        return;
    }
    
    std::map<std::string, std::string> options;
    std::vector<std::string> remaining_args;
    
    if (!parseOptions(args, options, remaining_args)) {
        return;
    }
    
    if (remaining_args.size() != 2) {
        std::cout << "Error: Expected exactly 2 arguments (local_file and remote_file)" << std::endl;
        return;
    }
    
    std::string local_file = remaining_args[0];
    std::string remote_file = remaining_args[1];
    
    bool enable_encryption = options.find("no-encryption") == options.end();
    bool enable_erasure_coding = options.find("erasure-coding") != options.end();
    
    std::cout << "Uploading " << local_file << " to " << remote_file << std::endl;
    if (!enable_encryption) std::cout << "  Encryption: Disabled" << std::endl;
    if (enable_erasure_coding) std::cout << "  Erasure Coding: Enabled" << std::endl;
    
    bool success = client_->put(local_file, remote_file, enable_encryption, enable_erasure_coding);
    
    if (!success) {
        std::cout << "Upload failed!" << std::endl;
    }
}

void CLI::handleGet(const std::vector<std::string>& args) {
    if (args.size() != 2) {
        std::cout << "Usage: get <remote_file> <local_file>" << std::endl;
        return;
    }
    
    std::string remote_file = args[0];
    std::string local_file = args[1];
    
    std::cout << "Downloading " << remote_file << " to " << local_file << std::endl;
    
    bool success = client_->get(remote_file, local_file);
    
    if (!success) {
        std::cout << "Download failed!" << std::endl;
    }
}

void CLI::handleDelete(const std::vector<std::string>& args) {
    if (args.size() != 1) {
        std::cout << "Usage: delete <remote_file>" << std::endl;
        return;
    }
    
    std::string remote_file = args[0];
    
    std::cout << "Are you sure you want to delete '" << remote_file << "'? (y/N): ";
    std::string confirmation;
    std::getline(std::cin, confirmation);
    
    if (confirmation != "y" && confirmation != "Y" && confirmation != "yes") {
        std::cout << "Delete cancelled." << std::endl;
        return;
    }
    
    bool success = client_->deleteFile(remote_file);
    
    if (!success) {
        std::cout << "Delete failed!" << std::endl;
    }
}

void CLI::handleList(const std::vector<std::string>& args) {
    std::string path_prefix;
    
    if (!args.empty()) {
        path_prefix = args[0];
    }
    
    bool success = client_->listFiles(path_prefix);
    
    if (!success) {
        std::cout << "List failed!" << std::endl;
    }
}

void CLI::handleInfo(const std::vector<std::string>& args) {
    if (args.size() != 1) {
        std::cout << "Usage: info <remote_file>" << std::endl;
        return;
    }
    
    std::string remote_file = args[0];
    
    bool success = client_->getFileInfo(remote_file);
    
    if (!success) {
        std::cout << "Info failed!" << std::endl;
    }
}

void CLI::handleStats(const std::vector<std::string>& args) {
    client_->printStatistics();
}

void CLI::handleHelp(const std::vector<std::string>& args) {
    printHelp();
}

void CLI::handleQuit(const std::vector<std::string>& args) {
    running_ = false;
}

void CLI::handleVerbose(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cout << "Usage: verbose <on|off>" << std::endl;
        return;
    }
    
    std::string mode = args[0];
    std::transform(mode.begin(), mode.end(), mode.begin(), ::tolower);
    
    if (mode == "on" || mode == "true" || mode == "1") {
        client_->enableVerboseLogging(true);
        std::cout << "Verbose logging enabled." << std::endl;
    } else if (mode == "off" || mode == "false" || mode == "0") {
        client_->enableVerboseLogging(false);
        std::cout << "Verbose logging disabled." << std::endl;
    } else {
        std::cout << "Invalid option. Use 'on' or 'off'." << std::endl;
    }
}

void CLI::handleCache(const std::vector<std::string>& args) {
    if (args.empty()) {
        client_->printStatistics();
        return;
    }
    
    std::string subcommand = args[0];
    
    if (subcommand == "size" && args.size() == 2) {
        try {
            size_t size_mb = std::stoull(args[1]);
            client_->setCacheSize(size_mb);
            std::cout << "Cache size set to " << size_mb << " MB." << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Invalid cache size: " << args[1] << std::endl;
        }
    } else {
        std::cout << "Usage: cache [size <MB>]" << std::endl;
        std::cout << "  cache         - Show cache statistics" << std::endl;
        std::cout << "  cache size N  - Set cache size to N MB" << std::endl;
    }
}

std::vector<std::string> CLI::parseCommand(const std::string& input) {
    std::vector<std::string> tokens;
    std::istringstream iss(input);
    std::string token;
    
    while (iss >> token) {
        tokens.push_back(token);
    }
    
    return tokens;
}

void CLI::printPrompt() {
    std::cout << "dfs> ";
}

void CLI::printBanner() {
    std::cout << std::endl;
    std::cout << "╔═══════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                  Distributed File System                     ║" << std::endl;
    std::cout << "║                      Client Interface                        ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;
    std::cout << "Type 'help' for available commands." << std::endl;
    std::cout << std::endl;
}

void CLI::printHelp() {
    std::cout << std::endl;
    std::cout << "Available Commands:" << std::endl;
    std::cout << "═══════════════════" << std::endl;
    std::cout << std::endl;
    
    std::cout << std::left << std::setw(25) << "put <local> <remote>" 
              << "Upload a file to the DFS" << std::endl;
    std::cout << std::left << std::setw(25) << "  --no-encryption" 
              << "Disable encryption for upload" << std::endl;
    std::cout << std::left << std::setw(25) << "  --erasure-coding" 
              << "Enable erasure coding for upload" << std::endl;
    std::cout << std::endl;
    
    std::cout << std::left << std::setw(25) << "get <remote> <local>" 
              << "Download a file from the DFS" << std::endl;
    std::cout << std::endl;
    
    std::cout << std::left << std::setw(25) << "delete <remote>" 
              << "Delete a file from the DFS" << std::endl;
    std::cout << std::left << std::setw(25) << "rm <remote>" 
              << "Alias for delete" << std::endl;
    std::cout << std::endl;
    
    std::cout << std::left << std::setw(25) << "list [prefix]" 
              << "List files in the DFS" << std::endl;
    std::cout << std::left << std::setw(25) << "ls [prefix]" 
              << "Alias for list" << std::endl;
    std::cout << std::endl;
    
    std::cout << std::left << std::setw(25) << "info <remote>" 
              << "Show detailed file information" << std::endl;
    std::cout << std::endl;
    
    std::cout << std::left << std::setw(25) << "stats" 
              << "Show client statistics" << std::endl;
    std::cout << std::endl;
    
    std::cout << std::left << std::setw(25) << "verbose <on|off>" 
              << "Enable/disable verbose logging" << std::endl;
    std::cout << std::endl;
    
    std::cout << std::left << std::setw(25) << "cache [size <MB>]" 
              << "Show/configure client cache" << std::endl;
    std::cout << std::endl;
    
    std::cout << std::left << std::setw(25) << "help, ?" 
              << "Show this help message" << std::endl;
    std::cout << std::left << std::setw(25) << "quit, exit" 
              << "Exit the client" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Examples:" << std::endl;
    std::cout << "─────────" << std::endl;
    std::cout << "  put document.pdf /docs/document.pdf" << std::endl;
    std::cout << "  get /docs/document.pdf downloaded.pdf" << std::endl;
    std::cout << "  put large_file.zip /backup/large_file.zip --erasure-coding" << std::endl;
    std::cout << "  list /docs/" << std::endl;
    std::cout << "  info /docs/document.pdf" << std::endl;
    std::cout << "  delete /docs/old_document.pdf" << std::endl;
    std::cout << std::endl;
}

bool CLI::parseOptions(const std::vector<std::string>& args, 
                      std::map<std::string, std::string>& options,
                      std::vector<std::string>& remaining_args) {
    
    options.clear();
    remaining_args.clear();
    
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        
        if (arg.substr(0, 2) == "--") {
            // Long option
            std::string option = arg.substr(2);
            
            // Check if it has a value (--option=value)
            size_t eq_pos = option.find('=');
            if (eq_pos != std::string::npos) {
                std::string key = option.substr(0, eq_pos);
                std::string value = option.substr(eq_pos + 1);
                options[key] = value;
            } else {
                // Check if next argument is a value
                if (i + 1 < args.size() && args[i + 1][0] != '-') {
                    options[option] = args[++i];
                } else {
                    options[option] = ""; // Flag option
                }
            }
        } else if (arg[0] == '-' && arg.length() > 1) {
            // Short option(s)
            for (size_t j = 1; j < arg.length(); ++j) {
                std::string option(1, arg[j]);
                options[option] = ""; // Flag option
            }
        } else {
            // Regular argument
            remaining_args.push_back(arg);
        }
    }
    
    return true;
}