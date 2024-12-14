#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "Commands.h"
#include "signals.h"

int main(int argc, char *argv[]) {
    if (signal(SIGINT, controlCSigHandler) == SIG_ERR) {
        perror("smash error: failed to set ctrl-C handler");
    }

    //TODO: setup sig alarm handler

    SmallShell &smash = SmallShell::getInstance();
    smash.createCommandVector();
    while (true) {
        //dup2(SmallShell::getInstance().getOriginalOut(), STDOUT_FILENO);
        std::cout <<smash.getPrompt()+"> ";
        std::string cmd_line;
        std::getline(std::cin, cmd_line);
        smash.executeCommand(cmd_line.c_str(),"","");
    }
    return 0;
}

void SmallShell::createCommandVector()
{
    commandString.push_back("chprompt");
    commandString.push_back("showpid");
    commandString.push_back("pwd");
    commandString.push_back("cd");
    commandString.push_back("jobs");
    commandString.push_back("fg");
    commandString.push_back("quit");
    commandString.push_back("kill");
    commandString.push_back("alias");
    commandString.push_back("unalias");
    commandString.push_back("listdir");
    commandString.push_back("getuser");
    commandString.push_back("watch");
}