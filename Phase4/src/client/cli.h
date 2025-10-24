#include "client.h"
#include <iostream>
#include <string>
#include <vector>
#include <map>

namespace dfs {

class CLI {
public:
    CLI(std::unique_ptr<DFSClient> client);
    ~CLI();
    
    // Main command loop
    void run();
    
    // Command handlers
    void handlePut(const std::vector<std::string>& args);
    void handleGet(const std::vector<std::string>& args);
    void handleDelete(const std::vector<std::string>& args);
    void handleList(const std::vector<std::string>& args);
    void handleInfo(const std::vector<std::string>& args);
    void handleStats(const std::vector<std::string>& args);
    void handleHelp(const std::vector<std::string>& args);
    void handleQuit(const std::vector<std::string>& args);
    void handleVerbose(const std::vector<std::string>& args);
    void handleCache(const std::vector<std::string>& args);
    
private:
    std::unique_ptr<DFSClient> client_;
    bool running_;
    
    using CommandHandler = void (CLI::*)(const std::vector<std::string>&);
    std::map<std::string, CommandHandler> commands_;
    
    // Helper methods
    std::vector<std::string> parseCommand(const std::string& input);
    void printPrompt();
    void printBanner();
    void printHelp();
    bool parseOptions(const std::vector<std::string>& args, 
                     std::map<std::string, std::string>& options,
                     std::vector<std::string>& remaining_args);
};

} // namespace dfs

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <master_address> <master_port> [command] [args...]" << std::endl;
        std::cout << "\nExamples:" << std::endl;
        std::cout << "  " << argv[0] << " localhost 50051                           # Interactive mode" << std::endl;
        std::cout << "  " << argv[0] << " localhost 50051 put local.txt remote.txt  # Single command" << std::endl;
        return 1;
    }
    
    std::string master_address = argv[1];
    int master_port = std::stoi(argv[2]);
    
    // Create client
    auto client = std::make_unique<dfs::DFSClient>(master_address, master_port);
    dfs::CLI cli(std::move(client));
    
    if (argc > 3) {
        // Single command mode
        std::vector<std::string> args;
        for (int i = 3; i < argc; ++i) {
            args.push_back(argv[i]);
        }
        
        std::string command = args[0];
        args.erase(args.begin());
        
        if (command == "put") {
            cli.handlePut(args);
        } else if (command == "get") {
            cli.handleGet(args);
        } else if (command == "delete" || command == "rm") {
            cli.handleDelete(args);
        } else if (command == "list" || command == "ls") {
            cli.handleList(args);
        } else if (command == "info") {
            cli.handleInfo(args);
        } else if (command == "stats") {
            cli.handleStats(args);
        } else {
            std::cout << "Unknown command: " << command << std::endl;
            return 1;
        }
    } else {
        // Interactive mode
        cli.run();
    }
    
    return 0;
}