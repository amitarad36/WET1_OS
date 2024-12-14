#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <cstring>
#include <list>
#include <map>

#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
#define MAX_SIZE 256

class Command {
protected:
	std::vector <std::string> cmd_segments; //cpoied!!!!!!!!!
	int processId; //cpoied!!!!!!!!!
	bool backRound; //cpoied!!!!!!!!!
	const char* m_cmd_line;
	std::string aliasChar; //cpoied!!!!!!!!!
	std::string fileDirectedForExternal; //cpoied!!!!!!!!!

public:
	Command(const char* cmd_line);
	
	virtual ~Command();
	
	std::string getCommandLine();
	
	virtual void execute() = 0;

	//virtual void prepare();
	//virtual void cleanup();
	// TODO: Add your extra methods if needed
};

class BuiltInCommand : public Command {
public:
	BuiltInCommand(const char* cmd_line);

	virtual ~BuiltInCommand();
};

class ExternalCommand : public Command {
public:
	ExternalCommand(const char* cmd_line);

	virtual ~ExternalCommand();

	void execute() override;
};

class ChangePromptCommand : public BuiltInCommand {
public:
	ChangePromptCommand(const char* cmd_line);

	virtual ~ChangePromptCommand();

	void execute() override;
};

class PipeCommand : public Command {
	// TODO: Add your data members
public:
	PipeCommand(const char* cmd_line);

	virtual ~PipeCommand();

	void execute() override;
};

class RedirectionCommand : public Command {
	// TODO: Add your data members
public:
	explicit RedirectionCommand(const char* cmd_line);

	virtual ~RedirectionCommand();

	void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
public:
	ChangeDirCommand(const char* cmd_line);

	virtual ~ChangeDirCommand();

	void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
	GetCurrDirCommand(const char* cmd_line);

	virtual ~GetCurrDirCommand();

	void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
	ShowPidCommand(const char* cmd_line);

	virtual ~ShowPidCommand();

	void execute() override;
};

class JobsList;

class QuitCommand : public BuiltInCommand {
	// TODO: Add your data members public:
public:
	QuitCommand(const char* cmd_line, JobsList* jobs);

	virtual ~QuitCommand();

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
private:
	JobsList* jobs;

public:
	JobsCommand(const char* cmd_line, JobsList* jobs);

	virtual ~JobsCommand();

	void execute() override;
};

class KillCommand : public BuiltInCommand {
	// TODO: Add your data members
public:
	KillCommand(const char* cmd_line, JobsList* jobs);

	virtual ~KillCommand();

	void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
	// TODO: Add your data members
public:
	ForegroundCommand(const char* cmd_line, JobsList* jobs);

	virtual ~ForegroundCommand();

	void execute() override;
};

class ListDirCommand : public Command {
public:
	ListDirCommand(const char* cmd_line);

	virtual ~ListDirCommand();

	void execute() override;
};

class WhoAmICommand : public Command {
public:
	WhoAmICommand(const char* cmd_line);

	virtual ~WhoAmICommand();

	void execute() override;
};

class NetInfo : public Command {
	// TODO: Add your data members
public:
	NetInfo(const char* cmd_line);

	virtual ~NetInfo();

	void execute() override;
};

class aliasCommand : public BuiltInCommand {
public:
	aliasCommand(const char* cmd_line);

	virtual ~aliasCommand();

	void execute() override;
};

class unaliasCommand : public BuiltInCommand {
public:
	unaliasCommand(const char* cmd_line);

	virtual ~unaliasCommand();

	void execute() override;
};

class SmallShell {
private:
	std::string m_prompt;
	char* m_lastPwd;
	JobsList jobsList;
	int m_pid;
	std::string m_currDir;


	SmallShell();

public:
	~SmallShell();

	char* my_strdup(const char* str);

	SmallShell(SmallShell const&) = delete;

	void operator=(SmallShell const&) = delete;

	static SmallShell& getInstance();
	
	Command* CreateCommand(const char* cmd_line);

	void executeCommand(const char* cmd_line);

	std::string getPrompt() const;

	void setPrompt(const std::string& newPrompt);

	char* getLastPwd() const;

	void setLastPwd(const char* newPwd);
		
	JobsList& getJobsList();

	int getSmashPid();

	std::string getSmashPwd();
};

#endif //SMASH_COMMAND_H_