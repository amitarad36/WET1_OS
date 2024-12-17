#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <iomanip>
#include "Commands.h"
#include <sstream>
#include <dirent.h>
#include <cstring>
#include <cerrno>
#include <pwd.h>
#include <grp.h>
#include <algorithm>
#include <chrono>
#include <thread>
#include <regex>

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

string _ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s) {
    return _rtrim(_ltrim(s));
}

void _removeBackgroundSignTemp(std::string &cmd_line)
    {
        cmd_line = _trim(cmd_line);
        if (cmd_line[cmd_line.length() - 1] == '&')
        {
            cmd_line = cmd_line.substr(0, cmd_line.length() - 1);
        }
    }
int _parseCommandLine(const char *cmd_line, char **args) {
    FUNC_ENTRY()
    int i = 0;
    std::string cmd_s=_trim(string(cmd_line));
    _removeBackgroundSignTemp(cmd_s);
    std::istringstream iss(cmd_s.c_str());
    for (std::string s; iss >> s;) {
        args[i] = (char *) malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;

    FUNC_EXIT()
}

bool _isBackgroundComamnd(const char *cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line) {
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

// TODO: Add your implementation for classes in Commands.h 
void createVectorForLine(const char* cmd_line,std::vector<std::string>& line)
{
    std::string temp=cmd_line;
    std::string toAdd;
    stringstream stringLine(temp);
    while (getline(stringLine, toAdd, ' ')) {  
      if(!toAdd.empty())
      {
        line.push_back(toAdd);
      }
    }  
}

bool specialCommand(const char* cmd_line)
{
  std::string temp=string(cmd_line);
  if(temp.find('*')!=std::string::npos || temp.find('?')!=std::string::npos)
  {
    return true;
  }
  return false;
}
std::string SmallShell::getPWD()
{
    char place[COMMAND_MAX_LENGTH];
    getcwd(place, COMMAND_MAX_LENGTH);
    return std::string(place);
}

SmallShell::SmallShell():prompt("smash"),pid(getpid()),workingDirectory(getPWD()),prevworkingDirectory(""),job(new JobsList),alias(),aliasInOrder(),currentFrontPid(-1),coutdirect(std::cout.rdbuf()),commandString(),cmd_lineForWatchCommand(),inDup(dup(stdIn)),outDup(dup(stdOut)),errDup(dup(stdErr)),watchFrontPid(-1),watchBackPid(-1)
{
// TODO: add your implementation
}

SmallShell::~SmallShell() {
// TODO: add your implementation
  delete job;
  std::cout.rdbuf(coutdirect);
  dup2(stdOut,1);
}
int SmallShell::getWatchInForegroundPid()
{
  return watchFrontPid;
}
void SmallShell::setWatchInForegroundPid(int pid)
{
  watchFrontPid=pid;
}
int SmallShell::getBackRoundWatch()
{
  return watchBackPid;
}
void SmallShell::setbackGroundWatch(int back)
{
  watchBackPid=back;
}
int SmallShell::getOriginalOut()
{
  return outDup;
}
bool validLetters(std::string name)
{
  std::string availableLetter="abcdefghigklmnopkrstuvwxyz1234567890_";
  for(char c:name)
  {
    if(availableLetter.find(c)==std::string::npos)
    {
      return false;
    }
  }
  return true;
}
bool SmallShell::validCommand(std::string name)
{
  for(std::string cmd:commandString)
  {
    if(name.compare(cmd)==0)
    {
      return true;
    }
  }
  return false;
}
/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/

Command *SmallShell::CreateCommand(const char *cmd_line) {
    // For example:
  SmallShell::getInstance().leaveProcess();
  std::string cmd_s=_trim(string(cmd_line));
  if(cmd_s.size()==0)
  {
    return nullptr;
  }
  _removeBackgroundSignTemp(cmd_s);
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
  bool isBackround=_isBackgroundComamnd(cmd_line);
  if(firstWord.compare("alias")==0)
  {
    return new aliasCommand(cmd_line);
  }
  else if(firstWord.compare("watch")==0)
  {
    return new WatchCommand(cmd_line);
  }
  else if(cmd_s.find("|")!=std::string::npos) // a wild pipe has appeared
  {
    return new PipeCommand(cmd_line);
  }
  else if(firstWord.compare("chprompt")==0)
  {
    return new changeSmashPrompt(cmd_line);
  }
  else if(cmd_s.find(">")!=std::string::npos && getAlias(firstWord).compare("")==0)//if he is an alias, we need to change him before redirecting him
  {
    return new RedirectionCommand(cmd_line);
  }
  else if (firstWord.compare("showpid") == 0) 
  {
    return new ShowPidCommand(cmd_line);
  }
  else if (firstWord.compare("pwd") == 0)
  {
    return new GetCurrDirCommand(cmd_line);
  }
  else if(firstWord.compare("cd")==0)
  {
    return new ChangeDirCommand(cmd_line);
  }
  else if(firstWord.compare("jobs")==0)
  {
    return new JobsCommand(cmd_line,SmallShell::getInstance().getJobs());
  }
  else if(firstWord.compare("fg")==0)
  {
    return new ForegroundCommand(cmd_line,SmallShell::getInstance().getJobs());
  }
  else if(firstWord.compare("quit")==0)
  {
    return new QuitCommand(cmd_line,SmallShell::getInstance().getJobs());
  }
  else if(firstWord.compare("kill")==0)
  {
    return new KillCommand(cmd_line,SmallShell::getInstance().getJobs());
  }
  else if(firstWord.compare("unalias")==0)
  {
    return new unaliasCommand(cmd_line); 
  }
  else if(firstWord.compare("listdir")==0)
  {
    return new ListDirCommand(cmd_line);
  }
  else if(firstWord.compare("getuser")==0)
  {
    return new GetUserCommand(cmd_line);
  }
  else if(getAlias(firstWord).compare("")!=0)
  {
    return new DemiAliasCommand(cmd_line,firstWord,isBackround);
  }
  else
  {
    return new ExternalCommand(cmd_line,cmd_s,isBackround);
  }
  return nullptr;
}


void SmallShell::executeCommand(const char *cmd_line,std::string aliasCommand,std::string externalPath) {
    // TODO: Add your implementation here
    // for example:
    if(externalPath.compare("")==0)
    {
      dup2(stdOut,1);
    }
    Command* cmd = CreateCommand(cmd_line);
    if(cmd==nullptr)//empty command
    {
      return;
    }
    cmd->setPath(externalPath);
    cmd->setAliasCommand(aliasCommand);
    cmd->execute();
    delete cmd;
    // Please note that you must fork smash process for some commands (e.g., external commands....)
}
/*

smallshell

*/
void SmallShell::setPrompt(std::string newPrompt)
{
  prompt=newPrompt;
}
std::string SmallShell::getPrompt()
{
  return prompt;
}
std::string SmallShell::getPrevPWD()
{
  return prevworkingDirectory;
}
void SmallShell::changePWD(std::string newPWD)
{
  std::string curr=workingDirectory;
  workingDirectory=newPWD;
  prevworkingDirectory=curr;
}
JobsList* SmallShell::getJobs()
{
  return job;
}
std::string SmallShell::getAlias(std::string name)
{
  if(alias.find(name)==alias.end())
  {
    return "";
  }
  return alias[name];
}
void SmallShell::setAlias(std::string name,std::string source)
{
  alias[name]=source;
  aliasInOrder.push_back(name);
}
void SmallShell::removeAlias(std::string name)
{
  alias.erase(name);
  aliasInOrder.erase(std::find(aliasInOrder.begin(), aliasInOrder.end(), name));
}
void SmallShell::getAllAlias(std::vector<std::string>& vec)
{
  for(auto element:aliasInOrder)
  {
    vec.push_back(element+"='"+alias[element]+"'");
  }
}
void SmallShell::bringProcessToFront(int pid)
{
  currentFrontPid=pid;
}
void SmallShell::leaveProcess()
{
  currentFrontPid=-1;
}
void SmallShell::smashControlCSigHandler()
{
  if(currentFrontPid==-1 && watchFrontPid==-1)//no process at front
  {
    return;
  }
  if(watchFrontPid!=-1)
  {
    //kill(watchFrontPid,SIGINT);
    std::cout<< "smash: process "<<watchFrontPid<<" was killed." << std::endl;
    currentFrontPid=-1;
    watchFrontPid=-1;
  }
  else
  {
    kill(currentFrontPid,SIGINT);
    std::cout<< "smash: process "<<currentFrontPid<<" was killed."  << std::endl;
    currentFrontPid=-1;
  }
}

/*

JobsList

*/
JobsList::~JobsList()
{
  for(auto it=list.begin(); it!=list.end();++it)
  {
    delete *it;
  }
}
void JobsList::addJob(Command *cmdCurr, int pidCurr,bool isStopped)
{
    removeFinishedJobs();
    int jobId=1;
    if(!list.empty())
    {
      jobId=1+(list.back()->jobId);
    }
    std::string commandString=cmdCurr->printCommand();
    if(cmdCurr->isAlias())
    {
      commandString=cmdCurr->getAliasCommand();
    }
    list.push_back(new JobEntry(cmdCurr,isStopped,jobId,pidCurr,commandString));
}

void JobsList::printJobsList()
{
  //TODO delete finished jobs
  removeFinishedJobs();
  for(JobsList::JobEntry* jobEntry:list)
  {
    std::cout << "[" << jobEntry->jobId << "] " << jobEntry->command << std::endl;
  }
}
void JobsList::killAllJobs()
{
  for(auto listIt=list.begin(); listIt!=list.end();++listIt)
  {
    kill((*listIt)->pid,SIGKILL);
    JobEntry* job=*listIt;
    (*listIt)=nullptr;
    delete job;
  }
}

bool isFinished(JobsList::JobEntry* job)
{
  if(job==nullptr)
  {
    return true;
  }
  int status;
  return waitpid(job->pid, &status, WNOHANG) != 0;
}

void JobsList::removeFinishedJobs()
{
  list.remove_if(isFinished);
}

JobsList::JobEntry *JobsList::getJobById(int jobId)
{
  for(auto listIt=list.begin(); listIt!=list.end();++listIt)
  {
    if((*listIt)->jobId==jobId)
    {
      return *listIt;
    }
  }
  return nullptr;
}

void JobsList::removeJobById(int jobId)
{
  for(auto listIt=list.begin(); listIt!=list.end();++listIt)
  {
    if((*listIt)->jobId==jobId)
    {
      delete *listIt;
      list.erase(listIt);
      return;
    }
  }
}

JobsList::JobEntry *JobsList::getLastJob()
{
  if(list.empty())
  {
    return nullptr;
  }
  return list.back();
}
void JobsList::printJobsForQuit()
{
  removeFinishedJobs();
  std::cout << "smash: sending SIGKILL signal to "<<list.size()<<" jobs:" << std::endl;
  for(auto listIt=list.begin(); listIt!=list.end();++listIt)
  {
    JobsList::JobEntry* job=*listIt;
    std::cout << job->pid <<": " << job->command  << std::endl;
  }
}
/*

command

*/

Command::Command(const char *cmd_line,std::string aliasCommand,std::string fileDirectedForExternal):cmd_segments(),processId(getpid()),backRound(false),cmd_line(cmd_line),fileDirectedForExternal("")
{
  createVectorForLine(cmd_line,cmd_segments);
  if(_isBackgroundComamnd(cmd_line))
  {
    backRound=true;
  }
}

Command::~Command()
{

}

std::string Command::printCommand()
{
  return cmd_line;
}
int Command::getProcessPid()
{
  return processId;
}
void Command::setProcessId(int pid)
{
  processId=pid;
}
bool Command::isAlias()
{
  return aliasChar.compare("")!=0;
}
std::string Command::getAliasCommand()
{
  return aliasChar;
}
void Command::setAliasCommand(std::string command)
{
  aliasChar=command;
}
void Command::setPath(std::string path)
{
  fileDirectedForExternal=path;
}
std::string Command::getPath()
{
  return fileDirectedForExternal;
}

/*

builtincommands

*/
BuiltInCommand::BuiltInCommand(const char *cmd_line):Command(cmd_line)
{

}

BuiltInCommand::~BuiltInCommand()
{

}
/*

changeSmashPrompt

*/
void changeSmashPrompt::execute()
{
    if(cmd_segments.size()>1)//there was a new prompt
    {
      std::string prompt=cmd_segments[1];
      _removeBackgroundSignTemp(prompt);
      if(prompt.empty())
      {
        newPrompt="smash";
        return;
      }
      newPrompt=prompt;
    }
    else
    {
      newPrompt="smash";
    }
    SmallShell::getInstance().setPrompt(newPrompt);
}

/*

ShowPidCommand

*/

int SmallShell::getShellPid()
{
  return pid;
}
void ShowPidCommand::execute()
{
  std::cout <<"smash pid is "<<SmallShell::getInstance().getShellPid()<< std::endl;
}

/*

GetCurrDirCommand

*/
void GetCurrDirCommand::execute()
{
  std::cout <<SmallShell::getInstance().getPWD()  << std::endl;
}

/*

ChangeDirCommand

*/
void ChangeDirCommand::execute()
{
  if(cmd_segments.size()>2)
  {
    if(cmd_segments[cmd_segments.size()-1].compare("&")==0)
    {
      cmd_segments.pop_back();
    }
  }
  if(cmd_segments.size()==1)
  {
    SmallShell::getInstance().changePWD(SmallShell::getInstance().getPWD());
    return;
  }
  if(cmd_segments.size()==2)
  {
    std::string path=cmd_segments[1];
    _removeBackgroundSignTemp(path);
    if(path.compare("-")==0)
    {
      if(SmallShell::getInstance().getPrevPWD().compare("")==0)
      {
        std::cerr <<"smash error: cd: OLDPWD not set" << std::endl;
      }
      else
      {
        if(chdir(SmallShell::getInstance().getPrevPWD().c_str())!=-1)
        {
          SmallShell::getInstance().changePWD(SmallShell::getInstance().getPrevPWD());
        }
        else
        {
          perror("smash error: chdir failed");
        }
      }
    }
    else
    {
      if(chdir(path.c_str())!=-1)
      {
        SmallShell::getInstance().changePWD(path);
      }
      else
      {
        perror("smash error: chdir failed");
      }
    }
  }
  else
  {
    std::cerr <<"smash error: cd: too many arguments" << std::endl;
  }
}

/*

JobsCommand

*/
JobsCommand::JobsCommand(const char *cmd_line, JobsList *jobs):BuiltInCommand(cmd_line),jobs(jobs)
{

}
void JobsCommand::execute()
{
  jobs->printJobsList();
}

/*

ForegroundCommand

*/
ForegroundCommand::ForegroundCommand(const char *cmd_line, JobsList *jobs):BuiltInCommand(cmd_line),jobs(jobs)
{

}
void ForegroundCommand::execute()
{
  int jobId=0;
  if(cmd_segments.size()==1)// need max job id
  {
    if(jobs->list.empty())
    {
      std::cerr << "smash error: fg: jobs list is empty" << std::endl;
      return;
    }
    else
    {
      jobId=jobs->getLastJob()->jobId;
    }
  }
  else
  {
    if(cmd_segments.size()==2)
    {
        try
        {
          jobId=stoi(cmd_segments[1]);
        }
        catch(const std::exception& e)
        {
          std::cerr << "smash error: fg: invalid arguments" << std::endl;
          return;
        }
        if(jobId<0)
        {
          std::cerr << "smash error: fg: invalid arguments" << std::endl;
          return;
        }
        
    }
    else
    {
      std::cerr << "smash error: fg: invalid arguments" << std::endl;
      return;
    }
  }
  //here job id was recieved
  JobsList::JobEntry* job=jobs->getJobById(jobId);
  if(job==nullptr)
  {
    std::cerr << "smash error: fg: job-id "<< jobId <<" does not exist" << std::endl;
    return;
  }
  std::string line=job->command;
  std::cout << job->command << " " <<job->pid  << std::endl;
  int pid=job->pid;
  jobs->removeJobById(jobId);
  SmallShell::getInstance().bringProcessToFront(pid);

  if(SmallShell::getInstance().getBackRoundWatch()==pid) //it is a watch command
  {
    //the idiotic way (or not, honestly not sure)
    //just run a regular watch ￣\(ó_ò)/￣
    SmallShell::getInstance().setbackGroundWatch(-1);
    _removeBackgroundSignTemp(line);
    //kill(pid,SIGSTOP); //kill the previous process
    kill(pid,SIGKILL); //kill the previous process
    SmallShell::getInstance().executeCommand(line.c_str());
  }
  else
  {
    waitpid(pid,NULL,0);
  }
}

/*

QuitCommand

*/
QuitCommand::QuitCommand(const char *cmd_line, JobsList *jobs):BuiltInCommand(cmd_line),jobs(jobs)
{

}
void QuitCommand::execute()
{
  if(cmd_segments.size()>=2)
  {
    std::string isKill=cmd_segments[1];
    _removeBackgroundSignTemp(isKill);
    if(isKill.compare("kill")==0)
    {
      jobs->removeFinishedJobs();
      jobs->printJobsForQuit();
      jobs->killAllJobs();
    }
  }
  exit(0);
}

/*

KillCommand

*/
KillCommand::KillCommand(const char *cmd_line, JobsList *jobs):BuiltInCommand(cmd_line),jobs(jobs)
{

}
void KillCommand::execute()
{
  if(cmd_segments.size()!=3)
  {
    std::cerr <<"smash error: kill: invalid arguments" << std::endl;
  }
  else
  {
    if((cmd_segments[1])[0]!='-')
    {
      std::cerr <<"smash error: kill: invalid arguments" << std::endl;
    }
    else
    {
      try
      {
        int signal=stoi(cmd_segments[1].substr(1,cmd_segments[1].size()-1));
        int jobId=stoi(cmd_segments[2]);
        JobsList::JobEntry* job=jobs->getJobById(jobId);
        if(job==nullptr)
        {
          std::cerr <<"smash error: kill: job-id "<<jobId<<" does not exist" << std::endl;
          return;
        }
        int pid=job->pid;
        std::cout << "signal number "<<signal<<" was sent to pid "<<pid << std::endl;
        if(kill(pid,signal)==0)
        {
          
        }
        else
        {
          perror("smash error: kill failed");
        }
      }
      catch(const std::exception& e)
      {
        std::cerr <<"smash error: kill: invalid arguments" << std::endl;
        return;
      }
    }
  }
}

/*

aliasCommand

*/

void aliasCommand::execute()
{
  if(cmd_segments.size()==1)
  {
    std::vector<std::string> alias;
    SmallShell::getInstance().getAllAlias(alias);
    for(std::string str: alias)
    {
      std::cout <<str  << std::endl;
    }
    return;
  }
  //TODO
  /*
    1) find two '. if not then its bad
    2) split to <alias> and <command>
    3) check if alias already exists
    4) check if alias is a command that already exists
    5)put alias in map
  */
 std::string fullString="";
 for(std::string str:cmd_segments)
 {
  fullString+=str+" ";
 }
 if(!fullString.empty())
 {
  fullString=fullString.substr(0,fullString.size()-1);
 }
 if(!fullString.empty() && this->backRound)
 {
  fullString=fullString.substr(0,fullString.size()-1);
 }
 int first=-1;
 int last=-1;
 std::regex expression("^alias [a-zA-Z0-9_]+='[^']*'$");
 if(!std::regex_match(fullString,expression))
 {
    std::cerr << "smash error: alias: invalid alias format"  << std::endl;
    return;
 }
 std::string temp=fullString.substr(cmd_segments[0].size()+1,fullString.size()-cmd_segments[0].size()-1);
 int size=temp.size();
 for(int i=0;i<size;i++)
 {
  if(temp[i]=='\'')
  {
    if(first==-1)
    {
      first=i;
    }
    else
    {
      if(last==-1)
      {
        last=i;
      }
      else//more than two, not good
      {
        std::cerr << "smash error: alias: invalid alias format"  << std::endl;
        return;
      }
    }
  }
 }
 /*
 if(last==-1 || last!=size-1 || first<2) // less than two, not good
 {
  std::cerr << "smash error: alias: invalid alias format"  << std::endl;
  return;
 }
 if(temp[first-1]!='=')
 {
  std::cerr << "smash error: alias: invalid alias format"  << std::endl;
  return;
 }
 */
 std::string alias=temp.substr(0,first-1);
 std::string command=temp.substr(first+1,last-first-1);
 if(SmallShell::getInstance().getAlias(alias).compare("")!=0)
 {
  std::cerr <<"smash error: alias: "<< alias <<" already exists or is a reserved command"  << std::endl;
  return;
 }
 if(SmallShell::getInstance().validCommand(alias))
 {
  std::cerr <<"smash error: alias: "<< alias <<" already exists or is a reserved command"  << std::endl;
  return;
 }
 SmallShell::getInstance().setAlias(alias,command);
}


/*

unaliasCommand

*/

void unaliasCommand::execute()
{
  if(cmd_segments.size()==1)
  {
    std::cerr << "smash error: unalias: not enough arguments" << std::endl;
  }
  else
  {
    int size=cmd_segments.size();
    for(int i=1;i<size;i++)
    {
      if(SmallShell::getInstance().getAlias(cmd_segments[i]).compare("")==0)
      {
        std::cerr <<"smash error: unalias: "<<cmd_segments[i]<<" alias does not exist" << std::endl;
        return;
      }
      SmallShell::getInstance().removeAlias(cmd_segments[i]);
    }
  }
}

/*

ExternalCommand

*/
ExternalCommand::ExternalCommand(const char *cmd_line,std::string cmd_s,bool isBackround):Command(cmd_line),argc()
{
  backRound=isBackround;
  std::istringstream iss(cmd_s);
  std::string token;

    while (iss >> token) {
        argc.push_back(token);
    }
}

bool withDirection(std::string line)
{
  if(line.empty())
  {
    return false;
  }
  if(line[0]=='/')
  {
    return true;
  }
  if(line.size()==1)
  {
    return false;
  }
  if(line[0]=='.' && line[1]=='/')
  {
    return true;
  }
  if(line.size()==2)
  {
    return false;
  }
  return (line[0]=='.' && line[1]=='.' && line[2]=='/');
}
void ExternalCommand::execute()
{
  pid_t pid = fork();
  if(pid<0)
  {
    perror("smash error: fork failed");
    return;
  }
  if(pid==0)//child
  {
        //important for later
        setpgrp();

        if(specialCommand(cmd_line))//run in bash
        {
          const char* bash_path = "/bin/bash";
          const char* bash_args[] = {"bash", "-c", cmd_line, nullptr};
          if(this->getPath().compare("")!=0)//file directed
          {
            //handle files
            int file = open(this->getPath() .c_str(), O_WRONLY | O_CREAT | O_APPEND, 0666);
            dup2(file, STDOUT_FILENO);
            close(file);
          }
          execv(bash_path, (char* const*)bash_args);
          perror("smash error: execv failed");
          exit(1);
        }
        char **arguments=nullptr;
        size_t size=0;
        arguments = new char *[argc.size() + 1];
        size=argc.size();
        for (size_t i = 0; i < size; i++)
        {
          arguments[i] = new char[argc[i].length() + 1];
          strcpy(arguments[i], argc[i].c_str());
        }
        arguments[argc.size()] = NULL;
        if(this->getPath().compare("")!=0)//file directed
        {
          //handle files
          int file = open(this->getPath() .c_str(), O_WRONLY | O_CREAT | O_APPEND, 0666);
          dup2(file, STDOUT_FILENO);
          close(file);
        }
        if(withDirection(argc[0]))
        {
          if (execv(argc[0].c_str(), arguments) == -1)
          {
            //failed
            perror("smash error: execv failed");
            exit(1);
          }
        }
        else
        {
          if (execvp(argc[0].c_str(), arguments) == -1)
          {
            //failed
            perror("smash error: execvp failed");
            exit(1);
          }
        }
    }
  else //parent
  {
    this->setProcessId(pid);
    if(backRound)//so add a job
    {
      JobsList* jobs=SmallShell::getInstance().getJobs();
      jobs->addJob(this,pid);
    }
    else
    {
      SmallShell::getInstance().bringProcessToFront(pid);
      pid_t pidStatus = waitpid(pid, NULL, 0);
      if (pidStatus == -1)
      {
        perror("smash error: waitpid failed");
      }
      SmallShell::getInstance().leaveProcess();
      return;
    }
  }
}
/*

ListDirCommand

*/

bool isDirectoryOrFile(const char *pathToElement, const char *element,bool directory)
{
  struct stat statStruct;
  std::string full_path = std::string(pathToElement) + "/" + element;
  if (stat(full_path.c_str(), &statStruct) == -1)
  {
    return false;
  }
  if(directory)
  {
    return S_ISDIR(statStruct.st_mode);
  }
  return S_ISREG(statStruct.st_mode);
}
void printSortedPrintingLine(std::string path,std::vector<std::string>&all,std::vector<std::string>&files,std::vector<std::string>&directories)
{
  for (const auto &elem : all)
  {
    if (isDirectoryOrFile(path.c_str(), elem.c_str(),true)) 
    {
        directories.push_back(elem);
    } 
    else
    {
      if(isDirectoryOrFile(path.c_str(), elem.c_str(),false))
      {
        files.push_back(elem);
      }
    }
  }

  //print files first
  std::sort(files.begin(),files.end());
  for (std::string file:files)
  {
    if(!file.empty())
    {
      if(file[0]!='.')
      {
        std::cout << "file: " << file  << std::endl;
      }
    }
  }
  std::sort(directories.begin(),directories.end());
  for (std::string directory:directories)
  {
    if(!directory.empty())
    {
      if(directory[0]!='.')
      {
        std::cout << "directory: " << directory  << std::endl;
      }
    }
  }
  
}

void ListDirCommand::execute()
{
    if(cmd_segments[cmd_segments.size()-1].compare("&")==0)
    {
      cmd_segments.pop_back();
    }
    if(cmd_segments.size()>2)
    {
      std::cerr << "smash error: listdir: too many arguments" << std::endl;
      return;
    }

    std::string pathToDir="";
    if(cmd_segments.size()==1)
    {
      pathToDir=SmallShell::getInstance().getPWD();
    }
    else
    {
      pathToDir=cmd_segments[1];
    }
    _removeBackgroundSignTemp(pathToDir);
    std::vector<std::string> all;
    std::vector<std::string>files;
    std::vector<std::string>directories;

    //try to open
    int fd = open(pathToDir.c_str(), O_RDONLY | O_DIRECTORY);
    if (fd == -1) 
    {
        perror("smash error: open failed");
        return;
    }

    char bufferRead[MAX_SIZE];
    ssize_t sizeToRead;

    while (true) {
        sizeToRead = syscall(SYS_getdents, fd, bufferRead, MAX_SIZE);
        if (sizeToRead == -1) 
        {
            perror("smash error: getdents failed");
            break;
        }

        if (sizeToRead == 0) {
            break;
        }

        for (int start = 0; start < sizeToRead;) {
            struct direntInfo *d = (struct direntInfo *) (bufferRead + start);
            all.push_back(std::string(d->direntName));
            start += d->direntRec;
        }
    }
    close(fd);
    printSortedPrintingLine(pathToDir,all,files,directories);
}

/*

RedirectionCommand

*/

RedirectionCommand::RedirectionCommand(const char *cmd_line):Command(cmd_line)
{
  relPath=cmd_segments[cmd_segments.size()-1];
}
void RedirectionCommand::execute()
{
  /*
  TODO
  change cout to file
  do the real action
  bring cout back
  */
    //run real deal
    bool append=true;
    std::string realCommand="";
    for(std::string str:cmd_segments)
    {
      if(str.compare(">")!=0 && str.compare(">>")!=0)
      {
        realCommand+=str+" ";
      }
      if(str.compare(">")==0)
      {
        append=false;
      }
    }

    //remove the file name
    realCommand=realCommand.substr(0,realCommand.size()-1-relPath.size());
    int fd;
    if (append) 
    {
      fd = open(relPath.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0666);
    } 
    else 
    {
      fd = open(relPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    }
    if(fd==-1)
    {
      perror("smash error: open failed");
      return;
    }
    dup2(fd, STDOUT_FILENO);
    close(fd);
    SmallShell &smash = SmallShell::getInstance();
    smash.executeCommand(realCommand.c_str(),"",relPath);
    dup2(SmallShell::getInstance().getOriginalOut(), STDOUT_FILENO);
}

/*

GetUserCommand

*/

GetUserCommand::GetUserCommand(const char* cmd_line):BuiltInCommand(cmd_line)
{

}
void GetUserCommand::execute()
{
  if(cmd_segments[cmd_segments.size()-1].compare("&")==0)
  {
    cmd_segments.pop_back();
  }
  if(cmd_segments.size()!=2)
  {
    std::cerr <<"smash error: getuser: too many arguments" << std::endl;
    return;
  }
  int processId;
  try
  {
    processId=stoi(cmd_segments[1]);
  }
  catch(const std::exception& e)
  {
    //maybe need to change that that?
    std::cerr  <<"smash error: getuser: too many arguments" << std::endl;
    return;
  }
  std::string pathToProc = "/proc/" + std::to_string(processId)+ "/status";
  int fd = open(pathToProc.c_str(), O_RDONLY);
  if (fd == -1) 
  {
      std::cerr << "smash error: getuser: process " << processId << " does not exist" << std::endl;
      return;
  }

  char info[4096];
  ssize_t bytesStreamInfo = read(fd, info, sizeof(info) - 1);
  if (bytesStreamInfo == -1) 
  {
      perror("smash error: read failed");
      close(fd);
      return;
  }
  info[bytesStreamInfo] = '\0';
  std::string fileInfo=info;

  std::istringstream stream(fileInfo);
  std::string line;
  std::string temp;
  uid_t userId;
  gid_t groupId;
  while (std::getline(stream, line))
  {
    if (line.find("Uid:") == 0) 
    {
        std::istringstream uidStream(line);
        std::string label;
        uidStream >> temp >> userId;
    } 
    else if (line.find("Gid:") == 0) 
    {
        std::istringstream gidStream(line);
        std::string label;
        gidStream >> temp >> groupId;
    }
  }
  close(fd);

  struct passwd *pointerToPasswd = getpwuid(userId);
  struct group *PointerToGroup = getgrgid(groupId);

  std::cout << "User: " << std::string(pointerToPasswd->pw_name) <<std::endl;
  std::cout << "Group: " << std::string(PointerToGroup->gr_name)<<std::endl;
}

/*

DemiAliasCommand

*/
DemiAliasCommand::DemiAliasCommand(const char *cmd_line,std::string wordToChange,bool isBackround):Command(cmd_line),wordToChange(wordToChange)
{
  backRound=isBackround;
  //change cmd_segments to the actual commend
}

void DemiAliasCommand::execute()
{
  //if no need to do anything with the cmd_line after
  std::string realLine=cmd_line;
  realLine=realLine.substr(wordToChange.size(),realLine.size()-wordToChange.size());
  realLine=SmallShell::getInstance().getAlias(wordToChange)+realLine;
  SmallShell::getInstance().executeCommand(realLine.c_str(),_trim(cmd_line));
}

/*

PipeCommand

*/
void PipeCommand::execute()
{
  //TODO
  //split to two commands
  std::string line=cmd_line;
  if(line.find_first_of("|")!=line.find_last_of("|") || line.find("|")==std::string::npos) //not a single | as expected
  {
    std::cerr << "smash error: pipe error";
  }
  int indexOfPipe=0;
  int size=line.size();
  for(int i=0;i<size;i++)
  {
    if(line[i]=='|')
    {
      indexOfPipe=i;
      break;
    }
  }
  std::string firstCommand=line.substr(0,indexOfPipe);
  std::string secondCommand=line.substr(indexOfPipe+1,size-indexOfPipe-1);
  bool apperSymbol=false;
  if(secondCommand.size()>0)
  {
    if(secondCommand[0]=='&')
    {
      apperSymbol=true;
      secondCommand=secondCommand.substr(1,secondCommand.size()-1);
    }
  }
  //create commands
  std::vector<Command*>pipeCommands;
  pipeCommands.push_back(SmallShell::getInstance().CreateCommand(firstCommand.c_str()));
  pipeCommands.push_back(SmallShell::getInstance().CreateCommand(secondCommand.c_str()));
  //set pipe

  int pipeSides[2];
  if (pipe(pipeSides) == -1)//createPipe
  {
      perror("smash error: pipe failed");
      return;
  }
  int tempIn=dup(STDIN_FILENO);
  int tempOut=dup(STDOUT_FILENO);
  int tempErr=dup(STDERR_FILENO);
  //run pipe
  /*
    need:
      1)set in/out
      2)run
      3) change in/out
  */
  //first command
  if(apperSymbol)
  {
    dup2(pipeSides[1],STDERR_FILENO);//change output
  }
  else
  {
    dup2(pipeSides[1],STDOUT_FILENO);//change output
  }
  pipeCommands[0]->execute();
  close(pipeSides[1]);
  //second command
  //return all to normal
  if(apperSymbol)
  {
    dup2(tempErr,STDERR_FILENO);//change output
  }
  else
  {
    dup2(tempOut,STDOUT_FILENO);//change output
  }
  //continue second command
  dup2(pipeSides[0],STDIN_FILENO);
  pipeCommands[1]->execute();
  close(pipeSides[0]);
  //clean
  dup2(tempIn,STDIN_FILENO);
}

/*

WatchCommand

*/
void WatchCommand::execute()
{
  //currently only works for foreground
  if(cmd_segments.size()==1) //no command
  {
    std::cerr << "smash error: watch: command not specified"  << std::endl;
    return;
  }
  int interval=2;
  int commandStartIndex=1;

  //TODO fix cheking
  if(cmd_segments.size()>=2)
  {
    if(!cmd_segments[1].empty())
    {
      if((cmd_segments[1])[0]=='-')
      {
        commandStartIndex=2;
        std::string intervalString=cmd_segments[1].substr(1,cmd_segments[1].size()-1);
        std::string valid="0123456789";
        int size=intervalString.size();
        for(int i=0;i<size;i++)
        {
          if(valid.find(intervalString[i])==std::string::npos)
          {
            std::cerr << "smash error: watch: invalid interval"  << std::endl;
            return;
          }
        }
        try
        {
          interval=stoi(intervalString);
        }
        catch(const std::exception& e)
        {
          std::cerr << "smash error: watch: invalid interval"  << std::endl;
          return;
        }
        if(interval<=0)
        {
          std::cerr << "smash error: watch: invalid interval"  << std::endl;
          return;
        }
      }
    }
  }
  int size=cmd_segments.size();
  std::string line="";
  while(commandStartIndex<size)
  {
    if(!cmd_segments[commandStartIndex].empty())
    {
      line+=cmd_segments[commandStartIndex]+" ";
    }
    commandStartIndex++;
  }
  if(line.compare("")==0)
  {
    std::cerr << "smash error: watch: command not specified"  << std::endl;
    return;
  }
  line=line.substr(0,line.size()-1);
  if(line.empty())
  {
    std::cerr << "smash error: watch: command not specified" << std::endl;
    return;
  }
  //make command
  //need to check for backround, we be different
  if(backRound)
  {
    line=_trim(line);//no need for &
    _removeBackgroundSignTemp(line);
    pid_t pid=fork();
    if(pid<0)
    {
      perror("smash error: fork failed");
    }
    if(pid==0) //child
    {
      //important for later
      setpgrp();

      dup2(open("/dev/null", 0),STDOUT_FILENO); //redirect shouln't print anything
      dup2(open("/dev/null", 0),STDERR_FILENO);
      while(true)
      {
        SmallShell::getInstance().executeCommand(line.c_str());
        std::this_thread::sleep_for(std::chrono::seconds(interval));
      }
    }
    else //parent
    {
      SmallShell::getInstance().setbackGroundWatch(pid);
      this->setProcessId(pid);
      JobsList* jobs=SmallShell::getInstance().getJobs();
      jobs->addJob(this,pid);
    }
  }
  else //foreground
  {
    SmallShell::getInstance().setWatchInForegroundPid(getpid());
    SmallShell::getInstance().bringProcessToFront(getpid());
    while(SmallShell::getInstance().getWatchInForegroundPid()!=-1)
    {
      std::system("clear");
      SmallShell::getInstance().executeCommand(line.c_str());
      std::this_thread::sleep_for(std::chrono::seconds(interval));
    }
  }
}
