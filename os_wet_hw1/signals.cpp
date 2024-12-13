#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void controlCSigHandler(int sig_num)
{
  std::cout << "smash: got ctrl-C" << "\n";
  SmallShell::getInstance().smashControlCSigHandler();
}