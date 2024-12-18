#include "Commands.h"
#include "signal.h"
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
#include <regex>

using namespace std;


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
		args[0] = nullptr;
		return 0;
	}

	int i = 0;
	std::istringstream iss(_trim(cmd_line));
	for (std::string token; iss >> token;) {
		// If the token ends with '&', split it into two arguments
		if (token.back() == '&' && token.length() > 1) {
			token.pop_back();  // Remove the trailing '&'
			args[i] = strdup(token.c_str());  // Add the token without '&'
			if (!args[i]) {
				perror("smash error: malloc failed");
				exit(1);
			}
			i++;

			args[i] = strdup("&");  // Add '&' as a separate argument
			if (!args[i]) {
				perror("smash error: malloc failed");
				exit(1);
			}
			i++;
		}
		else {
			// Normal token handling
			args[i] = strdup(token.c_str());
			if (!args[i]) {
				perror("smash error: malloc failed");
				exit(1);
			}
			i++;
		}

		if (i >= COMMAND_MAX_ARGS - 1) break;
	}

	args[i] = nullptr;  // Null-terminate the argument list
	return i;
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


// ctrlC Handler
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


// Signal handler for SIGCHLD
void sigchldHandler(int sig_num) {
	(void)sig_num; // Suppress unused warning
	int status;
	pid_t pid;

	// Reap terminated child processes
	while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
		SmallShell& shell = SmallShell::getInstance();
		JobsList& jobsList = shell.getJobsList();

		// Remove the job from the jobs list
		for (auto it = jobsList.getJobs().begin(); it != jobsList.getJobs().end(); ++it) {
			if ((*it)->pid == pid) {
				jobsList.removeJobById((*it)->m_job_id);
				break;
			}
		}
	}
}


// Setup signal handling for SIGCHLD
void setupSignals() {
	struct sigaction sa;
	sa.sa_handler = ctrlCHandler;
	sa.sa_flags = SA_RESTART; // Restart system calls if interrupted
	if (sigaction(SIGINT, &sa, nullptr) == -1) {
		perror("smash error: sigaction failed");
	}
}


// Command Class
Command::Command(const char* cmd_line)
	: cmdSegments(), m_PID(-1), m_is_background(false), m_cmd_line(cmd_line) {
}
Command::~Command() {}
int Command::getm_PID() const { return m_PID; }
void Command::setm_PID(int pid) { m_PID = pid; }
std::string Command::getAlias() const { return alias; }
void Command::setAlias(const std::string& aliasCommand) { alias = aliasCommand; }
std::string Command::getPath() const { return m_file_redirect; }
void Command::setPath(const std::string& path) { m_file_redirect = path; }
std::string Command::getCommandLine() const {
	if (!m_cmd_line.empty()) {
		return m_cmd_line; // Return the original command line if available
	}

	// Reconstruct the command line from segments
	std::ostringstream oss;
	for (size_t i = 0; i < cmdSegments.size(); ++i) {
		oss << cmdSegments[i];
		if (i < cmdSegments.size() - 1) {
			oss << " ";
		}
	}
	if (m_is_background) {
		oss << " &";
	}
	return oss.str();
}


// BuiltInCommand Class
BuiltInCommand::BuiltInCommand(const char* cmd_line) : Command(cmd_line) {}
BuiltInCommand::~BuiltInCommand() {}


// ExternalCommand Class
ExternalCommand::ExternalCommand(const char* cmd_line) : Command(cmd_line) {}
ExternalCommand::~ExternalCommand() {
	// Free dynamically allocated memory in cmdSegments
	for (auto& segment : cmdSegments) {
		free(const_cast<char*>(segment.c_str())); // Deallocate memory used for command arguments
	}
	cmdSegments.clear();
}
void ExternalCommand::execute() {
	char* args[COMMAND_MAX_ARGS];
	int argc = _parseCommandLine(m_cmd_line, args);

	if (argc == 0) return;

	// Check for background execution
	bool m_is_background = false;
	if (strcmp(args[argc - 1], "&") == 0) {
		m_is_background = true;
		free(args[argc - 1]);
		args[argc - 1] = nullptr;
	}

	pid_t pid = fork();
	if (pid == 0) { // Child process
		setpgrp(); // Create a new process group
		execvp(args[0], args); // Execute command
		perror("smash error: execvp failed");
		exit(1);
	}
	else if (pid > 0) { // Parent process
		SmallShell& shell = SmallShell::getInstance();
		if (m_is_background) {
			shell.getJobsList().addJob(m_cmd_line, pid, false);
		}
		else {
			shell.setForegroundJob(pid, m_cmd_line);
			waitpid(pid, nullptr, WUNTRACED);
			shell.clearForegroundJob();
		}
	}
	else { // Fork failed
		perror("smash error: fork failed");
	}

	// Free allocated arguments
	for (int i = 0; i < argc; ++i) {
		free(args[i]);
	}
}


// ChangePromptCommand Class
ChangePromptCommand::ChangePromptCommand(const char* cmd_line, std::string& prompt)
	: BuiltInCommand(cmd_line), prompt(prompt) {}
ChangePromptCommand::~ChangePromptCommand() {}
void ChangePromptCommand::execute() {
	char* args[COMMAND_MAX_ARGS];
	int argc = _parseCommandLine(m_cmd_line, args);

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
		if (shell.getLastDir().empty()) {
			std::cerr << "smash error: cd: OLDPWD not set" << std::endl;
			free(currentDir);
			return;
		}
		if (chdir(shell.getLastDir().c_str()) == -1) {
			perror("smash error: chdir failed");
		}
		else {
			shell.setLastDir(currentDir); // Update lastWorkingDir with the previous directory
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
			shell.setLastDir(currentDir); // Update lastWorkingDir
		}
		free(currentDir);
		return;
	}

	// Handle regular path: Change directory
	if (chdir(targetDir.c_str()) == -1) {
		perror("smash error: chdir failed");
	}
	else {
		shell.setLastDir(currentDir); // Update lastWorkingDir
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
JobsList::JobsList() : lastm_job_id(0) {}
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
void JobsList::addJob(const std::string& command, int pid, bool m_is_stopped) {
	removeFinishedJobs(); // Clean up finished jobs
	int m_job_id = ++lastm_job_id;
	jobs.push_back(new JobEntry(m_job_id, pid, command, m_is_stopped));
}
void JobsList::printJobs() const {
	for (const auto& job : jobs) {
		std::cout << "[" << job->m_job_id << "] " << job->command
			<< (job->m_is_stopped ? " (stopped)" : "") << std::endl;
	}
}
void JobsList::removeFinishedJobs() {
	for (auto it = jobs.begin(); it != jobs.end();) {
		int status;
		pid_t result = waitpid((*it)->pid, &status, WNOHANG);

		if (result > 0) { // Job finished
			delete* it;
			it = jobs.erase(it);
		}
		else if (result == 0) { // Job still running
			++it;
		}
		else { // Error in waitpid
			perror("smash error: waitpid failed");
			++it;
		}
	}
}
JobsList::JobEntry* JobsList::getJobById(int m_job_id) {
	for (auto& job : jobs) {
		if (job->m_job_id == m_job_id) {
			return job;
		}
	}
	return nullptr; // Job not found
}
void JobsList::removeJobById(int m_job_id) {
	for (auto it = jobs.begin(); it != jobs.end(); ++it) {
		if ((*it)->m_job_id == m_job_id) {
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
	int argc = _parseCommandLine(m_cmd_line, args);

	// Validate input: must have exactly 3 arguments and first starts with '-'
	if (argc != 3 || args[1][0] != '-' || !isdigit(args[1][1]) || !isdigit(args[2][0])) {
		std::cerr << "smash error: kill: invalid arguments" << std::endl;
		for (int i = 0; i < argc; ++i) free(args[i]);
		return;
	}

	int signum = atoi(args[1] + 1);  // Skip '-' and parse the signal number
	int m_job_id = atoi(args[2]);       // Parse job ID

	// Validate job ID
	JobsList::JobEntry* job = jobsList->getJobById(m_job_id);
	if (!job) {
		std::cerr << "smash error: kill: job-id " << m_job_id << " does not exist" << std::endl;
		for (int i = 0; i < argc; ++i) free(args[i]);
		return;
	}

	// Send signal to process
	if (kill(job->pid, signum) == -1) {
		perror("smash error: kill failed");
	}
	else {
		std::cout << "signal number " << signum << " was sent to pid " << job->pid << std::endl;
	}

	for (int i = 0; i < argc; ++i) free(args[i]);
}


// QuitCommand Class
QuitCommand::QuitCommand(const char* cmd_line, JobsList* jobs)
	: BuiltInCommand(cmd_line), jobsList(jobs) {}
QuitCommand::~QuitCommand() {}
void QuitCommand::execute() {
	JobsList& jobsList = SmallShell::getInstance().getJobsList();

	if (strstr(m_cmd_line.c_str(), "kill")) {
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
	int argc = _parseCommandLine(m_cmd_line, args);

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
		int m_job_id = atoi(args[1]);
		if (m_job_id <= 0) {
			std::cerr << "smash error: fg: invalid arguments" << std::endl;
			return;
		}

		// Try to find the job with the given job ID
		job = jobsList->getJobById(m_job_id);
		if (!job) {
			std::cerr << "smash error: fg: job-id " << m_job_id << " does not exist" << std::endl;
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
	jobsList->removeJobById(job->m_job_id);

	// Free allocated memory
	for (int i = 0; i < argc; ++i) {
		free(args[i]);
	}
}


// AliasCommand Class
AliasCommand::AliasCommand(const char* cmd_line, std::map<std::string, std::string>& aliasMap)
	: BuiltInCommand(cmd_line), aliasMap(aliasMap) {}
AliasCommand::~AliasCommand() {}
void AliasCommand::execute() {
	// Trim the command line to handle any spaces
	std::string commandLine = _trim(m_cmd_line);

	// Case 1: If the command is exactly "alias" with no arguments
	if (commandLine == "alias") {
		// Print all aliases in the map
		for (const auto& alias : aliasMap) {
			std::cout << alias.first << "='" << alias.second << "'" << std::endl;
		}
		return; // Exit after printing
	}

	// Case 2: Handle alias creation (alias <name>='<command>')
	size_t equalPos = commandLine.find('=');
	if (equalPos == std::string::npos || equalPos < 6) { // Missing '=' or alias name
		std::cerr << "smash error: alias: invalid alias format" << std::endl;
		return;
	}

	// Extract the alias name and command
	std::string aliasName = _trim(commandLine.substr(6, equalPos - 6));
	std::string aliasCommand = _trim(commandLine.substr(equalPos + 1));

	// Validate alias name format
	if (!std::regex_match(aliasName, std::regex("^[a-zA-Z0-9_]+$"))) {
		std::cerr << "smash error: alias: invalid alias format" << std::endl;
		return;
	}

	// Check for proper quotes around the alias command
	if (aliasCommand.length() < 2 || aliasCommand.front() != '\'' || aliasCommand.back() != '\'') {
		std::cerr << "smash error: alias: invalid alias format" << std::endl;
		return;
	}

	// Remove surrounding quotes from the command
	aliasCommand = aliasCommand.substr(1, aliasCommand.length() - 2);

	// Check for reserved keywords
	static const std::set<std::string> reservedKeywords = {
		"quit", "fg", "bg", "jobs", "kill", "cd", "listdir", "chprompt", "alias", "unalias", "pwd", "showpid"
	};

	if (reservedKeywords.count(aliasName) || aliasMap.count(aliasName)) {
		std::cerr << "smash error: alias: " << aliasName << " already exists or is a reserved command" << std::endl;
		return;
	}

	// Add the alias to the map
	aliasMap[aliasName] = aliasCommand;
}

// UnaliasCommand Class
UnaliasCommand::UnaliasCommand(const char* cmd_line, std::map<std::string, std::string>& aliasMap)
	: BuiltInCommand(cmd_line), aliasMap(aliasMap) {}
UnaliasCommand::~UnaliasCommand() {}
void UnaliasCommand::execute() {
	char* args[COMMAND_MAX_ARGS];
	int argc = _parseCommandLine(m_cmd_line, args);

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
	size_t redirectionPos = m_cmd_line.find('>');
	if (redirectionPos != std::string::npos) {
		m_isAppend = (m_cmd_line[redirectionPos + 1] == '>'); // Check if it's >>
		size_t fileStart = m_isAppend ? redirectionPos + 2 : redirectionPos + 1;

		// Trim and extract the file path
		m_file_redirect = _trim(m_cmd_line.substr(fileStart));

		// Remove redirection and file path from the command
		m_cmd_line = _trim(m_cmd_line.substr(0, redirectionPos));

		// Update `cmdSegments` after removing redirection parts
		cmdSegments.clear();
		std::istringstream iss(m_cmd_line);
		std::string segment;
		while (iss >> segment) {
			cmdSegments.push_back(segment);
		}
	}
	else {
		std::cerr << "smash error: invalid redirection syntax" << std::endl;
		m_file_redirect.clear();
	}
}
RedirectionCommand::~RedirectionCommand() {}
void RedirectionCommand::execute() {
	if (m_file_redirect.empty()) {
		std::cerr << "smash error: no file specified for redirection" << std::endl;
		return;
	}

	int flags = O_WRONLY | O_CREAT | (m_isAppend ? O_APPEND : O_TRUNC);
	int fd = open(m_file_redirect.c_str(), flags, 0644);
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

	Command* cmd = SmallShell::getInstance().generateCommand(m_cmd_line.c_str());
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
ListDirCommand::~ListDirCommand() {}
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


// WhoamiCommand Class
WhoamiCommand::WhoamiCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}
void WhoamiCommand::execute() {
	char* username = getenv("USER"); // Retrieve the USER environment variable
	char* homeDir = getenv("HOME");  // Retrieve the HOME environment variable

	if (username && homeDir) {
		std::cout << username << " " << homeDir << std::endl;
	}
	else {
		std::cerr << "smash error: whoami: failed to retrieve user information" << std::endl;
	}
}


// SmallShell Class
SmallShell::SmallShell()
	: prompt("smash"), lastWorkingDir(""), foregroundPid(-1), foregroundCommand("") {
	setupSignals();
}
SmallShell::~SmallShell() {}
SmallShell& SmallShell::getInstance() {
	static SmallShell instance;
	return instance;
}
Command* SmallShell::generateCommand(const char* cmd_line) {
	std::string cmd_s = _trim(std::string(cmd_line));
	std::istringstream iss(cmd_s);
	std::string firstWord;
	iss >> firstWord;

	// Handle alias expansion
	auto aliasIt = aliasMap.find(firstWord);
	if (aliasIt != aliasMap.end()) {
		std::string expandedCommand = aliasIt->second;

		// Append remaining arguments
		std::string remainingArgs;
		getline(iss, remainingArgs);
		if (!remainingArgs.empty()) {
			expandedCommand += " " + _trim(remainingArgs);
		}

		cmd_s = _trim(expandedCommand);
	}

	// Parse command again after alias expansion
	std::istringstream expandedIss(cmd_s);
	expandedIss >> firstWord;

	if (cmd_s.find('>') != std::string::npos) return new RedirectionCommand(cmd_s.c_str());
	if (firstWord == "chprompt") return new ChangePromptCommand(cmd_s.c_str(), prompt);
	if (firstWord == "pwd") return new GetCurrDirCommand(cmd_s.c_str());
	if (firstWord == "showpid") return new ShowPidCommand(cmd_s.c_str());
	if (firstWord == "cd") return new ChangeDirCommand(cmd_s.c_str());
	if (firstWord == "jobs") return new JobsCommand(cmd_s.c_str(), &jobs);
	if (firstWord == "kill") return new KillCommand(cmd_s.c_str(), &jobs);
	if (firstWord == "quit") return new QuitCommand(cmd_s.c_str(), &jobs);
	if (firstWord == "fg") return new ForegroundCommand(cmd_s.c_str(), &jobs);
	if (firstWord == "alias") return new AliasCommand(cmd_s.c_str(), aliasMap);
	if (firstWord == "unalias") return new UnaliasCommand(cmd_s.c_str(), aliasMap);
	if (firstWord == "whoami") return new WhoamiCommand(cmd_s.c_str());

	return new ExternalCommand(cmd_s.c_str());
}
void SmallShell::executeCommand(const char* cmd_line) {
	Command* cmd = generateCommand(cmd_line);
	if (cmd) {
		cmd->execute();
		delete cmd;
	}
}
std::string SmallShell::getLastDir() const {
	return lastWorkingDir;
}
void SmallShell::setLastDir(const std::string& dir) {
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
void SmallShell::updateWorkingDir(const std::string& newDir) {
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
