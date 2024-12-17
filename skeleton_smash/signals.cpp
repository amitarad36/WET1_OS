#include "signals.h"
#include "Commands.h"
#include <unistd.h>
#include <sys/wait.h>
#include <iostream>
#include <cstring>


using namespace std;


void ctrlCHandler(int sig_num) {
    SmallShell& shell = SmallShell::getInstance();
    int fgPid = shell.getForegroundPid();

    cout << "smash: got ctrl-C" << endl;

    if (fgPid > 0) {
        if (kill(fgPid, SIGKILL) == -1) {
            perror("smash error: kill failed");
        }
        else {
            cout << "smash: process " << fgPid << " was killed" << endl;
        }
        shell.clearForegroundJob(); // Clear the foreground job
    }
}

