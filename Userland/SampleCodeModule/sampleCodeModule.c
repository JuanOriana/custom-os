// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <systemCalls.h>
#include <shell.h>
#include <processes.h>
#include <stringLib.h>

int main()
{
      char *args[] = {"Shell"};
      createProcess(&runShell, 1, args);
      return 0;
}