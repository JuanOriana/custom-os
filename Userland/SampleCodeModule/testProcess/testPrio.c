// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <stdint.h>
#include <stringLib.h>
#include <utils.h>
#include <testUtil.h>

#define LOW_PRIO 1
#define MED_PRIO 10
#define HIGH_PRIO 20

#define TOTAL_PROCESSES 3 // TODO: Long enough to see theese processes beeing run at least twice

void testPrio()
{
    uint64_t pids[TOTAL_PROCESSES];
    uint64_t i;

    busyWait(3 * MAJOR_WAIT);

    for (i = 0; i < TOTAL_PROCESSES; i++)
    {
        char *argv[] = {"endlessLoop"};
        pids[i] = createProcess(&endlessLoop, 1, argv, BG, NULL);
    }

    busyWait(5 * MAJOR_WAIT);

    print("\nCHANGING PRIORITIES...\n");

    for (i = 0; i < TOTAL_PROCESSES; i++)
    {
        switch (i % 3)
        {
        case 0:
            nice(pids[i], LOW_PRIO);
        case 1:
            nice(pids[i], MED_PRIO);
            break;
        case 2:
            nice(pids[i], HIGH_PRIO);
            break;
        }
    }

    busyWait(3 * MAJOR_WAIT);

    print("\nBLOCKING...\n");

    for (i = 0; i < TOTAL_PROCESSES; i++)
        blockProcess(pids[i]);


    print("CHANGING PRIORITIES WHILE BLOCKED...\n");
    for (i = 0; i < TOTAL_PROCESSES; i++)
        nice(pids[i], MED_PRIO);

    print("UNBLOCKING...\n");

    for (i = 0; i < TOTAL_PROCESSES; i++)
        unblockProcess(pids[i]);

    busyWait(3 * MAJOR_WAIT);

    print("\nKILLING...\n");

    for (i = 0; i < TOTAL_PROCESSES; i++)
        killProcess(pids[i]);
    return;
}