#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "Commands.h"
#include "signals.h"

int main(int argc, char* argv[]) {
    std::cout << "Debug: Starting smash" << std::endl;

    if (signal(SIGINT, ctrlCHandler) == SIG_ERR) {
        perror("smash error: failed to set ctrl-C handler");
        exit(1); // Exit if signal setup fails
    }

    std::cout << "Debug: Signal handler set for SIGINT" << std::endl;

    SmallShell& smash = SmallShell::getInstance();
    std::cout << "Debug: SmallShell instance initialized" << std::endl;

    while (true) {
        std::cout << "Debug: Entering main loop" << std::endl;

        // Get the current prompt
        std::string prompt = smash.getPrompt();
        std::cout << "Debug: Prompt retrieved: \"" << prompt << "\"" << std::endl;
        std::cout << prompt;

        // Read command line input
        std::string cmd_line;
        if (!std::getline(std::cin, cmd_line)) {
            std::cerr << "Debug: Failed to read input from std::cin" << std::endl;
            break; // Exit the loop on input failure
        }
        std::cout << "Debug: Command line read: \"" << cmd_line << "\"" << std::endl;

        // Execute the command
        try {
            smash.executeCommand(cmd_line.c_str());
            std::cout << "Debug: Command executed successfully" << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "Debug: Exception during command execution: " << e.what() << std::endl;
        }
        catch (...) {
            std::cerr << "Debug: Unknown exception during command execution" << std::endl;
        }

        std::cout << "Debug: Main loop iteration complete" << std::endl;
    }

    std::cout << "Debug: Exiting smash" << std::endl;
    return 0;
}
