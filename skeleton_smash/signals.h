
#ifndef SIGNALS_H
#define SIGNALS_H

#include <csignal>
#include <sys/types.h>
#include <sys/wait.h>
#include <iostream>


void ctrlCHandler(int sig_num);


#endif // SIGNALS_H
