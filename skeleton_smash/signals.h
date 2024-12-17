
#ifndef SIGNALS_H
#define SIGNALS_H

#include <csignal>
#include <sys/types.h>
#include <sys/wait.h>
#include <iostream>

// Signal handling function prototypes
void sigchldHandler(int sig_num);
void setupSignals();
void ctrlCHandler(int sig_num);

#endif // SIGNALS_H
