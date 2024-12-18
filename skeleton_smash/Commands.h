#ifndef SMASH_COMMANDS_H_
#define SMASH_COMMANDS_H_


#include <string>
#include <vector>
#include <list>
#include <map>
#include <set>


// Constants
constexpr int COMMAND_MAX_LENGTH = 200;
constexpr int COMMAND_MAX_ARGS = 20;


// Utility
std::string _trim(const std::string& str);
int _parseCommandLine(const std::string& cmd_line, char** args);
bool isDirectory(const std::string& path);
void _trimAmp(std::string& cmd_line);


// Signal handling function prototypes
void ctrlCHandler(int sig_num);
void sigchldHandler(int sig_num);
void setupSignals();


class JobsList;

class Command {
protected:
    std::vector<std::string> cmdSegments;
    int m_PID;
    bool m_is_background;
    std::string m_cmd_line;
    std::string alias;
    std::string m_file_redirect;

public:
    Command(const char* cmd_line);
    virtual ~Command();
    virtual void execute() = 0;
    int getm_PID() const;
    void setm_PID(int pid);
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
        int m_job_id;
        int pid;
        bool m_is_stopped;
        std::string command;

        JobEntry(int m_job_id, int pid, const std::string& command, bool m_is_stopped)
            : m_job_id(m_job_id), pid(pid), m_is_stopped(m_is_stopped), command(command) {}
        ~JobEntry() {}
    };

private:
    std::list<JobEntry*> jobs;
    int lastm_job_id;

public:
    JobsList();
    ~JobsList();
    int size() const;
    const std::list<JobEntry*>& getJobs() const;
    void addJob(const std::string& command, int pid, bool m_is_stopped = false);
    void printJobs() const;
    void removeFinishedJobs();
    JobEntry* getJobById(int m_job_id);
    void removeJobById(int m_job_id);
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
    virtual ~ListDirCommand();
    void execute() override;

private:
    void listDirectoryRecursively(const std::string& path, const std::string& indent = "") const;
};

class WhoamiCommand : public BuiltInCommand {
public:
    explicit WhoamiCommand(const char* cmd_line);
    virtual ~WhoamiCommand() = default;
    void execute() override;
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
    Command* generateCommand(const char* cmd_line);
    void executeCommand(const char* cmd_line);
    std::string getLastDir() const;
    void setLastDir(const std::string& dir);
    std::string getPwd() const;
    std::string getPrompt() const;
    void setPrompt(const std::string& newPrompt);
    void updateWorkingDir(const std::string& newDir);
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
