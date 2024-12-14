#include <iostream>
#include <dirent.h>
#include <regex>
#include <cstring>
#include <sstream>
#include <sys/wait.h>
#include <sys/types.h>
#include <string.h>
#include <sys/syscall.h>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <grp.h>
#include <iomanip>
#include "Commands.h"
#include <sys/stat.h>
#include <cerrno>
#include <sstream>
#include <thread>
#include <algorithm>
#include <chrono>
#include <pwd.h>

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

void delBackSign(std::string& cmd_line) {
	cmd_line = _trim(cmd_line);
	if (!cmd_line.empty() && cmd_line.back() == '&') {
		cmd_line.pop_back();
	}
}

void vecFromLine(const char* cmd_line, std::vector<std::string>& line) {
	std::string temp = cmd_line;
	std::string add;
	bool inWord = false;

	for (char ch : temp) {
		if (ch != ' ') {
			add += ch;
			inWord = true;
		}
		else if (inWord) {
			line.push_back(add);
			add.clear();
			inWord = false;
		}
	}

	if (inWord) {
		line.push_back(add);
	}
}

// ================= Command Base Class =================

Command::Command(const char* cmd_line) : m_cmd_line(cmd_line), m_command_seg() {
	vecFromLine(cmd_line, m_command_seg);
}

Command::~Command() {}

string Command::getCommandLine() {
	return string(m_cmd_line);
}

// ================= BuiltInCommand Class =================

BuiltInCommand::BuiltInCommand(const char* cmd_line) : Command(cmd_line) {}

BuiltInCommand::~BuiltInCommand() {}

// ================= ExternalCommand Class =================

ExternalCommand::ExternalCommand(const char* cmd_line) : Command(cmd_line) {}

ExternalCommand::~ExternalCommand() {}

void ExternalCommand::execute() {
	pid_t pid = fork();
	if (pid == 0) { // Child process
		setpgrp();
		const char* args[] = { "/bin/bash", "-c", m_cmd_line, nullptr };
		execv("/bin/bash", (char* const*)args);
		perror("smash error: execv failed");
		exit(1);
	}
	else if (pid > 0) { // Parent process
		waitpid(pid, nullptr, WUNTRACED);
	}
	else {
		perror("smash error: fork failed");
	}
}

// ================= ChangePromptCommand Class =================

ChangePromptCommand::ChangePromptCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

ChangePromptCommand::~ChangePromptCommand() {}

void ChangePromptCommand::execute() {
	SmallShell& shell = SmallShell::getInstance();
	char* token = strtok((char*)m_cmd_line, " ");
	token = strtok(nullptr, " ");
	if (token) {
		shell.setPrompt(string(token));
	}
	else {
		shell.setPrompt("smash");
	}
}

// ================= PipeCommand Class =================

PipeCommand::PipeCommand(const char* cmd_line) : Command(cmd_line) {}

PipeCommand::~PipeCommand() {}

void PipeCommand::execute() {
	cout << "PipeCommand not implemented yet." << endl;
}

// ================= RedirectionCommand Class =================

RedirectionCommand::RedirectionCommand(const char* cmd_line) : Command(cmd_line) {}

RedirectionCommand::~RedirectionCommand() {}

void RedirectionCommand::execute() {
	cout << "RedirectionCommand not implemented yet." << endl;
}

// ================= ChangeDirCommand Class =================

ChangeDirCommand::ChangeDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

ChangeDirCommand::~ChangeDirCommand() {}

void ChangeDirCommand::execute() {
	if (!m_command_seg.empty() && m_command_seg.back() == "&") {
		m_command_seg.pop_back();
	}

	if (m_command_seg.size() == 1) {
		SmallShell::getInstance().changePwd(SmallShell::getInstance().getSmashPwd());
	}
	else if (m_command_seg.size() == 2) {
		std::string path = m_command_seg[1];
		delBackSign(path);

		if (path == "-") {
			const std::string& prevPwd = SmallShell::getInstance().getPrevPwd();
			if (prevPwd.empty()) {
				std::cerr << "smash error: cd: OLDPWD not set" << std::endl;
			}
			else {
				if (chdir(prevPwd.c_str()) != -1) {
					SmallShell::getInstance().changePwd(prevPwd);
				}
				else {
					perror("smash error: chdir failed");
				}
			}
		}
		else {
			if (chdir(path.c_str()) != -1) {
				SmallShell::getInstance().changePwd(path);
			}
			else {
				perror("smash error: chdir failed");
			}
		}
	}
	else {
		// Too many arguments
		std::cerr << "smash error: cd: too many arguments" << std::endl;
	}
}

// ================= GetCurrDirCommand Class =================

GetCurrDirCommand::GetCurrDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

GetCurrDirCommand::~GetCurrDirCommand() {}

void GetCurrDirCommand::execute() {
	cout << SmallShell::getInstance().getSmashPwd() << endl;
}

// ================= ShowPidCommand Class =================

ShowPidCommand::ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

ShowPidCommand::~ShowPidCommand() {}

void ShowPidCommand::execute() {
	cout << "smash pid is " << SmallShell::getInstance().getSmashPid() << endl;
}

// ================= QuitCommand Class =================

QuitCommand::QuitCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line) {}

QuitCommand::~QuitCommand() {}

void QuitCommand::execute() {}

// ================= JobsList Class =================

JobsList::JobsList() {
	// Constructor implementation (empty)
}

JobsList::~JobsList() {
	// Destructor implementation (empty)
}

void JobsList::addJob(Command* cmd, bool isStopped) {
	// Method implementation (empty)
}

void JobsList::printJobsList() {
	// Method implementation (empty)
}

void JobsList::killAllJobs() {
	// Method implementation (empty)
}

void JobsList::removeFinishedJobs() {
	// Method implementation (empty)
}

JobsList::JobEntry* JobsList::getJobById(int jobId) {
	// Method implementation (empty)
	return nullptr;
}

void JobsList::removeJobById(int jobId) {
	// Method implementation (empty)
}

JobsList::JobEntry* JobsList::getLastJob(int* lastJobId) {
	// Method implementation (empty)
	return nullptr;
}

JobsList::JobEntry* JobsList::getLastStoppedJob(int* jobId) {
	// Method implementation (empty)
	return nullptr;
}

// ================= JobsCommand Class =================

JobsCommand::JobsCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}

JobsCommand::~JobsCommand() {}

void JobsCommand::execute() {
	jobs->printJobsList();
}

// ================= KillCommand Class =================

KillCommand::KillCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line) {}

KillCommand::~KillCommand() {}

void KillCommand::execute() {}

// ================= ForegroundCommand Class =================

ForegroundCommand::ForegroundCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line) {}

ForegroundCommand::~ForegroundCommand() {}

void ForegroundCommand::execute() {}

// ================= ListDirCommand Class =================

ListDirCommand::ListDirCommand(const char* cmd_line) : Command(cmd_line) {}

ListDirCommand::~ListDirCommand() {}

void ListDirCommand::execute() {}

// ================= WhoAmICommand Class =================

WhoAmICommand::WhoAmICommand(const char* cmd_line) : Command(cmd_line) {}

WhoAmICommand::~WhoAmICommand() {}

void WhoAmICommand::execute() {}

// ================= aliasCommand Class =================

aliasCommand::aliasCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

aliasCommand::~aliasCommand() {}

void aliasCommand::execute() {}

// ================= NetInfo Class =================

NetInfo::NetInfo(const char* cmd_line) : Command(cmd_line) {}

NetInfo::~NetInfo() {}

void NetInfo::execute() {}

// ================= unaliasCommand Class =================

unaliasCommand::unaliasCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}

unaliasCommand::~unaliasCommand() {}

void unaliasCommand::execute() {}

// ================= SmallShell Singleton =================

SmallShell::SmallShell() : m_prompt("smash"), m_prevDir(""), m_pid(getpid()), m_currDir(getSmashPwd()){}

SmallShell::~SmallShell() {
	if (m_lastPwd) {
		free(m_lastPwd); // Free memory allocated for lastPwd
	}
}

SmallShell& SmallShell::getInstance() {
	static SmallShell instance; // Instantiated once on first use
	return instance;
}

Command* SmallShell::CreateCommand(const char* cmd_line) {
	std::string cmd_s = std::string(cmd_line);
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
	else if (firstWord == "jobs") {
		return new JobsCommand(cmd_line, &jobsList);
	}
	else if (firstWord == "kill") {
		return new KillCommand(cmd_line, &jobsList);
	}
	else if (firstWord == "fg") {
		return new ForegroundCommand(cmd_line, &jobsList);
	}
	else if (firstWord == "quit") {
		return new QuitCommand(cmd_line, &jobsList);
	}
	else if (firstWord == "netinfo") {
		return new NetInfo(cmd_line);
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

	return m_prompt + ">";
}

void SmallShell::setPrompt(const std::string& newPrompt) {
	m_prompt = newPrompt;
}

std::string SmallShell::getPrevPwd() const {
	return m_prevDir;
}

JobsList& SmallShell::getJobsList() {
	return jobsList;
}

int SmallShell::getSmashPid() {
	return m_pid;
}

std::string SmallShell::getSmashPwd()
{
	char path[COMMAND_MAX_LENGTH];
	getcwd(path, COMMAND_MAX_LENGTH);
	return std::string(path);
}

void SmallShell::changePwd(std::string pwd) {
	std::string curr = m_currDir;
	m_currDir = pwd;
	m_prevDir = curr;
}