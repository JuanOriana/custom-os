#ifndef SEMLIB_H
#define SEMLIB_H

#include <systemCalls.h>

#define MAX_BLOCKED_PIDS 10

uint64_t sOpen(uint64_t id, uint64_t initValue);

int sWait(uint64_t id);

int sPost(uint64_t id);

int sClose(uint64_t id);

#endif