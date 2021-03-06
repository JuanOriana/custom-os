// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <colours.h>
#include <idtLoader.h>
#include <lib.h>
#include <moduleLoader.h>
#include <memoryManager.h>
#include <schedule.h>
#include <stdint.h>
#include <stringLib.h>
#include <timerTick.h>
#include <videoDriver.h>
#include <interrupts.h>
#include <pipes.h>
#include <keyboardDriver.h>

extern uint8_t text;
extern uint8_t rodata;
extern uint8_t data;
extern uint8_t bss;
extern uint8_t endOfKernelBinary;
extern uint8_t endOfKernel;

static const uint64_t PageSize = 0x1000;

static void *const sampleCodeModuleAddress = (void *)0x400000;
static void *const sampleDataModuleAddress = (void *)0x500000;
static void *const sampleCodeModuleHeapAddress = (void *)0x600000;

#define HEAP_MEMORY_SIZE (64 * 1024 * 1024) //64 MB

typedef int (*EntryPoint)();

void clearBSS(void *bssAddress, uint64_t bssSize)
{
      memset(bssAddress, 0, bssSize);
}

void *getStackBase()
{
      return (void *)((uint64_t)&endOfKernel + PageSize * 8 //The size of the stack itself, 32KiB
                      - sizeof(uint64_t)                    //Begin at the top of the stack
      );
}

void *initializeKernelBinary()
{
      void *moduleAddresses[] = {
          sampleCodeModuleAddress,
          sampleDataModuleAddress};
      loadModules(&endOfKernelBinary, moduleAddresses);
      clearBSS(&bss, &endOfKernel - &bss);
      return getStackBase();
}

int main()
{
      load_idt();
      initVideoDriver(BLACK, WHITE);
      memInit((char *)sampleCodeModuleHeapAddress, HEAP_MEMORY_SIZE);
      initScheduler();
      initKeyboard();

      char *argv[] = {"Overseer"};
      addProcess(sampleCodeModuleAddress, 1, argv, 1, 0);
      _hlt();

      printStringLn("RIP");
      return 0;
}