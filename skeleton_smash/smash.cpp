#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "Commands.h"
#include "signals.h"

int main(int argc, char* argv[]) {
    if (signal(SIGINT, ctrlCHandler) == SIG_ERR) {
        perror("smash error: failed to set ctrl-C handler");
        exit(1); // Exit if signal setup fails
    }
    if (signal(SIGCHLD, sigchldHandler) == SIG_ERR) {
        perror("smash error: failed to set SIGCHLD handler");
        exit(1);
    }

    SmallShell& smash = SmallShell::getInstance();

    while (true) {
        // Get the current prompt
        std::string prompt = smash.getPrompt();
        std::cout << prompt;

        // Read command line input
        std::string cmd_line;
        if (!std::getline(std::cin, cmd_line)) {
            break; // Exit the loop on input failure
        }

        // Execute the command
        smash.executeCommand(cmd_line.c_str());
    }

    return 0;
}
