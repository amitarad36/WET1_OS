
#include "Commands.h"
#include <unistd.h>
#include <sys/wait.h>
#include <iostream>
#include <cstring>
#include <csignal>
#include <sstream>

// Utility functions
std::string _ltrim(const std::string& s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

std::string _rtrim(const std::string& s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

std::string _trim(const std::string& s) {
    return _rtrim(_ltrim(s));
}

bool _isBackgroundCommand(const char* cmd_line) {
    const std::string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char* cmd_line) {
    std::string str(cmd_line);
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    if (idx == std::string::npos) return;
    if (cmd_line[idx] != '&') return;
    cmd_line[idx] = ' ';
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

int _parseCommandLine(const char* cmd_line, char** args) {
    if (cmd_line == nullptr || strlen(cmd_line) == 0) {
        // Empty input: No arguments to parse
        args[0] = nullptr;
        return 0;
    }

    int i = 0;
    std::istringstream iss(_trim(std::string(cmd_line))); // Trim and initialize input stream
    for (std::string s; iss >> s;) {
        args[i] = (char*)malloc(s.length() + 1); // Allocate memory for each argument
        if (args[i] == nullptr) {
            perror("smash error: malloc failed");
            exit(1);
        }
        strcpy(args[i], s.c_str()); // Copy the token into the argument array
        i++;

        if (i >= COMMAND_MAX_ARGS) {
            std::cerr << "smash error: too many arguments" << std::endl;
            break;
        }
    }
    args[i] = nullptr; // Null-terminate the argument list
    return i;
}

// ======================= Command Class ====================
Command::Command(const char* cmd_line) : m_cmd_line(cmd_line) {}
Command::~Command() {}

std::string Command::getCommandLine() const {
    return std::string(m_cmd_line);
}

// ==================== BuiltInCommand Class ===============
BuiltInCommand::BuiltInCommand(const char* cmd_line) : Command(cmd_line) {}
BuiltInCommand::~BuiltInCommand() {}

// ================= ChangePromptCommand Class ===============
ChangePromptCommand::ChangePromptCommand(const char* cmd_line, std::string& prompt) : BuiltInCommand(cmd_line), m_prompt(prompt) {}
ChangePromptCommand::~ChangePromptCommand() {}

void ChangePromptCommand::execute() {
    // Trim and parse the command line
    std::string cmd_str = _trim(std::string(m_cmd_line));

    size_t first_space = cmd_str.find(' ');

    if (first_space == std::string::npos) {
        // No argument provided, reset to "smash"
        m_prompt = "smash";
    }
    else {
        // Extract the argument after the first space
        std::string new_prompt = cmd_str.substr(first_space + 1);
        new_prompt = _trim(new_prompt); // Remove any trailing or leading spaces
        size_t second_space = new_prompt.find(' ');

        // Only take the first word
        if (second_space != std::string::npos) {
            new_prompt = new_prompt.substr(0, second_space);
        }

        // Update the prompt
        m_prompt = new_prompt;
    }
}

// ================== ShowPidCommand Class =================
ShowPidCommand::ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}
ShowPidCommand::~ShowPidCommand() {}

void ShowPidCommand::execute() {
    std::cout << "smash pid is " << getpid() << std::endl;
}

// ================= GetCurrDirCommand Class ===============
GetCurrDirCommand::GetCurrDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}
GetCurrDirCommand::~GetCurrDirCommand() {}

void GetCurrDirCommand::execute() {
    char cwd[COMMAND_MAX_LENGTH];
    if (getcwd(cwd, sizeof(cwd)) == nullptr) {
        perror("smash error: getcwd failed");
    }
    else {
        std::cout << cwd << std::endl;
    }
}

// ================= ChangeDirCommand Class ===============
ChangeDirCommand::ChangeDirCommand(const char* cmd_line, std::string& lastWorkingDir) : BuiltInCommand(cmd_line), m_lastWorkingDir(lastWorkingDir) {}
ChangeDirCommand::~ChangeDirCommand() {}

void ChangeDirCommand::execute() {
    char* args[COMMAND_MAX_ARGS];
    int argCount = _parseCommandLine(m_cmd_line, args);

    if (argCount == 1) {
        // No argument provided, do nothing
        for (int i = 0; i < argCount; ++i) {
            free(args[i]);
        }
        return;
    }

    if (argCount > 2) {
        std::cerr << "smash error: cd: too many arguments" << std::endl;
        for (int i = 0; i < argCount; ++i) {
            free(args[i]);
        }
        return;
    }

    std::string targetDir = args[1];
    free(args[0]);
    free(args[1]);

    if (targetDir == "-") {
        if (m_lastWorkingDir.empty()) {
            std::cerr << "smash error: cd: OLDPWD not set" << std::endl;
            return;
        }
        targetDir = m_lastWorkingDir;
    }

    char cwd[COMMAND_MAX_LENGTH];
    if (getcwd(cwd, sizeof(cwd)) == nullptr) {
        perror("smash error: getcwd failed");
        return;
    }

    if (chdir(targetDir.c_str()) == -1) {
        perror("smash error: chdir failed");
    }
    else {
        m_lastWorkingDir = std::string(cwd);
    }
}

// ================= JobsCommand Class ===============
JobsCommand::JobsCommand(const char* cmd_line, JobsList& jobsList) : BuiltInCommand(cmd_line), m_jobsList(jobsList) {}
JobsCommand::~JobsCommand() {}

void JobsCommand::execute() {
    m_jobsList.removeFinishedJobs(); // Remove completed jobs
    m_jobsList.printJobsList();      // Print remaining jobs
}

// ==================== ExternalCommand Class ==============
ExternalCommand::ExternalCommand(const char* cmd_line) : Command(cmd_line) {
    m_isBackground = (m_cmdLine.back() == '&');
    if (m_isBackground) {
        m_cmdLine.pop_back();
    }
}

ExternalCommand::~ExternalCommand() {}

void ExternalCommand::execute() {
    // Check if the command is complex (contains * or ?)
    bool isComplex = (m_cmdLine.find('*') != std::string::npos || m_cmdLine.find('?') != std::string::npos);

    pid_t pid = fork();
    if (pid == 0) { // Child process
        setpgrp();

        if (isComplex) {
            // Complex command: Use bash -c
            char* bashArgs[] = { (char*)"/bin/bash", (char*)"-c", (char*)m_cmdLine.c_str(), nullptr };
            execv("/bin/bash", bashArgs);
        }
        else {
            // Simple command: Parse and execute directly
            char* args[COMMAND_MAX_ARGS];
            _parseCommandLine(m_cmdLine.c_str(), args); // No need to store argCount
            execvp(args[0], args);
        }

        // If exec fails, print an error and exit
        perror("smash error: exec failed");
        exit(1);
    }
    else if (pid > 0) { // Parent process
        SmallShell& shell = SmallShell::getInstance();
        if (m_isBackground) {
            // Add to jobs list for background commands
            shell.getJobsList().addJob(pid, m_cmdLine, false);
            std::cout << "Background job started with PID: " << pid << std::endl;
        }
        else {
            // Wait for foreground commands
            shell.setForegroundJob(pid, m_cmdLine);
            waitpid(pid, nullptr, 0);
            shell.clearForegroundJob();
        }
    }
    else {
        perror("smash error: fork failed");
    }
}

// ================= JobsList::JobEntry Class ==============
JobsList::JobEntry::JobEntry(int jobId, const std::string& command, int pid, bool isStopped)
    : m_jobId(jobId), m_command(command), m_pid(pid), m_isStopped(isStopped) {}

// ======================= JobsList Class ===================
JobsList::JobsList() : m_lastJobId(0) {}
JobsList::~JobsList() {}

void JobsList::addJob(int pid, const std::string& command, bool isStopped) {
    // Ensure finished jobs are removed
    removeFinishedJobs();

    // Add the new job with a unique ID
    m_jobs.emplace_back(++m_lastJobId, command, pid, isStopped);
}

void JobsList::removeFinishedJobs() {
    for (auto it = m_jobs.begin(); it != m_jobs.end();) {
        int status;
        if (waitpid(it->m_pid, &status, WNOHANG) > 0) {
            it = m_jobs.erase(it);
        }
        else {
            ++it;
        }
    }
}

void JobsList::printJobsList() const {
    for (const auto& job : m_jobs) {
        std::cout << "[" << job.m_jobId << "] " << job.m_command
            << (job.m_isStopped ? " (stopped)" : "") << std::endl;
    }
}

void JobsList::killAllJobs() {
    for (const auto& job : m_jobs) {
        kill(job.m_pid, SIGKILL);
        std::cout << job.m_pid << ": " << job.m_command << std::endl;
    }
    m_jobs.clear();
}

// ====================== SmallShell Class ==================
SmallShell::SmallShell() : m_prompt("smash"), m_foregroundPid(-1), m_foregroundCommand("") {}
SmallShell::~SmallShell() {}

SmallShell& SmallShell::getInstance() {
    static SmallShell instance;
    return instance;
}

Command* SmallShell::CreateCommand(const char* cmd_line) {
    std::string cmd_s = _trim(std::string(cmd_line));

    std::string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" "));

    if (firstWord == "chprompt") {
        return new ChangePromptCommand(cmd_line, m_prompt);
    }
    else if (firstWord == "pwd") {
        return new GetCurrDirCommand(cmd_line);
    }
    else if (firstWord == "showpid") {
        return new ShowPidCommand(cmd_line);
    }
    else if (firstWord == "cd") {
        return new ChangeDirCommand(cmd_line, m_lastWorkingDir);
    }
    else if (firstWord == "jobs") {
        return new JobsCommand(cmd_line, m_jobsList);
    }
    else {
        return new ExternalCommand(cmd_line); // Default to external command
    }
}

void SmallShell::executeCommand(const char* cmd_line) {
    Command* cmd = CreateCommand(cmd_line);
    if (cmd) {
        cmd->execute();
        delete cmd;
    }
}

std::string SmallShell::getPrompt() const {
    return m_prompt + "> ";
}

void SmallShell::setPrompt(const std::string& prompt) {
    m_prompt = prompt;
}

void SmallShell::setForegroundJob(int pid, const std::string& command) {
    m_foregroundPid = pid;
    m_foregroundCommand = command;
}

void SmallShell::clearForegroundJob() {
    m_foregroundPid = -1;
    m_foregroundCommand.clear();
}

int SmallShell::getForegroundPid() const {
    return m_foregroundPid;
}

std::string SmallShell::getForegroundCommand() const {
    return m_foregroundCommand;
}

JobsList& SmallShell::getJobsList() {
    return m_jobsList;
}