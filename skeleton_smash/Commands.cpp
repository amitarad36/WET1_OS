
#include <unistd.h>
#include <sys/wait.h>
#include <iostream>
#include <cstring>
#include <csignal>
#include <string.h>
#include <vector>
#include <sstream>
#include <iomanip>
#include "Commands.h"

using namespace std;
const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;
#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

string _ltrim(const std::string& s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string& s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string& s) {
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char* cmd_line, char** args) {
    FUNC_ENTRY()
        int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;) {
        args[i] = (char*)malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;
    FUNC_EXIT()
}

bool _isBackgroundComamnd(const char* cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char* cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

// ==========================================================
// ======================= Command Class ====================
// ==========================================================
Command::Command(const char* cmd_line) : m_cmd_line(cmd_line) {}
Command::~Command() {}

std::string Command::getCommandLine() const {
    return std::string(m_cmd_line);
}

// ==========================================================
// ==================== BuiltInCommand Class ===============
// ==========================================================
BuiltInCommand::BuiltInCommand(const char* cmd_line) : Command(cmd_line) {}
BuiltInCommand::~BuiltInCommand() {}

// ==========================================================
// ================= ChangePromptCommand Class ==============
// ==========================================================
ChangePromptCommand::ChangePromptCommand(const char* cmd_line, std::string& prompt) 
    : BuiltInCommand(cmd_line), m_prompt(prompt) {}

ChangePromptCommand::~ChangePromptCommand() {}

void ChangePromptCommand::execute() {
    char* token = strtok((char*)m_cmd_line, " ");
    token = strtok(nullptr, " ");
    if (token) {
        m_prompt = std::string(token);
    } else {
        m_prompt = "smash";
    }
}

// ==========================================================
// ==================== ExternalCommand Class ==============
// ==========================================================
ExternalCommand::ExternalCommand(const char* cmd_line) : Command(cmd_line) {
    m_isBackground = (m_cmdLine.back() == '&');
    if (m_isBackground) {
        m_cmdLine.pop_back();
    }
}
ExternalCommand::~ExternalCommand() {}

void ExternalCommand::execute() {
    pid_t pid = fork();
    if (pid == 0) { // Child process
        setpgrp();
        execlp(m_cmdLine.c_str(), m_cmdLine.c_str(), (char*)NULL);
        perror("Exec failed");
        exit(1);
    } else if (pid > 0) { // Parent process
        SmallShell& shell = SmallShell::getInstance();
        if (m_isBackground) {
            shell.getJobsList().addJob(pid, m_cmdLine, false);
            std::cout << "Background job started with PID: " << pid << std::endl;
        } else {
            shell.setForegroundJob(pid, m_cmdLine);
            waitpid(pid, nullptr, 0);
            shell.clearForegroundJob();
        }
    } else {
        perror("smash error: fork failed");
    }
}

// ==========================================================
// ================= JobsList::JobEntry Class ==============
// ==========================================================
JobsList::JobEntry::JobEntry(int jobId, const std::string& command, int pid, bool isStopped)
    : m_jobId(jobId), m_command(command), m_pid(pid), m_isStopped(isStopped) {}

// ==========================================================
// ======================= JobsList Class ===================
// ==========================================================
JobsList::JobsList() : m_lastJobId(0) {}
JobsList::~JobsList() {}

void JobsList::addJob(int pid, const std::string& command, bool isStopped) {
    removeFinishedJobs();
    m_jobs.emplace_back(++m_lastJobId, command, pid, isStopped);
}

void JobsList::removeFinishedJobs() {
    for (auto it = m_jobs.begin(); it != m_jobs.end();) {
        int status;
        if (waitpid(it->m_pid, &status, WNOHANG) > 0) {
            it = m_jobs.erase(it);
        } else {
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

// ==========================================================
// ====================== SmallShell Class ==================
// ==========================================================
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
    } else {
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
