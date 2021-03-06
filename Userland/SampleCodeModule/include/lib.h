#ifndef LIB_H
#define LIB_H

#include <stdint.h>

char* cpuVendor(char* result);
uint64_t cpuModel();
void getRegistersData(uint64_t * registers);

void * mallocCust(unsigned int qty);
void freeCust(void* memory);

#endif
