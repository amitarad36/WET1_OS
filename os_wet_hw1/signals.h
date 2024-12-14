#ifndef SMASH__SIGNALS_H_
#define SMASH__SIGNALS_H_
#include <unistd.h>
#include <sys/wait.h>

void controlCSigHandler(int sig_num);

#endif //SMASH__SIGNALS_H_
