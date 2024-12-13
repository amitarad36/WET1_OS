#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <cstring>

#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

class Command {
protected:
	std::string m_cmd_line;
public:
	Command(const char* cmd_line);

	virtual ~Command();

	virtual void execute() = 0;

	//virtual void prepare();
	//virtual void cleanup();
	// TODO: Add your extra methods if needed
};

class BuiltInCommand : public Command {
public:
	BuiltInCommand(const char* cmd_line);

	virtual ~BuiltInCommand() {
	}
};

class ChangePromptCommand : public BuiltInCommand {
public:
	ChangePromptCommand(const char* cmd_line);

	virtual ~ChangePromptCommand() {
	}

	void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
	ShowPidCommand(const char* cmd_line);

	virtual ~ShowPidCommand() {
	}

	void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
	GetCurrDirCommand(const char* cmd_line);

	virtual ~GetCurrDirCommand() {
	}

	void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
public:
	ChangeDirCommand(const char* cmd_line)
		: BuiltInCommand(cmd_line) {}

	virtual ~ChangeDirCommand() {}

	void execute() override;
};

class ExternalCommand : public Command {
public:
	ExternalCommand(const char* cmd_line);

	virtual ~ExternalCommand() {
	}

	void execute() override;
};

class PipeCommand : public Command {
	// TODO: Add your data members
public:
	PipeCommand(const char* cmd_line);

	virtual ~PipeCommand() {
	}

	void execute() override;
};

class RedirectionCommand : public Command {
	// TODO: Add your data members
public:
	explicit RedirectionCommand(const char* cmd_line);

	virtual ~RedirectionCommand() {
	}

	void execute() override;
};

class JobsList;

class QuitCommand : public BuiltInCommand {
	// TODO: Add your data members public:
	QuitCommand(const char* cmd_line, JobsList* jobs);

	virtual ~QuitCommand() {
	}

	void execute() override;
};

class JobsList {
public:
	class JobEntry {
		// TODO: Add your data members
	};

	// TODO: Add your data members
public:
	JobsList();

	~JobsList();

	void addJob(Command* cmd, bool isStopped = false);

	void printJobsList();

	void killAllJobs();

	void removeFinishedJobs();

	JobEntry* getJobById(int jobId);

	void removeJobById(int jobId);

	JobEntry* getLastJob(int* lastJobId);

	JobEntry* getLastStoppedJob(int* jobId);

	// TODO: Add extra methods or modify exisitng ones as needed
};

class JobsCommand : public BuiltInCommand {
	// TODO: Add your data members
public:
	JobsCommand(const char* cmd_line, JobsList* jobs);

	virtual ~JobsCommand() {
	}

	void execute() override;
};

class KillCommand : public BuiltInCommand {
	// TODO: Add your data members
public:
	KillCommand(const char* cmd_line, JobsList* jobs);

	virtual ~KillCommand() {
	}

	void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
	// TODO: Add your data members
public:
	ForegroundCommand(const char* cmd_line, JobsList* jobs);

	virtual ~ForegroundCommand() {
	}

	void execute() override;
};

class ListDirCommand : public Command {
public:
	ListDirCommand(const char* cmd_line);

	virtual ~ListDirCommand() {
	}

	void execute() override;
};

class WhoAmICommand : public Command {
public:
	WhoAmICommand(const char* cmd_line);

	virtual ~WhoAmICommand() {
	}

	void execute() override;
};

class NetInfo : public Command {
	// TODO: Add your data members
public:
	NetInfo(const char* cmd_line);

	virtual ~NetInfo() {
	}

	void execute() override;
};

class aliasCommand : public BuiltInCommand {
public:
	aliasCommand(const char* cmd_line);

	virtual ~aliasCommand() {
	}

	void execute() override;
};

class unaliasCommand : public BuiltInCommand {
public:
	unaliasCommand(const char* cmd_line);

	virtual ~unaliasCommand() {
	}

	void execute() override;
};

class SmallShell {
private:
	std::string m_prompt;
	char* lastPwd;
	JobsList jobsList;

	SmallShell() : m_prompt("smash"), lastPwd(nullptr) {}

public:
	Command* CreateCommand(const char* cmd_line);

	char* my_strdup(const char* str) {
		if (!str) {
			return nullptr;
		}
		char* dup = (char*)malloc(strlen(str) + 1); // Allocate memory
		if (!dup) {
			return nullptr; // Return nullptr if allocation fails
		}
		strcpy(dup, str); // Copy the string
		return dup;
	}

	SmallShell(SmallShell const&) = delete; // Disable copy constructor

	void operator=(SmallShell const&) = delete; // Disable assignment operator

	static SmallShell& getInstance() // Make SmallShell singleton
	{
		static SmallShell instance; // Guaranteed to be destroyed.
		// Instantiated on first use.
		return instance;
	}

	~SmallShell() {
		if (lastPwd) {
			free(lastPwd); // Free memory allocated for lastPwd
		}
	}

	void executeCommand(const char* cmd_line);

	std::string getPrompt() const {
		return m_prompt;
	}

	void setPrompt(const std::string& newPrompt) {
		m_prompt = newPrompt;
	}

	// Getter for lastPwd
	char* getLastPwd() const {
		return lastPwd;
	}

	// Setter for lastPwd
	void setLastPwd(const char* newPwd) {
		if (lastPwd) {
			free(lastPwd); // Free old memory
		}
		lastPwd = my_strdup(newPwd);
	}
		
	JobsList& getJobsList() {
		return jobsList;
	}
};

#endif //SMASH_COMMAND_H_