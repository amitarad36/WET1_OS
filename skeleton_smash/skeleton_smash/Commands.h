#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <cstring>
#include <string>
#include <iostream>

#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

class Command {
protected:
    std::string m_cmd_line;

public:
    Command(const char* cmd_line) : m_cmd_line(cmd_line) {}
    virtual ~Command() {}

    virtual void execute() = 0;
};

class BuiltInCommand : public Command {
public:
    BuiltInCommand(const char* cmd_line) : Command(cmd_line) {}
    virtual ~BuiltInCommand() {}
};

class ChangePromptCommand : public BuiltInCommand {
public:
    ChangePromptCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}
    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
    ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}
    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    GetCurrDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}
    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
public:
    ChangeDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}
    void execute() override;
};

class ExternalCommand : public Command {
public:
    ExternalCommand(const char* cmd_line) : Command(cmd_line) {}
    void execute() override;
};

class JobsList;

class JobsCommand : public BuiltInCommand {
public:
    JobsCommand(const char* cmd_line, JobsList* jobs);
    void execute() override;
};

class SmallShell {
private:
    std::string m_prompt;
    char* lastPwd;
    JobsList jobsList;

    SmallShell() : m_prompt("smash"), lastPwd(nullptr) {}

public:
    static SmallShell& getInstance() {
        static SmallShell instance;
        return instance;
    }

    Command* CreateCommand(const char* cmd_line);
    void executeCommand(const char* cmd_line);

    std::string getPrompt() const { return m_prompt; }
    void setPrompt(const std::string& newPrompt) { m_prompt = newPrompt; }

    char* getLastPwd() const { return lastPwd; }
    void setLastPwd(const char* newPwd);

    JobsList& getJobsList() { return jobsList; }

    // Disable copy constructor and assignment operator
    SmallShell(const SmallShell&) = delete;
    void operator=(const SmallShell&) = delete;

    ~SmallShell() {
        if (lastPwd) {
            free(lastPwd);
        }
    }
};

#endif //SMASH_COMMAND_H_
