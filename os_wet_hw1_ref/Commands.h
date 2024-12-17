#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_
#define MAX_SIZE 256
#include <vector>
#include <list>
#include <map>
#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

struct direntInfo {
	long           direntInformation;
	off_t          direntOff;
	unsigned short direntRec;
	char           direntName[];
};

class Command {
protected:
	std::vector<std::string>cmd_segments;
	int processId;
	bool backRound;
	const char* cmd_line;
	std::string aliasChar;
	std::string fileDirectedForExternal;
public:
	Command(const char* cmd_line, std::string aliasCommand = "", std::string fileDirectedForExternal = "");

	virtual ~Command();

	virtual void execute() = 0;

	int getProcessPid();
	void setProcessId(int pid);
	//virtual void prepare();
	//virtual void cleanup();
	// TODO: Add your extra methods if needed

	std::string printCommand();
	bool isAlias();
	std::string getAliasCommand();
	void setAliasCommand(std::string command);
	std::string getPath();
	void setPath(std::string path);
};

class BuiltInCommand : public Command {
public:
	BuiltInCommand(const char* cmd_line);

	virtual ~BuiltInCommand();
};

class ExternalCommand : public Command {
public:
	std::vector<std::string>argc;
	ExternalCommand(const char* cmd_line, std::string cmd_s, bool isBackround);

	virtual ~ExternalCommand() {}

	void execute() override;
};

class DemiAliasCommand : public Command {
public:
	std::string wordToChange;
	DemiAliasCommand(const char* cmd_line, std::string wordToChange, bool isBackround);

	virtual ~DemiAliasCommand() {}

	void execute() override;
};
class PipeCommand : public Command {
	// TODO: Add your data members
public:
	PipeCommand(const char* cmd_line) :Command(cmd_line)
	{

	}

	virtual ~PipeCommand() {}

	void execute() override;
};
class WatchCommand : public Command {
	// TODO: Add your data members
public:
	WatchCommand(const char* cmd_line) :Command(cmd_line)
	{

	}

	virtual ~WatchCommand() {}

	void execute() override;
};
class RedirectionCommand : public Command {
	// TODO: Add your data members
	std::string relPath;
public:
	explicit RedirectionCommand(const char* cmd_line);

	virtual ~RedirectionCommand() {}

	void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
	// TODO: Add your data members public:
public:
	ChangeDirCommand(const char* cmd_line) :BuiltInCommand(cmd_line)
	{

	}

	virtual ~ChangeDirCommand()
	{

	}

	void execute() override;
};
class GetCurrDirCommand : public BuiltInCommand {
public:
	GetCurrDirCommand(const char* cmd_line) :BuiltInCommand(cmd_line)
	{

	}

	virtual ~GetCurrDirCommand() {}

	void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
	ShowPidCommand(const char* cmd_line) :BuiltInCommand(cmd_line)
	{

	}

	virtual ~ShowPidCommand() {}

	void execute() override;
};

class JobsList;

class QuitCommand : public BuiltInCommand {
	// TODO: Add your data members public:
public:
	JobsList* jobs;
	QuitCommand(const char* cmd_line, JobsList* jobs);

	virtual ~QuitCommand() {}

	void execute() override;
};

class JobsList {
public:
	class JobEntry {
	public:
		Command* cmd;
		bool isStopped;
		int jobId;
		int pid;
		std::string command;
		JobEntry(Command* cmd, bool isStopped, int jobId, int pid, std::string command) :cmd(cmd), isStopped(isStopped), jobId(jobId), pid(pid), command(command)
		{

		}
		~JobEntry() = default;
		std::string getDetails();
	};
	std::list<JobEntry*> list;
	// TODO: Add your data members
public:
	JobsList() :list()
	{

	}

	~JobsList();

	void addJob(Command* cmdCurr, int pidCurr, bool isStopped = false);

	void printJobsList();

	void killAllJobs();

	void removeFinishedJobs();

	JobEntry* getJobById(int jobId);

	void removeJobById(int jobId);

	JobEntry* getLastJob();
	
	void printJobsForQuit();
};

class JobsCommand : public BuiltInCommand {
	// TODO: Add your data members
	JobsList* jobs;
public:
	JobsCommand(const char* cmd_line, JobsList* jobs);

	virtual ~JobsCommand() {}

	void execute() override;
};

class KillCommand : public BuiltInCommand {
	// TODO: Add your data members
	JobsList* jobs;
public:
	KillCommand(const char* cmd_line, JobsList* jobs);

	virtual ~KillCommand() {}

	void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
	// TODO: Add your data members
	JobsList* jobs;
public:
	ForegroundCommand(const char* cmd_line, JobsList* jobs);

	virtual ~ForegroundCommand() {}

	void execute() override;
};

class ListDirCommand : public BuiltInCommand {
public:
	ListDirCommand(const char* cmd_line) :BuiltInCommand(cmd_line)
	{

	}

	virtual ~ListDirCommand() {}

	void execute() override;
};

class GetUserCommand : public BuiltInCommand {
public:
	GetUserCommand(const char* cmd_line);

	virtual ~GetUserCommand() {}

	void execute() override;
};

class aliasCommand : public BuiltInCommand {
public:
	aliasCommand(const char* cmd_line) :BuiltInCommand(cmd_line)
	{

	}

	virtual ~aliasCommand() {}

	void execute() override;
};

class unaliasCommand : public BuiltInCommand {
public:
	unaliasCommand(const char* cmd_line) :BuiltInCommand(cmd_line)
	{

	}

	virtual ~unaliasCommand() {}

	void execute() override;
};

class changeSmashPrompt : public BuiltInCommand {
public:

	std::string newPrompt;

	changeSmashPrompt(const char* cmd_line) :BuiltInCommand(cmd_line) {};

	virtual ~changeSmashPrompt() {}

	void execute() override;
};

class SmallShell {
private:
	// TODO: Add your data members
	std::string prompt;
	int pid;
	std::string workingDirectory;
	std::string prevworkingDirectory;
	JobsList* job;
	std::map<std::string, std::string>alias;
	std::vector<std::string> aliasInOrder;
	int currentFrontPid;
	std::streambuf* coutdirect;
	std::vector<std::string>commandString;
	char* cmd_lineForWatchCommand;
	int inDup;
	int outDup;
	int errDup;
	int watchFrontPid;
	int watchBackPid;
	SmallShell();

public:
	static const int stdIn = STDIN_FILENO;
	static const int stdOut = STDOUT_FILENO;
	static const int stdErr = STDERR_FILENO;
	Command* CreateCommand(const char* cmd_line);

	SmallShell(SmallShell const&) = delete; // disable copy ctor
	void operator=(SmallShell const&) = delete; // disable = operator
	static SmallShell& getInstance() // make SmallShell singleton
	{
		static SmallShell instance; // Guaranteed to be destroyed.
		// Instantiated on first use.
		return instance;
	}
	void setPrompt(std::string newPrompt);
	std::string getPrompt();
	int getShellPid();
	std::string getPWD();
	std::string getPrevPWD();
	void changePWD(std::string newPWD);
	JobsList* getJobs();
	std::string getAlias(std::string name);
	void setAlias(std::string name, std::string source);
	void removeAlias(std::string name);
	void getAllAlias(std::vector<std::string>& vec);
	void bringProcessToFront(int pid);
	void leaveProcess();
	void createCommandVector();
	bool validCommand(std::string name);
	void runWatchCommand();
	int getWatchInForegroundPid();
	void setWatchInForegroundPid(int pid);
	int getBackRoundWatch();
	void setbackGroundWatch(int back);
	int getOriginalOut();

	void smashControlCSigHandler();

	~SmallShell();

	void executeCommand(const char* cmd_line, std::string aliasCommand = "", std::string externalPath = "");
};

#endif //SMASH_COMMAND_H_
