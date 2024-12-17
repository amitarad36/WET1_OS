#include "signals.h"
#include <unistd.h>
#include <sys/wait.h>
#include <iostream>
#include <cstring>

using namespace std;


// Signal handler for SIGCHLD
void sigchldHandler(int sig_num) {
    (void)sig_num; // Avoid unused parameter warning
    while (waitpid(-1, nullptr, WNOHANG) > 0) {
        // Reap terminated background processes
    }
}

// Setup signal handling for SIGCHLD
void setupSignals() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigchldHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGCHLD, &sa, nullptr) == -1) {
        perror("smash error: sigaction failed");
        exit(1);
    }
}

void ctrlCHandler(int sig_num) {
    // TODO: Add your implementation
}

