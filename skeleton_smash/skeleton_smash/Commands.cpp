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
#include <set>
#include <string>


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

ExternalCommand::ExternalCommand(const char* cmd_line)
	: Command(cmd_line), m_isBackground(false) {
	m_cmdLine = std::string(cmd_line);

	// Trim whitespace and detect background execution
	m_cmdLine.erase(m_cmdLine.find_last_not_of(" \t\n\r\f\v") + 1);
	if (!m_cmdLine.empty() && m_cmdLine.back() == '&') {
		m_cmdLine.pop_back(); // Remove '&'
		m_cmdLine.erase(m_cmdLine.find_last_not_of(" \t\n\r\f\v") + 1);
		m_isBackground = true;
	}
}

ExternalCommand::~ExternalCommand() {}

bool ExternalCommand::isComplexCommand(const std::string& cmd_line) {
	return cmd_line.find('*') != std::string::npos || cmd_line.find('?') != std::string::npos;
}

void ExternalCommand::execute() {
	if (m_isBackground) {
		// For background commands, add the job using JobsList
		SmallShell::getInstance().getJobsList().addJob(this, false);
	}
	else {
		// Handle foreground commands directly
		pid_t pid = fork();

		if (pid == -1) {
			perror("smash error: fork failed");
			return;
		}

		if (pid == 0) { // Child process
			setpgrp(); // Create a new process group
			if (isComplexCommand(m_cmdLine)) {
				// Complex command: execute via /bin/bash
				execl("/bin/bash", "bash", "-c", m_cmdLine.c_str(), nullptr);
			}
			else {
				// Simple command: prepare arguments for execvp
				char* args[21] = { nullptr };
				char* cmdCopy = strdup(m_cmdLine.c_str());
				char* token = strtok(cmdCopy, " ");
				int i = 0;

				while (token != nullptr && i < 20) {
					args[i++] = token;
					token = strtok(nullptr, " ");
				}

				execvp(args[0], args); // Execute the command
				perror("smash error: execvp failed");
				free(cmdCopy);
				exit(1);
			}
		}
		else { // Parent process
			int status;
			waitpid(pid, &status, WUNTRACED); // Wait for foreground command to finish
		}
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
	// Remove any background sign if present at the end of the command
	if (!m_command_seg.empty() && m_command_seg.back() == "&") {
		m_command_seg.pop_back();
	}

	// If no argument is provided, do nothing
	if (m_command_seg.size() == 1) {
		return;
	}

	// If exactly one argument is provided (the path)
	if (m_command_seg.size() == 2) {
		std::string path = m_command_seg[1];
		delBackSign(path);  // Clean up background sign if any

		// Handle the special case for 'cd -'
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
		// Change to the specified directory (relative or absolute path)
		else {
			if (chdir(path.c_str()) != -1) {
				SmallShell::getInstance().changePwd(path);
			}
			else {
				perror("smash error: chdir failed");
			}
		}
	}
	// If there are too many arguments, print the error message
	else {
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

QuitCommand::QuitCommand(const char* cmd_line, JobsList* jobsList, bool kill)
	: BuiltInCommand(cmd_line), m_jobsList(jobsList), m_kill(kill) {}

QuitCommand::~QuitCommand() {}

void QuitCommand::execute() {
	if (m_kill) {
		// Print the number of jobs to be killed
		int killedJobsCount = 0;
		for (auto& job : m_jobsList->getJobs()) {
			if (!job.m_isStopped) {
				std::cout << "smash: sending SIGKILL signal to " << m_jobsList->getJobs().size() << " jobs:" << std::endl;
				break;  // Print only once before killing jobs
			}
		}

		// Iterate over jobs and kill them
		for (auto& job : m_jobsList->getJobs()) {
			if (!job.m_isStopped) {
				std::cout << job.m_pid << ": " << job.m_command << std::endl;
				kill(job.m_pid, SIGKILL);  // Send SIGKILL to the job's PID
				++killedJobsCount;
			}
		}

		std::cout << killedJobsCount << " jobs were killed." << std::endl;
	}

	// Exit the shell (program)
	exit(0);
}

// ================= JobsList Class =================

JobsList::JobsList() : m_lastJobId(0) {}

JobsList::~JobsList() {
	m_jobs.clear();
}

void JobsList::addJob(Command* cmd, bool isStopped) {
	int pid = fork();

	if (pid == -1) {  // Fork failed
		perror("smash error: fork failed");
		return;  // Exit on error
	}

	if (pid == 0) {  // Child process
		setpgrp();  // Set a new process group for the child process

		// Prepare arguments for execv
		const char* args[] = { "/bin/bash", "-c", cmd->getCommandLine().c_str(), nullptr };

		// Execute the command in the child process
		if (execv("/bin/bash", (char* const*)args) == -1) {
			perror("smash error: execv failed");
			exit(1);  // Ensure child exits if execv fails
		}
	}
	else {  // Parent process
		// Increment job ID and add the job to the list
		m_jobs.push_back(JobEntry(++m_lastJobId, cmd->getCommandLine(), pid, isStopped));
	}
}

void JobsList::printJobsList() {
	removeFinishedJobs();  // Remove finished jobs before printing
	if (m_jobs.empty()) {
		std::cout << "No jobs found." << std::endl;
		return;
	}

	// Sort jobs by jobId
	std::sort(m_jobs.begin(), m_jobs.end(), [](const JobEntry& a, const JobEntry& b) {
		return a.m_jobId < b.m_jobId;
		});

	// Print each job in the required format
	for (const JobEntry& job : m_jobs) {
		std::cout << "[" << job.m_jobId << "] " << job.m_command << (job.m_isStopped ? " (stopped)" : "") << std::endl;
	}
}

void JobsList::killAllJobs() {
	for (const auto& job : m_jobs) {
		kill(job.m_pid, SIGKILL);
	}
	m_jobs.clear();
}

void JobsList::removeFinishedJobs() {
	for (auto it = m_jobs.begin(); it != m_jobs.end();) {
		int status;
		int result = waitpid(it->m_pid, &status, WNOHANG);  // Non-blocking wait

		if (result == -1) {
			++it;  // Continue if error occurred
		}
		else if (result == 0) {
			++it;  // Continue if job is still running
		}
		else {
			it = m_jobs.erase(it);  // Remove job if finished
		}
	}
}

JobsList::JobEntry* JobsList::getJobById(int jobId) {
	for (auto& job : m_jobs) {
		if (job.m_jobId == jobId) {
			return &job;
		}
	}
	return nullptr;
}

void JobsList::removeJobById(int jobId) {
	auto it = std::remove_if(m_jobs.begin(), m_jobs.end(), [jobId](const JobEntry& job) {
		return job.m_jobId == jobId;
		});
	m_jobs.erase(it, m_jobs.end());
}

JobsList::JobEntry* JobsList::getLastJob() {
	if (m_jobs.empty()) {
		return nullptr;
	}
	return &m_jobs.back();
}

JobsList::JobEntry* JobsList::getLastStoppedJob(int* jobId) {
	for (auto it = m_jobs.rbegin(); it != m_jobs.rend(); ++it) {
		if (it->m_isStopped) {
			*jobId = it->m_jobId;  // Set the job ID of the last stopped job
			return &(*it);  // Return last stopped job
		}
	}
	return nullptr;
}

bool JobsList::isEmpty() const {
	return m_jobs.empty();
}

void JobsList::printJobs() const {
	for (const auto& job : m_jobs) {
		std::cout << "[" << job.m_jobId << "] " << job.m_command << "&" << std::endl;
	}
}

std::vector<JobsList::JobEntry>& JobsList::getJobs() {
	return m_jobs;
}

// ================= JobsCommand Class =================

JobsCommand::JobsCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), m_jobs(jobs) {}

JobsCommand::~JobsCommand() {}

void JobsCommand::execute() {
	m_jobs->printJobsList();
}

// ================= KillCommand Class =================

KillCommand::KillCommand(const char* cmd_line, JobsList* jobsList)
	: BuiltInCommand(cmd_line), m_jobsList(jobsList) {

	char signumStr[10], jobIdStr[10];
	int numArgs = sscanf(cmd_line, "kill -%s %s", signumStr, jobIdStr);

	if (numArgs != 2) {
		std::cerr << "smash error: kill: invalid arguments" << std::endl;
		return;
	}

	// Convert the signum to an integer
	m_signum = atoi(signumStr);

	// Convert the job ID to an integer
	m_jobId = atoi(jobIdStr);

	if (m_signum <= 0 || m_jobId <= 0) {
		std::cerr << "smash error: kill: invalid arguments" << std::endl;
		return;
	}
}

KillCommand::~KillCommand() {}

void KillCommand::execute() {
	JobsList::JobEntry* job = m_jobsList->getJobById(m_jobId);

	if (!job) {
		std::cerr << "smash error: kill: job-id " << m_jobId << " does not exist" << std::endl;
		return;
	}

	// Try to send the signal
	if (kill(job->m_pid, m_signum) == -1) {
		perror("smash error: kill");
		return;
	}

	// Print the success message
	std::cout << "signal number " << m_signum << " was sent to pid " << job->m_pid << std::endl;
}

// ================= ForegroundCommand Class =================

ForegroundCommand::ForegroundCommand(const char* cmd_line, JobsList* jobs)
	: BuiltInCommand(cmd_line), m_jobs(jobs) {}

ForegroundCommand::~ForegroundCommand() {}

void ForegroundCommand::execute() {
	JobsList::JobEntry* job = m_jobs->getLastJob();
	if (job == nullptr) {
		std::cout << "smash error: fg: jobs list is empty" << std::endl;
		return;
	}

	int jobId = job->m_jobId;
	int pid = job->m_pid; // Use int for pid

	// Print job information
	std::cout << job->m_command << " : " << pid << std::endl;

	// Remove the job from the jobs list
	m_jobs->removeJobById(jobId);

	// Resume the job with SIGCONT
	if (kill(pid, SIGCONT) == -1) {
		perror("smash error: kill failed");
	}

	// Wait for the job to finish execution or stop
	if (waitpid(pid, nullptr, WUNTRACED) == -1) {
		perror("smash error: waitpid failed");
	}
}

// ================= ListDirCommand Class =================

ListDirCommand::ListDirCommand(const char* cmd_line) : Command(cmd_line) {}

ListDirCommand::~ListDirCommand() {}

void ListDirCommand::execute() {}

// ================= WhoAmICommand Class =================

WhoAmICommand::WhoAmICommand(const char* cmd_line) : Command(cmd_line) {}

WhoAmICommand::~WhoAmICommand() {}

void WhoAmICommand::execute() {}

// ================= aliasCommand Class =================

aliasCommand::aliasCommand(const char* cmd_line, std::map<std::string, std::string>& aliases)
	: BuiltInCommand(cmd_line), m_aliases(aliases) {

	// Convert cmd_line (const char*) to std::string
	std::string cmd_str(cmd_line);

	// Define regex pattern to match alias command
	std::regex aliasFormat("^alias ([a-zA-Z0-9_]+)='([^']*)'$");
	std::smatch matches;

	// Check if the format of the alias command is valid
	if (std::regex_match(cmd_str, matches, aliasFormat)) {
		m_name = matches[1].str();     // alias name
		m_command = matches[2].str();  // alias command

		// Check if alias already exists
		if (m_aliases.find(m_name) != m_aliases.end()) {
			std::cerr << "smash error: alias: " << m_name << " already exists or is a reserved command" << std::endl;
			return;
		}

		// Add the alias to the map
		m_aliases[m_name] = m_command;
	}
	else {
		std::cerr << "smash error: alias: invalid alias format" << std::endl;
	}
}

aliasCommand::~aliasCommand() {}

void aliasCommand::execute() {
	if (m_name.empty()) {
		for (const auto& alias : m_aliases) {
			std::cout << alias.first << "='" << alias.second << "'" << std::endl;
		}
	}
	else {
		// If alias was added, execute it (no action needed here, already done in the constructor)
		std::cout << m_name << "='" << m_command << "'" << std::endl;
	}
}

bool aliasCommand::isValidAliasName(const std::string& name) {
	// Reserved commands (e.g., quit, lsdir, etc.) can be checked here
	static const std::set<std::string> reservedCommands = {
		"quit", "alias", "jobs", "fg", "kill", "lsdir"
	};

	// Check if the alias name exists in the reserved commands or if it is already in use
	if (reservedCommands.find(name) != reservedCommands.end() || m_aliases.find(name) != m_aliases.end()) {
		return false;
	}
	return true;
}

void aliasCommand::listAliases() const {
	if (m_aliases.empty()) {
		return;
	}

	for (const auto& alias : m_aliases) {
		std::cout << alias.first << "='" << alias.second << "'" << std::endl;
	}
}

void aliasCommand::createAlias() {
	if (m_aliases.find(m_name) != m_aliases.end()) {
		std::cerr << "smash error: alias: " << m_name << " already exists or is a reserved command" << std::endl;
		return;
	}

	m_aliases[m_name] = m_command;
	std::cout << "Alias " << m_name << " created successfully." << std::endl;
}

// ================= NetInfo Class =================

NetInfo::NetInfo(const char* cmd_line) : Command(cmd_line) {}

NetInfo::~NetInfo() {}

void NetInfo::execute() {}

// ================= unaliasCommand Class =================

unaliasCommand::unaliasCommand(const char* cmd_line, std::map<std::string, std::string>& aliases)
	: BuiltInCommand(cmd_line), m_aliases(aliases) {

	if (cmd_line == nullptr || cmd_line[0] == '\0') {
		std::cerr << "smash error: unalias: not enough arguments" << std::endl;
		return;
	}

	// Parse the command line arguments after 'unalias'
	std::string cmdStr(cmd_line);
	std::stringstream ss(cmdStr);
	std::string alias;

	// Extract alias names from the command line
	while (ss >> alias) {
		m_names.push_back(alias);
	}

	// Skip the first "unalias" part
	m_names.erase(m_names.begin());

	if (m_names.empty()) {
		std::cerr << "smash error: unalias: not enough arguments" << std::endl;
	}
	else {
		removeAliases(m_names);
	}
}

unaliasCommand::~unaliasCommand() {}

void unaliasCommand::execute() {
	if (m_names.empty()) {
		std::cerr << "smash error: unalias: not enough arguments" << std::endl;
		return;
	}

	for (const auto& name : m_names) {
		auto it = m_aliases.find(name);
		if (it != m_aliases.end()) {
			m_aliases.erase(it);  // Remove alias
		}
		else {
			std::cerr << "smash error: unalias: " << name << " alias does not exist" << std::endl;
			return;  // Stop at the first non-existing alias
		}
	}
}

void unaliasCommand::removeAliases(const std::vector<std::string>& aliasNames) {
	for (const auto& name : aliasNames) {
		// Check if alias exists
		if (m_aliases.find(name) == m_aliases.end()) {
			std::cerr << "smash error: unalias: " << name << " alias does not exist" << std::endl;
			return;
		}

		m_aliases.erase(name);
	}
}

// ================= SmallShell Singleton =================

SmallShell::SmallShell() : m_prompt("smash"), m_prevDir(""), m_pid(getpid()), m_currDir(getSmashPwd()){}

SmallShell::~SmallShell() {}

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
	else if (firstWord == "alias") {
		return new aliasCommand(cmd_line, m_aliases);  // Pass m_aliases for alias handling
	}
	else if (firstWord == "unalias") {
		return new unaliasCommand(cmd_line, m_aliases);  // Pass m_aliases for alias removal
	}
	else {
		return new ExternalCommand(cmd_line); // Default to external command if no match
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