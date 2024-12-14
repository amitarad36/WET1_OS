#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <cstring>
#include <list> // not used
#include <map> // not used

#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
#define MAX_SIZE 256 // not used

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
	QuitCommand(const char* cmd_line, JobsList* jobs);

	virtual ~QuitCommand();

	void execute() override;
};

class JobsList {
public:
	class JobEntry {
	public:
		int jobId;               // Unique job ID
		pid_t pid;               // Process ID of the job
		std::string command;     // The original command line
		time_t startTime;        // Timestamp of when the job was added
		bool isStopped;          // Whether the job is stopped

		JobEntry(int id, pid_t pid, const std::string& cmd, bool stopped)
			: jobId(id), pid(pid), command(cmd), startTime(time(nullptr)), isStopped(stopped) {}
	};

private:
	std::vector<JobEntry> jobs; // Vector of active jobs
	int nextJobId;              // Tracks the next available job ID

public:
	JobsList();

	~JobsList();

	void addJob(Command* cmd, pid_t pid, bool isStopped = false) {
		removeFinishedJobs(); // Clean up finished jobs before adding
		jobs.emplace_back(nextJobId++, pid, cmd->getCommandLine(), isStopped);
	}

	void printJobsList() {
		removeFinishedJobs(); // Clean up finished jobs before printing
		std::sort(jobs.begin(), jobs.end(), [](const JobEntry& a, const JobEntry& b) {
			return a.jobId < b.jobId; // Sort by jobId
			});

		for (const auto& job : jobs) {
			std::cout << "[" << job.jobId << "] " << job.command;
			if (job.isStopped) {
				std::cout << " (stopped)";
			}
			std::cout << std::endl;
		}
	}

	void removeFinishedJobs() {
		for (auto it = jobs.begin(); it != jobs.end(); ) {
			int status;
			if (waitpid(it->pid, &status, WNOHANG) > 0) {
				it = jobs.erase(it); // Remove finished jobs
			}
			else {
				++it;
			}
		}
	}

	JobEntry* getJobById(int jobId) {
		for (auto& job : jobs) {
			if (job.jobId == jobId) {
				return &job;
			}
		}
		return nullptr;
	}

	void removeJobById(int jobId) {
		jobs.erase(std::remove_if(jobs.begin(), jobs.end(), [jobId](const JobEntry& job) {
			return job.jobId == jobId;
			}), jobs.end());
	}

	JobEntry* getLastJob(int* lastJobId) {
		if (jobs.empty()) {
			return nullptr;
		}
		*lastJobId = jobs.back().jobId;
		return &jobs.back();
	}

	JobEntry* getLastStoppedJob(int* jobId) {
		for (auto it = jobs.rbegin(); it != jobs.rend(); ++it) {
			if (it->isStopped) {
				*jobId = it->jobId;
				return &(*it);
			}
		}
		return nullptr;
	}
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
	char* lastPwd;
	JobsList jobsList;

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
};

#endif //SMASH_COMMAND_H_