#include "Commands.h"
#include <unistd.h>
#include <sys/wait.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string.h>
#include <cstdlib>
#include <stdexcept>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>


#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

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
		// Check if the token ends with '&'
		if (token.back() == '&' && token.length() > 1) {
			// Separate the `&` from the token
			token.pop_back(); // Remove '&' from the token
			args[i] = (char*)malloc(token.length() + 1); // Allocate memory
			if (!args[i]) {
				perror("smash error: malloc failed");
				exit(1);
			}
			strcpy(args[i], token.c_str());
			++i;

			// Add `&` as a separate argument
			args[i] = (char*)malloc(2); // Allocate memory for "&"
			if (!args[i]) {
				perror("smash error: malloc failed");
				exit(1);
			}
			strcpy(args[i], "&");
			++i;
		}
		else {
			// Normal token handling
			args[i] = (char*)malloc(token.length() + 1); // Allocate memory
			if (!args[i]) {
				perror("smash error: malloc failed");
				exit(1);
			}
			strcpy(args[i], token.c_str());
			++i;
		}

		if (i >= COMMAND_MAX_ARGS) {
			std::cerr << "smash error: too many arguments" << std::endl;
			break;
		}
	}
	args[i] = nullptr; // Null-terminate the argument list
	return i;          // Return the number of arguments parsed
}
bool isDirectory(const std::string& path) {
	struct stat statbuf;
	if (stat(path.c_str(), &statbuf) != 0) {
		return false;
	}
	return S_ISDIR(statbuf.st_mode);
}
void _trimAmp(std::string& cmd_line) {
	cmd_line = _trim(cmd_line); // Remove leading/trailing spaces
	if (!cmd_line.empty() && cmd_line.back() == '&') {
		cmd_line.pop_back(); // Remove the `&` character
		cmd_line = _trim(cmd_line); // Clean up trailing spaces again
	}
}


// Command Class
Command::Command(const char* cmd_line) : cmdLine(cmd_line), processId(-1), isBackground(false) {
	char* args[COMMAND_MAX_ARGS];
	int argc = _parseCommandLine(cmd_line, args);

	for (int i = 0; i < argc; ++i) {
		cmdSegments.push_back(args[i]);
		free(args[i]); // Free the dynamically allocated memory
	}

	if (!cmdSegments.empty() && cmdSegments.back() == "&") {
		isBackground = true;
		cmdSegments.pop_back(); // Remove the `&` from the segments
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
ChangeDirCommand::ChangeDirCommand(const char* cmd_line)
	: BuiltInCommand(cmd_line) {}
ChangeDirCommand::~ChangeDirCommand() {}
void ChangeDirCommand::execute() {
	SmallShell& shell = SmallShell::getInstance();

	// No arguments: Do nothing
	if (cmdSegments.size() == 1) {
		return;
	}

	// Too many arguments: Print error and return
	if (cmdSegments.size() > 2) {
		std::cerr << "smash error: cd: too many arguments" << std::endl;
		return;
	}

	const std::string& targetDir = cmdSegments[1];
	char* currentDir = getcwd(nullptr, 0); // Get the current working directory
	if (!currentDir) {
		perror("smash error: getcwd failed");
		return;
	}

	// Handle "cd -": Change to the last working directory
	if (targetDir == "-") {
		if (shell.getLastWorkingDir().empty()) {
			std::cerr << "smash error: cd: OLDPWD not set" << std::endl;
			free(currentDir);
			return;
		}
		if (chdir(shell.getLastWorkingDir().c_str()) == -1) {
			perror("smash error: chdir failed");
		}
		else {
			shell.setLastWorkingDir(currentDir); // Update lastWorkingDir with the previous directory
		}
		free(currentDir);
		return;
	}

	// Handle "cd ..": Move up one directory
	if (targetDir == "..") {
		if (chdir("..") == -1) {
			perror("smash error: chdir failed");
		}
		else {
			shell.setLastWorkingDir(currentDir); // Update lastWorkingDir
		}
		free(currentDir);
		return;
	}

	// Handle regular path: Change directory
	if (chdir(targetDir.c_str()) == -1) {
		perror("smash error: chdir failed");
	}
	else {
		shell.setLastWorkingDir(currentDir); // Update lastWorkingDir
	}
	free(currentDir);
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
int JobsList::size() const {
	return jobs.size();
}
const std::list<JobsList::JobEntry*>& JobsList::getJobs() const {
	return jobs;
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

	// Check for valid arguments: exactly 3 arguments and the second argument starts with '-'
	if (argc != 3 || args[1][0] != '-') {
		std::cerr << "smash error: kill: invalid arguments" << std::endl;
		for (int i = 0; i < argc; ++i) {
			free(args[i]);
		}
		return;
	}

	// Parse the signal number and job ID
	int signal = atoi(args[1] + 1);  // Skip the '-' character
	int jobId = atoi(args[2]);

	// Find the job in the JobsList
	JobsList::JobEntry* job = jobsList->getJobById(jobId);
	if (!job) {
		std::cerr << "smash error: kill: job-id " << jobId << " does not exist" << std::endl;
		for (int i = 0; i < argc; ++i) {
			free(args[i]);
		}
		return;
	}

	// Send the signal to the process
	if (kill(job->pid, signal) == -1) {
		perror("smash error: kill failed");
	}
	else {
		std::cout << "signal number " << signal << " was sent to pid " << job->pid << std::endl;
	}

	// Free allocated memory
	for (int i = 0; i < argc; ++i) {
		free(args[i]);
	}
}


// QuitCommand Class
QuitCommand::QuitCommand(const char* cmd_line, JobsList* jobs)
	: BuiltInCommand(cmd_line), jobsList(jobs) {}
QuitCommand::~QuitCommand() {}
void QuitCommand::execute() {
	JobsList& jobsList = SmallShell::getInstance().getJobsList();

	if (strstr(cmdLine.c_str(), "kill")) {
		std::cout << "smash: sending SIGKILL signal to " << jobsList.getJobs().size() << " jobs:" << std::endl;
		jobsList.killAllJobs();
	}
	exit(0);
}


// ForegroundCommand Class
ForegroundCommand::ForegroundCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobsList(jobs) {}
ForegroundCommand::~ForegroundCommand() {}
void ForegroundCommand::execute() {
	char* args[COMMAND_MAX_ARGS];
	int argc = _parseCommandLine(cmdLine, args);

	JobsList::JobEntry* job = nullptr;

	// Case 1: No arguments provided
	if (argc == 1) {
		// Get the job with the highest job ID
		job = jobsList->getJobById(jobsList->size());
		if (!job) {
			std::cerr << "smash error: fg: jobs list is empty" << std::endl;
			return;
		}
	}
	// Case 2: One argument provided
	else if (argc == 2) {
		int jobId = atoi(args[1]);
		if (jobId <= 0) {
			std::cerr << "smash error: fg: invalid arguments" << std::endl;
			return;
		}

		// Try to find the job with the given job ID
		job = jobsList->getJobById(jobId);
		if (!job) {
			std::cerr << "smash error: fg: job-id " << jobId << " does not exist" << std::endl;
			return;
		}
	}
	// Case 3: Invalid number of arguments
	else {
		std::cerr << "smash error: fg: invalid arguments" << std::endl;
		return;
	}

	// Print the job's command and PID
	std::cout << job->command << " " << job->pid << std::endl;

	// Send SIGCONT to the job to resume it if stopped
	if (kill(job->pid, SIGCONT) == -1) {
		perror("smash error: fg failed");
		return;
	}

	// Set the job as the foreground job in the shell
	SmallShell& shell = SmallShell::getInstance();
	shell.setForegroundJob(job->pid, job->command);

	// Wait for the job to finish
	if (waitpid(job->pid, nullptr, WUNTRACED) == -1) {
		perror("smash error: waitpid failed");
	}

	// Clear the foreground job in the shell
	shell.clearForegroundJob();

	// Remove the job from the jobs list after bringing it to the foreground
	jobsList->removeJobById(job->jobId);

	// Free allocated memory
	for (int i = 0; i < argc; ++i) {
		free(args[i]);
	}
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


// RedirectionCommand Class
RedirectionCommand::RedirectionCommand(const char* cmd_line)
	: BuiltInCommand(cmd_line), m_isAppend(false) {
	// Parse the command line for redirection
	size_t redirectionPos = cmdLine.find('>');
	if (redirectionPos != std::string::npos) {
		m_isAppend = (cmdLine[redirectionPos + 1] == '>'); // Check if it's >>
		size_t fileStart = m_isAppend ? redirectionPos + 2 : redirectionPos + 1;

		// Trim and extract the file path
		fileRedirect = _trim(cmdLine.substr(fileStart));

		// Remove redirection and file path from the command
		cmdLine = _trim(cmdLine.substr(0, redirectionPos));

		// Update `cmdSegments` after removing redirection parts
		cmdSegments.clear();
		std::istringstream iss(cmdLine);
		std::string segment;
		while (iss >> segment) {
			cmdSegments.push_back(segment);
		}
	}
	else {
		std::cerr << "smash error: invalid redirection syntax" << std::endl;
		fileRedirect.clear();
	}
}
void RedirectionCommand::execute() {
	if (fileRedirect.empty()) {
		std::cerr << "smash error: no file specified for redirection" << std::endl;
		return;
	}

	int flags = O_WRONLY | O_CREAT | (m_isAppend ? O_APPEND : O_TRUNC);
	int fd = open(fileRedirect.c_str(), flags, 0644);
	if (fd == -1) {
		perror("smash error: open failed");
		return;
	}

	int stdoutBackup = dup(STDOUT_FILENO);
	if (stdoutBackup == -1) {
		perror("smash error: dup failed");
		close(fd);
		return;
	}

	if (dup2(fd, STDOUT_FILENO) == -1) {
		perror("smash error: dup2 failed");
		close(fd);
		close(stdoutBackup);
		return;
	}
	close(fd);

	Command* cmd = SmallShell::getInstance().createCommand(cmdLine.c_str());
	if (cmd) {
		cmd->execute();
		delete cmd;
	}
	else {
		std::cerr << "smash error: failed to create command" << std::endl;
	}

	// Restore stdout
	if (dup2(stdoutBackup, STDOUT_FILENO) == -1) {
		perror("smash error: dup2 restore failed");
	}
	close(stdoutBackup);
}


// ListDirCommand Class
ListDirCommand::ListDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}
void ListDirCommand::listDirectoryRecursively(const std::string& path, const std::string& indent) const {
	DIR* dir = opendir(path.c_str());
	if (!dir) {
		perror("smash error: opendir failed");
		return;
	}

	std::vector<std::string> directories;
	std::vector<std::string> files;

	struct dirent* entry;
	while ((entry = readdir(dir)) != nullptr) {
		std::string name = entry->d_name;

		// Skip current and parent directory symbols
		if (name == "." || name == "..") {
			continue;
		}

		std::string fullPath = path + "/" + name;

		if (isDirectory(fullPath)) {
			directories.push_back(name);
		}
		else {
			files.push_back(name);
		}
	}
	closedir(dir);

	// Sort directories and files alphabetically
	std::sort(directories.begin(), directories.end());
	std::sort(files.begin(), files.end());

	// Print directories
	for (const auto& dirName : directories) {
		std::cout << indent << dirName << "/" << std::endl;
		listDirectoryRecursively(path + "/" + dirName, indent + "\t");
	}

	// Print files
	for (const auto& fileName : files) {
		std::cout << indent << fileName << std::endl;
	}
}
void ListDirCommand::execute() {
	if (cmdSegments.size() > 2) {
		std::cerr << "smash error: listdir: too many arguments" << std::endl;
		return;
	}

	// Use SmallShell::getPwd to get the current working directory if no argument is provided
	std::string directoryPath = (cmdSegments.size() == 1)
		? SmallShell::getInstance().getPwd()
		: cmdSegments[1];

	// Remove trailing background sign '&'
	_trimAmp(directoryPath);

	if (!isDirectory(directoryPath)) {
		perror("smash error: listdir");
		return;
	}

	listDirectoryRecursively(directoryPath);
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
	std::string cmd(cmd_line);

	if (cmd.find('>') != std::string::npos) {
		return new RedirectionCommand(cmd_line);
	}
	else if (firstWord == "chprompt") {
		return new ChangePromptCommand(cmd_line, prompt);
	}
	else if (firstWord == "pwd") {
		return new GetCurrDirCommand(cmd_line);
	}
	else if (firstWord == "showpid") {
		return new ShowPidCommand(cmd_line);
	}
	else if (firstWord == "cd") {
		return new ChangeDirCommand(cmd_line);
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
	else if (firstWord.compare("listdir") == 0)
	{
		return new ListDirCommand(cmd_line);
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
std::string SmallShell::getLastWorkingDir() const {
	return lastWorkingDir;
}
void SmallShell::setLastWorkingDir(const std::string& dir) {
	lastWorkingDir = dir;
}
std::string SmallShell::getPwd() const {
	char cwd[COMMAND_MAX_LENGTH];
	if (getcwd(cwd, sizeof(cwd)) == nullptr) {
		perror("smash error: getcwd failed");
		return ""; // Return an empty string if getting the working directory fails
	}
	return std::string(cwd); // Convert to a `std::string` and return
}
std::string SmallShell::getPrompt() const {
	return prompt + "> ";
}
void SmallShell::setPrompt(const std::string& newPrompt) {
	prompt = newPrompt;
}
JobsList& SmallShell::getJobsList() {
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
