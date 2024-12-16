#include "Commands.h"
#include <unistd.h>
#include <sys/wait.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string.h>
#include <cstdlib>
#include <stdexcept>


// Utility Functions
std::string _trim(const std::string& str) {
	const std::string WHITESPACE = " \n\r\t\f\v";
	size_t start = str.find_first_not_of(WHITESPACE);
	size_t end = str.find_last_not_of(WHITESPACE);
	return (start == std::string::npos) ? "" : str.substr(start, end - start + 1);
}
int _parseCommandLine(const std::string& cmd_line, char** args) {
	if (cmd_line.empty()) {
		args[0] = nullptr; // Null-terminate in case of empty input
		return 0;
	}

	int i = 0;
	std::istringstream iss(_trim(cmd_line)); // Use _trim to clean input
	for (std::string token; iss >> token;) {
		args[i] = (char*)malloc(token.length() + 1); // Allocate memory for each token
		if (!args[i]) {
			perror("smash error: malloc failed");
			exit(1);
		}
		strcpy(args[i], token.c_str()); // Copy token into allocated memory
		++i;

		if (i >= COMMAND_MAX_ARGS) {
			std::cerr << "smash error: too many arguments" << std::endl;
			break;
		}
	}
	args[i] = nullptr; // Null-terminate the argument list
	return i;          // Return the number of arguments parsed
}


// Command Class
Command::Command(const char* cmd_line) : cmdLine(cmd_line), processId(-1), isBackground(false) {
	std::istringstream iss(_trim(cmd_line));
	for (std::string token; iss >> token;) {
		cmdSegments.push_back(token);
	}
	if (!cmdSegments.empty() && cmdSegments.back() == "&") {
		isBackground = true;
		cmdSegments.pop_back();
	}
}

Command::~Command() {}
int Command::getProcessId() const { return processId; }
void Command::setProcessId(int pid) { processId = pid; }
std::string Command::getAlias() const { return alias; }
void Command::setAlias(const std::string& aliasCommand) { alias = aliasCommand; }
std::string Command::getPath() const { return fileRedirect; }
void Command::setPath(const std::string& path) { fileRedirect = path; }
std::string Command::getCommandLine() const {
	if (!cmdLine.empty()) {
		return cmdLine; // Return the original command line if available
	}

	// Reconstruct the command line from segments
	std::ostringstream oss;
	for (size_t i = 0; i < cmdSegments.size(); ++i) {
		oss << cmdSegments[i];
		if (i < cmdSegments.size() - 1) {
			oss << " ";
		}
	}
	if (isBackground) {
		oss << " &";
	}
	return oss.str();
}


// BuiltInCommand Class
BuiltInCommand::BuiltInCommand(const char* cmd_line) : Command(cmd_line) {}
BuiltInCommand::~BuiltInCommand() {}


// ExternalCommand Class
ExternalCommand::ExternalCommand(const char* cmd_line) : Command(cmd_line) {}
void ExternalCommand::execute() {
	pid_t pid = fork();
	if (pid == 0) { // Child process
		setpgrp();
		char* args[COMMAND_MAX_ARGS];
		for (size_t i = 0; i < cmdSegments.size(); ++i) {
			args[i] = strdup(cmdSegments[i].c_str());
		}
		args[cmdSegments.size()] = nullptr;
		execvp(args[0], args);
		perror("smash error: execvp failed");
		exit(1);
	}
	else if (pid > 0) { // Parent process
		SmallShell& shell = SmallShell::getInstance();
		if (isBackground) {
			shell.getJobsList().addJob(getCommandLine(), pid, false);
			std::cout << "Background job started with PID: " << pid << std::endl;
		}
		else {
			shell.setForegroundJob(pid, getCommandLine());
			waitpid(pid, nullptr, 0);
			shell.clearForegroundJob();
		}
	}
	else {
		perror("smash error: fork failed");
	}
}


// ChangePromptCommand Class
ChangePromptCommand::ChangePromptCommand(const char* cmd_line, std::string& prompt)
	: BuiltInCommand(cmd_line), prompt(prompt) {}
ChangePromptCommand::~ChangePromptCommand() {}
void ChangePromptCommand::execute() {
	char* args[COMMAND_MAX_ARGS];
	int argc = _parseCommandLine(cmdLine, args);

	if (argc == 1) {
		// No argument provided, reset to "smash"
		prompt = "smash";
	}
	else {
		// Update the prompt with the first argument
		prompt = args[1];
	}

	// Free allocated memory
	for (int i = 0; i < argc; ++i) {
		free(args[i]);
	}
}


// ChangeDirCommand Class
ChangeDirCommand::ChangeDirCommand(const char* cmd_line, std::string& lastDir)
	: BuiltInCommand(cmd_line), lastWorkingDir(lastDir) {}
ChangeDirCommand::~ChangeDirCommand() {}
void ChangeDirCommand::execute() {
	if (cmdSegments.size() != 2) {
		std::cerr << "smash error: cd: too many arguments" << std::endl;
		return;
	}
	const std::string& targetDir = cmdSegments[1];
	if (chdir(targetDir.c_str()) == -1) {
		perror("smash error: chdir failed");
	}
	else {
		lastWorkingDir = getcwd(nullptr, 0);
	}
}


// ShowPidCommand Class
ShowPidCommand::ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}
ShowPidCommand::~ShowPidCommand() {}
void ShowPidCommand::execute() {
	std::cout << "smash pid is " << getpid() << std::endl;
}


// GetCurrDirCommand Class
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


// JobsList Class
JobsList::JobsList() : lastJobId(0) {}
JobsList::~JobsList() {
	for (auto job : jobs) {
		delete job;
	}
}
void JobsList::addJob(const std::string& command, int pid, bool isStopped) {
	removeFinishedJobs(); // Clean up finished jobs
	int jobId = ++lastJobId;
	jobs.push_back(new JobEntry(jobId, pid, command, isStopped));
}
void JobsList::printJobs() const {
	for (const auto& job : jobs) {
		std::cout << "[" << job->jobId << "] " << job->command
			<< (job->isStopped ? " (stopped)" : "") << std::endl;
	}
}
void JobsList::removeFinishedJobs() {
	for (auto it = jobs.begin(); it != jobs.end();) {
		int status;
		if (waitpid((*it)->pid, &status, WNOHANG) > 0) {
			delete* it;
			it = jobs.erase(it);
		}
		else {
			++it;
		}
	}
}
JobsList::JobEntry* JobsList::getJobById(int jobId) {
	for (auto& job : jobs) {
		if (job->jobId == jobId) {
			return job;
		}
	}
	return nullptr; // Job not found
}
void JobsList::removeJobById(int jobId) {
	for (auto it = jobs.begin(); it != jobs.end(); ++it) {
		if ((*it)->jobId == jobId) {
			delete* it;
			jobs.erase(it);
			return;
		}
	}
}
void JobsList::killAllJobs() {
	for (auto& job : jobs) {
		if (kill(job->pid, SIGKILL) == -1) {
			perror("smash error: kill failed");
		}
		else {
			std::cout << job->pid << ": " << job->command << std::endl;
		}
		delete job;
	}
	jobs.clear();
}


// JobsCommand Class
JobsCommand::JobsCommand(const char* cmd_line, JobsList* jobsList) : BuiltInCommand(cmd_line), m_jobsList(jobsList) {}
JobsCommand::~JobsCommand() {}
void JobsCommand::execute() {
	if (!m_jobsList) {
		std::cerr << "smash error: jobs list is null" << std::endl;
		return;
	}

	m_jobsList->removeFinishedJobs();
	m_jobsList->printJobs();
}


// KillCommand Class
KillCommand::KillCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobsList(jobs) {}
KillCommand::~KillCommand() {}
void KillCommand::execute() {
	char* args[COMMAND_MAX_ARGS];
	int argc = _parseCommandLine(cmdLine, args);

	if (argc != 3 || args[1][0] != '-') {
		std::cerr << "smash error: kill: invalid arguments" << std::endl;
		return;
	}

	int signal = atoi(args[1] + 1);
	int jobId = atoi(args[2]);

	JobsList::JobEntry* job = jobsList->getJobById(jobId);
	if (!job) {
		std::cerr << "smash error: kill: job-id " << jobId << " does not exist" << std::endl;
		return;
	}

	if (kill(job->pid, signal) == -1) {
		perror("smash error: kill failed");
	}
	else {
		std::cout << "signal number " << signal << " was sent to PID " << job->pid << std::endl;
	}
}


// QuitCommand Class
QuitCommand::QuitCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobsList(jobs) {}
QuitCommand::~QuitCommand() {}
void QuitCommand::execute() {
	if (strstr(cmdLine.c_str(), "kill")) {
		jobsList->killAllJobs();
	}
	exit(0);
}


// ForegroundCommand Class
ForegroundCommand::ForegroundCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobsList(jobs) {}
ForegroundCommand::~ForegroundCommand() {}
void ForegroundCommand::execute() {
	char* args[COMMAND_MAX_ARGS];
	int argc = _parseCommandLine(cmdLine, args);

	if (argc != 2) {
		std::cerr << "smash error: fg: invalid arguments" << std::endl;
		return;
	}

	int jobId = atoi(args[1]);
	JobsList::JobEntry* job = jobsList->getJobById(jobId);
	if (!job) {
		std::cerr << "smash error: fg: job-id " << jobId << " does not exist" << std::endl;
		return;
	}

	if (kill(job->pid, SIGCONT) == -1) {
		perror("smash error: fg failed");
		return;
	}

	waitpid(job->pid, nullptr, 0);
}

// BackgroundCommand Class
BackgroundCommand::BackgroundCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobsList(jobs) {}
BackgroundCommand::~BackgroundCommand() {}
void BackgroundCommand::execute() {
	char* args[COMMAND_MAX_ARGS];
	int argc = _parseCommandLine(cmdLine, args);

	if (argc != 2) {
		std::cerr << "smash error: bg: invalid arguments" << std::endl;
		return;
	}

	int jobId = atoi(args[1]);
	JobsList::JobEntry* job = jobsList->getJobById(jobId);
	if (!job) {
		std::cerr << "smash error: bg: job-id " << jobId << " does not exist" << std::endl;
		return;
	}

	if (!job->isStopped) {
		std::cerr << "smash error: bg: job-id " << jobId << " is not stopped" << std::endl;
		return;
	}

	if (kill(job->pid, SIGCONT) == -1) {
		perror("smash error: bg failed");
		return;
	}

	job->isStopped = false;
	std::cout << job->command << " resumed" << std::endl;
}

// AliasCommand Class
AliasCommand::AliasCommand(const char* cmd_line, std::map<std::string, std::string>& aliasMap)
	: BuiltInCommand(cmd_line), aliasMap(aliasMap) {}
AliasCommand::~AliasCommand() {}
void AliasCommand::execute() {
	char* args[COMMAND_MAX_ARGS];
	int argc = _parseCommandLine(cmdLine, args);

	if (argc != 3) {
		std::cerr << "smash error: alias: invalid arguments" << std::endl;
		return;
	}

	std::string aliasName = args[1];
	std::string aliasCommand = args[2];

	// Prevent alias loops
	if (aliasCommand == aliasName) {
		std::cerr << "smash error: alias: alias loop detected" << std::endl;
		return;
	}

	aliasMap[aliasName] = aliasCommand;
	std::cout << "Alias added: " << aliasName << " -> " << aliasCommand << std::endl;

	for (int i = 0; i < argc; ++i) {
		free(args[i]);
	}
}

// UnaliasCommand Class
UnaliasCommand::UnaliasCommand(const char* cmd_line, std::map<std::string, std::string>& aliasMap)
	: BuiltInCommand(cmd_line), aliasMap(aliasMap) {}
UnaliasCommand::~UnaliasCommand() {}
void UnaliasCommand::execute() {
	char* args[COMMAND_MAX_ARGS];
	int argc = _parseCommandLine(cmdLine, args);

	if (argc != 2) {
		std::cerr << "smash error: unalias: invalid arguments" << std::endl;
		return;
	}

	std::string aliasName = args[1];

	if (aliasMap.erase(aliasName) == 0) {
		std::cerr << "smash error: unalias: alias \"" << aliasName << "\" does not exist" << std::endl;
	}
	else {
		std::cout << "Alias \"" << aliasName << "\" removed" << std::endl;
	}

	for (int i = 0; i < argc; ++i) {
		free(args[i]);
	}
}

// SmallShell Class
SmallShell::SmallShell()
	: prompt("smash"), lastWorkingDir(""), foregroundPid(-1), foregroundCommand("") {}
SmallShell::~SmallShell() {}
SmallShell& SmallShell::getInstance() {
	static SmallShell instance;
	return instance;
}
Command* SmallShell::createCommand(const char* cmd_line) {
	std::string cmd_s = _trim(std::string(cmd_line));
	std::string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" "));

	if (firstWord == "chprompt") {
		return new ChangePromptCommand(cmd_line, prompt);
	}
	else if (firstWord == "pwd") {
		return new GetCurrDirCommand(cmd_line);
	}
	else if (firstWord == "showpid") {
		return new ShowPidCommand(cmd_line);
	}
	else if (firstWord == "cd") {
		return new ChangeDirCommand(cmd_line, lastWorkingDir);
	}
	else if (firstWord == "jobs") {
		return new JobsCommand(cmd_line, &jobs);
	}
	else if (firstWord == "kill") {
		return new KillCommand(cmd_line, &jobs);
	}
	else if (firstWord == "quit") {
		return new QuitCommand(cmd_line, &jobs);
	}
	else if (firstWord == "fg") {
		return new ForegroundCommand(cmd_line, &jobs);
	}
	else if (firstWord == "bg") {
		return new BackgroundCommand(cmd_line, &jobs);
	}
	else {
		return new ExternalCommand(cmd_line);
	}
}
void SmallShell::executeCommand(const char* cmd_line) {
	Command* cmd = createCommand(cmd_line);
	if (cmd) {
		cmd->execute();
		delete cmd;
	}
}
std::string SmallShell::getPrompt() const {
	return prompt + "> ";
}
void SmallShell::setPrompt(const std::string& newPrompt) {
	prompt = newPrompt;
}
const JobsList& SmallShell::getJobsList() const {
	return jobs;
}
void SmallShell::updateWorkingDirectory(const std::string& newDir) {
	if (!lastWorkingDir.empty()) {
		prevWorkingDir = lastWorkingDir; // Save current as previous
	}
	lastWorkingDir = newDir; // Update to the new directory
}
void SmallShell::setForegroundJob(int pid, const std::string& command) {
	foregroundPid = pid;
	foregroundCommand = command;
}
void SmallShell::clearForegroundJob() {
	foregroundPid = -1;
	foregroundCommand.clear();
}
int SmallShell::getForegroundPid() const {
	return foregroundPid;
}
std::string SmallShell::getForegroundCommand() const {
	return foregroundCommand;
}
void SmallShell::setAlias(const std::string& aliasName, const std::string& aliasCommand) {
	if (aliasCommand == aliasName) {
		std::cerr << "smash error: alias: alias loop detected" << std::endl;
		return;
	}
	aliasMap[aliasName] = aliasCommand;
}
void SmallShell::removeAlias(const std::string& aliasName) {
	if (aliasMap.erase(aliasName) == 0) {
		std::cerr << "smash error: unalias: alias \"" << aliasName << "\" does not exist" << std::endl;
	}
}
std::string SmallShell::getAlias(const std::string& aliasName) const {
	auto it = aliasMap.find(aliasName);
	if (it != aliasMap.end()) {
		return it->second;
	}
	return ""; // Alias not found
}
void SmallShell::printAliases() const {
	for (const auto& alias : aliasMap) {
		std::cout << alias.first << " -> " << alias.second << std::endl;
	}
}

