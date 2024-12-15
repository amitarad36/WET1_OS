#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <string>
#include <vector>

#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
#define WHITESPACE " \n\r\t\f\v"

// Utility functions
std::string _ltrim(const std::string& s);

std::string _rtrim(const std::string& s);

std::string _trim(const std::string& s);

bool _isBackgroundCommand(const char* cmd_line);

void _removeBackgroundSign(char* cmd_line);

int _parseCommandLine(const char* cmd_line, char** args);

// Command class hierarchy
class Command {
protected:
    const char* m_cmd_line;

public:
    Command(const char* cmd_line);
    virtual ~Command();
    virtual void execute() = 0;
    std::string getCommandLine() const;
};

class BuiltInCommand : public Command {
public:
    BuiltInCommand(const char* cmd_line);
    virtual ~BuiltInCommand();
};

class ChangePromptCommand : public BuiltInCommand {
private:
    std::string& m_prompt;

public:
    ChangePromptCommand(const char* cmd_line, std::string& prompt);
    virtual ~ChangePromptCommand();
    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
    ShowPidCommand(const char* cmd_line);
    virtual ~ShowPidCommand();
    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    GetCurrDirCommand(const char* cmd_line);
    virtual ~GetCurrDirCommand();
    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
private:
    std::string& m_lastWorkingDir;

public:
    ChangeDirCommand(const char* cmd_line, std::string& lastWorkingDir);
    virtual ~ChangeDirCommand();
    void execute() override;
};

class JobsCommand : public BuiltInCommand {
private:
    JobsList& m_jobsList;

public:
    JobsCommand(const char* cmd_line, JobsList& jobsList);
    virtual ~JobsCommand();
    void execute() override;
};

class ExternalCommand : public Command {
private:
    std::string m_cmdLine;
    bool m_isBackground;

public:
    ExternalCommand(const char* cmd_line);
    virtual ~ExternalCommand();
    void execute() override;
};

class JobsList {
public:
    class JobEntry {
    public:
        int m_jobId;
        std::string m_command;
        int m_pid;
        bool m_isStopped;

        JobEntry(int jobId, const std::string& command, int pid, bool isStopped);
    };

private:
    std::vector<JobEntry> m_jobs;
    int m_lastJobId;

public:
    JobsList();
    ~JobsList();
    void addJob(int pid, const std::string& command, bool isStopped);
    void removeFinishedJobs();
    void printJobsList() const;
    void killAllJobs();
};

class SmallShell {
private:
    std::string m_prompt;
    std::string m_lastWorkingDir;
    JobsList m_jobsList;
    int m_foregroundPid;
    std::string m_foregroundCommand;

    SmallShell();

public:
    ~SmallShell();
    static SmallShell& getInstance();
    std::string getPrompt() const;
    void setPrompt(const std::string& prompt);
    void setForegroundJob(int pid, const std::string& command);
    void clearForegroundJob();
    int getForegroundPid() const;
    std::string getForegroundCommand() const;
    JobsList& getJobsList();

    Command* CreateCommand(const char* cmd_line);
    void executeCommand(const char* cmd_line);
};

#endif // SMASH_COMMAND_H_
