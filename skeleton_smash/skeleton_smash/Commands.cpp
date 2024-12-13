
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
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

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/

void SmallShell::executeCommand(const char* cmd_line) { // in progress...
	Command* cmd = CreateCommand(cmd_line);
	cmd->execute();
	// we might need to delete the command or store it for later usage or deletion
	// Please note that you must fork smash process for some commands (e.g., external commands....)
}

void ChangePromptCommand::execute()  {
	std::istringstream iss(m_cmd_line);
	std::string command, newPrompt;

	iss >> command; // Skip the command name
	if (iss >> newPrompt) {
		SmallShell::getInstance().setPrompt(newPrompt); // Use the singleton
	}
	else {
		SmallShell::getInstance().setPrompt("smash");
	}
}

void ShowPidCommand::execute()  {
	std::cout << "smash pid is " << getpid() << std::endl;
}

void GetCurrDirCommand::execute()  {
	char cwd[COMMAND_MAX_LENGTH];
	if (getcwd(cwd, sizeof(cwd)) != nullptr) {
		std::cout << cwd << std::endl;
	}
	else {
		perror("smash error: getcwd failed");
	}
}

void ChangeDirCommand::execute()  {
	std::istringstream iss(this->m_cmd_line);
	std::string command, path;

	iss >> command; // Extract the "cd" command
	if (!(iss >> path)) {
		// No arguments provided, no action
		return;
	}

	std::string extraArg;
	if (iss >> extraArg) {
		// Too many arguments
		std::cerr << "smash error: cd: too many arguments" << std::endl;
		return;
	}

	// Get SmallShell instance
	SmallShell& shell = SmallShell::getInstance();

	if (path == "-") {
		// Handle 'cd -'
		if (!shell.getLastPwd()) {
			std::cerr << "smash error: cd: OLDPWD not set" << std::endl;
			return;
		}
		path = std::string(shell.getLastPwd()); // Set path to the last working directory
	}

	char currentDir[COMMAND_MAX_LENGTH];
	if (getcwd(currentDir, sizeof(currentDir)) == nullptr) {
		perror("smash error: getcwd failed");
		return;
	}

	if (chdir(path.c_str()) == -1) {
		perror("smash error: chdir failed");
		return;
	}

	// Update lastPwd in SmallShell
	shell.setLastPwd(currentDir);
}

Command* SmallShell::CreateCommand(const char* cmd_line) { // in progress...
	string cmd_s = _trim(string(cmd_line));
	string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
	if (firstWord.compare("chprompt") == 0) {
		return new ChangePromptCommand(cmd_line);
	}
	else if (firstWord.compare("showpid") == 0) {
		return new ShowPidCommand(cmd_line);
	}
	else if (firstWord.compare("pwd") == 0) {
		return new GetCurrDirCommand(cmd_line);
	}
	else if (firstWord.compare("cd") == 0) {
		return new ChangeDirCommand(cmd_line);
	}
	else if (firstWord.compare("jobs") == 0) {
		return new JobsCommand(cmd_line, &getInstance().getJobsList());
	}
	else {
		return new ExternalCommand(cmd_line);
	}
	return nullptr;
}