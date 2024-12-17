#ifndef SMASH_COMMANDS_H_
#define SMASH_COMMANDS_H_


#include <string>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <signal.h>


// constants
constexpr int COMMAND_MAX_LENGTH = 200;
constexpr int COMMAND_MAX_ARGS = 20;


std::string _trim(const std::string& str);
int _parseCommandLine(const std::string& cmd_line, char** args);
bool isDirectory(const std::string& path);
void _trimAmp(std::string& cmd_line);


class JobsList;

class Command {
protected:
    std::vector<std::string> cmdSegments;
    int processId;
    bool isBackground;
    std::string cmdLine;
    std::string alias;
    std::string fileRedirect;

public:
    Command(const char* cmd_line);
    virtual ~Command();
    virtual void execute() = 0;
    int getProcessId() const;
    void setProcessId(int pid);
    std::string getAlias() const;
    void setAlias(const std::string& aliasCommand);
    std::string getPath() const;
    void setPath(const std::string& path);
    std::string getCommandLine() const;
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
private:
    std::string& prompt;

public:
    ChangePromptCommand(const char* cmd_line, std::string& prompt);
    ~ChangePromptCommand();
    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
public:
    ChangeDirCommand(const char* cmd_line);
    ~ChangeDirCommand();
    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
    ShowPidCommand(const char* cmd_line);
    ~ShowPidCommand();
    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    GetCurrDirCommand(const char* cmd_line);
    ~GetCurrDirCommand();
    void execute() override;
};

class JobsList {
public:
    class JobEntry {
    public:
        int jobId;
        int pid;
        bool isStopped;
        std::string command;

        JobEntry(int jobId, int pid, const std::string& command, bool isStopped)
            : jobId(jobId), pid(pid), isStopped(isStopped), command(command) {}
        ~JobEntry() {}
    };

private:
    std::list<JobEntry*> jobs;
    int lastJobId;

public:
    JobsList();
    ~JobsList();
    int size() const;
    const std::list<JobEntry*>& getJobs() const;
    void addJob(const std::string& command, int pid, bool isStopped = false);
    void printJobs() const;
    void removeFinishedJobs();
    JobEntry* getJobById(int jobId);
    void removeJobById(int jobId);
    void killAllJobs();
};

class JobsCommand : public BuiltInCommand {
private:
    JobsList* m_jobsList;

public:
    JobsCommand(const char* cmd_line, JobsList* jobsList);
    virtual ~JobsCommand();
    void execute() override;
};

class KillCommand : public BuiltInCommand {
    JobsList* jobsList;

public:
    KillCommand(const char* cmd_line, JobsList* jobs);
    ~KillCommand();
    void execute() override;
};

class QuitCommand : public BuiltInCommand {
    JobsList* jobsList;

public:
    QuitCommand(const char* cmd_line, JobsList* jobs);
    ~QuitCommand();
    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
    JobsList* jobsList;

public:
    ForegroundCommand(const char* cmd_line, JobsList* jobs);
    ~ForegroundCommand();
    void execute() override;
};


class AliasCommand : public BuiltInCommand {
private:
    std::map<std::string, std::string>& aliasMap;

public:
    AliasCommand(const char* cmd_line, std::map<std::string, std::string>& aliasMap);
    ~AliasCommand();
    void execute() override;
};

class UnaliasCommand : public BuiltInCommand {
private:
    std::map<std::string, std::string>& aliasMap;

public:
    UnaliasCommand(const char* cmd_line, std::map<std::string, std::string>& aliasMap);
    ~UnaliasCommand();
    void execute() override;
};

class RedirectionCommand : public BuiltInCommand {
private:
    bool m_isAppend;

public:
    RedirectionCommand(const char* cmd_line);
    ~RedirectionCommand();
    void execute() override;
};

class ListDirCommand : public BuiltInCommand {
public:
    ListDirCommand(const char* cmd_line);
    virtual ~ListDirCommand() {}
    void execute() override;

private:
    void listDirectoryRecursively(const std::string& path, const std::string& indent = "") const;
};

class SmallShell {
private:
    std::string prompt;
    std::string lastWorkingDir;
    std::string prevWorkingDir;
    JobsList jobs;
    int foregroundPid;
    std::string foregroundCommand;
    std::map<std::string, std::string> aliasMap;
    SmallShell();

public:
    ~SmallShell();
    static SmallShell& getInstance();
    Command* createCommand(const char* cmd_line);
    void executeCommand(const char* cmd_line);
    std::string getLastWorkingDir() const;
    void setLastWorkingDir(const std::string& dir);
    std::string getPwd() const;
    std::string getPrompt() const;
    void setPrompt(const std::string& newPrompt);
    void updateWorkingDirectory(const std::string& newDir);
    void setForegroundJob(int pid, const std::string& command);
    void clearForegroundJob();
    int getForegroundPid() const;
    std::string getForegroundCommand() const;
    JobsList& getJobsList();
    void setAlias(const std::string& aliasName, const std::string& aliasCommand);
    void removeAlias(const std::string& aliasName);
    std::string getAlias(const std::string& aliasName) const;
    void printAliases() const;
};

#endif // SMASH_COMMANDS_H_
