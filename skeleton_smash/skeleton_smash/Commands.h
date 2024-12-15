#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <cstring>
#include <list>
#include <map>

#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

class Command {
protected:
	std::vector <std::string> m_command_seg;
	const char* m_cmd_line;

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
private:
	std::string m_cmdLine;
	bool m_isBackground;
	bool isComplexCommand(const std::string& cmd_line);

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
private:
	JobsList* m_jobsList;
	bool m_kill;

public:
	QuitCommand(const char* cmd_line, JobsList* jobsList, bool kill = false);

	virtual ~QuitCommand();

	void execute() override;
};

class JobsList {
public:
	class JobEntry {
	public:
		int m_jobId;
		std::string m_cmdLine;
		std::string m_command;
		int m_pid;
		bool m_isStopped;

		JobEntry(int jobId, const std::string& cmdLine, int pid, bool isStopped)
			: m_jobId(jobId), m_cmdLine(cmdLine), m_command(cmdLine), m_pid(pid), m_isStopped(isStopped) {}
	};

private:
	std::vector<JobEntry> m_jobs;
	int m_lastJobId;


public:
	JobsList();

	~JobsList();

	void addJob(Command* cmd, bool isStopped = false);

	void printJobsList();

	void killAllJobs();

	void removeFinishedJobs();

	JobEntry* getJobById(int jobId);

	void removeJobById(int jobId);

	JobEntry* getLastJob();

	JobEntry* getLastStoppedJob(int* jobId);

	bool isEmpty() const;

	void printJobs() const;

	std::vector<JobEntry>& getJobs();


};

class JobsCommand : public BuiltInCommand {
private:
	JobsList* m_jobs;

public:
	JobsCommand(const char* cmd_line, JobsList* jobs);

	virtual ~JobsCommand();

	void execute() override;
};

class KillCommand : public BuiltInCommand {
private:
	int m_signum;
	int m_jobId;
	JobsList* m_jobsList; 

public:
	KillCommand(const char* cmd_line, JobsList* jobs);

	virtual ~KillCommand();

	void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
private:
	JobsList* m_jobs;
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
private:
	std::map<std::string, std::string> m_aliases;
	std::string m_name;
	std::string m_command;

public:
	aliasCommand(const char* cmd_line, std::map<std::string, std::string>& aliases);

	virtual ~aliasCommand();

	void execute() override;

	bool isValidAliasName(const std::string& name);

	void listAliases() const;

	void createAlias();
};

class unaliasCommand : public BuiltInCommand {
private:
	std::map<std::string, std::string>& m_aliases;
	std::vector<std::string> m_names;

public:
	unaliasCommand(const char* cmd_line, std::map<std::string, std::string>& aliases);
	
	~unaliasCommand();
	
	void execute() override;
	
	void removeAliases(const std::vector<std::string>& aliasNames);  // Remove specified aliases
};

class SmallShell {
private:
	std::string m_prompt;
	JobsList jobsList;
	int m_pid;
	std::string m_currDir;
	std::string m_prevDir;
	std::map<std::string, std::string> m_aliases;

	SmallShell();

public:
	~SmallShell();

	SmallShell(SmallShell const&) = delete;

	void operator=(SmallShell const&) = delete;

	static SmallShell& getInstance();
	
	Command* CreateCommand(const char* cmd_line);

	void executeCommand(const char* cmd_line);

	std::string getPrompt() const;

	void setPrompt(const std::string& newPrompt);

	std::string getPrevPwd() const;
		
	JobsList& getJobsList();

	int getSmashPid();

	std::string getSmashPwd();

	void changePwd(std::string pwd);

	void addAlias(const std::string& name, const std::string& command);

	void removeAlias(const std::string& name);

	void listAliases();
};

#endif //SMASH_COMMAND_H_
