#include "cli.h"
#include <iostream>

int main(int argc, char** argv) {
    try {
        dfs::CLI cli;
        
        if (argc == 1) {
            // Interactive mode
            std::cout << "DFS Client - Interactive Mode" << std::endl;
            std::cout << "Type 'help' for available commands, 'quit' to exit." << std::endl;
            return cli.runInteractive();
        } else {
            // Command line mode
            std::vector<std::string> args;
            for (int i = 1; i < argc; ++i) {
                args.emplace_back(argv[i]);
            }
            return cli.executeCommand(args);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}