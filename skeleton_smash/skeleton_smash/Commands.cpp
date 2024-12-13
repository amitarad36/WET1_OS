#include "Commands.h"
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <cstdlib>
#include <iostream>

void ChangePromptCommand::execute() {
    std::istringstream iss(m_cmd_line);
    std::string command, newPrompt;

    iss >> command;
    if (iss >> newPrompt) {
        SmallShell::getInstance().setPrompt(newPrompt);
    }
    else {
        SmallShell::getInstance().setPrompt("smash");
    }
}

void ShowPidCommand::execute() {
    std::cout << "smash pid is " << getpid() << std::endl;
}

void GetCurrDirCommand::execute() {
    char cwd[COMMAND_MAX_LENGTH];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        std::cout << cwd << std::endl;
    }
    else {
        perror("smash error: getcwd failed");
    }
}

void ChangeDirCommand::execute() {
    std::istringstream iss(m_cmd_line);
    std::string command, path;

    iss >> command;
    if (!(iss >> path)) {
        return;
    }

    std::string extraArg;
    if (iss >> extraArg) {
        std::cerr << "smash error: cd: too many arguments" << std::endl;
        return;
    }

    SmallShell& shell = SmallShell::getInstance();

    if (path == "-") {
        if (!shell.getLastPwd()) {
            std::cerr << "smash error: cd: OLDPWD not set" << std::endl;
            return;
        }
        path = std::string(shell.getLastPwd());
    }

    if (chdir(path.c_str()) == -1) {
        perror("smash error: chdir failed");
        return;
    }

    char currentDir[COMMAND_MAX_LENGTH];
    if (getcwd(currentDir, sizeof(currentDir)) == nullptr) {
        perror("smash error: getcwd failed");
        return;
    }

    shell.setLastPwd(currentDir);
}

void ExternalCommand::execute() {
    const char* cmd = m_cmd_line.c_str();
    char* args[COMMAND_MAX_ARGS];
    // Parse the command line
    int argCount = _parseCommandLine(cmd, args);

    if (fork() == 0) {
        execvp(args[0], args);
        perror("smash error: execvp failed");
        exit(1);
    }
    else {
        wait(nullptr);
    }
}

Command* SmallShell::CreateCommand(const char* cmd_line) {
    std::string cmd_s = _trim(std::string(cmd_line));
    std::string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

    if (firstWord == "chprompt") {
        return new ChangePromptCommand(cmd_line);
    }
    else if (firstWord == "showpid") {
        return new ShowPidCommand(cmd_line);
    }
    else if (firstWord == "pwd") {
        return new GetCurrDirCommand(cmd_line);
    }
    else if (firstWord == "cd") {
        return new ChangeDirCommand(cmd_line);
    }
    else {
        return new ExternalCommand(cmd_line);
    }
}

void SmallShell::executeCommand(const char* cmd_line) {
    Command* cmd = CreateCommand(cmd_line);
    cmd->execute();
    delete cmd; // Clean up the command object after execution
}

void SmallShell::setLastPwd(const char* newPwd) {
    if (lastPwd) {
        free(lastPwd);
    }
    lastPwd = my_strdup(newPwd);
}
