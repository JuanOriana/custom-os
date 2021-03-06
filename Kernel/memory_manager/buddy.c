// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#ifdef BUDDY
#include "memoryManager.h"
#include "stringLib.h"
#include "buddyList.h"
#include <unistd.h>
#include <utils.h>

// Reference: https://github.com/evanw/buddy-malloc/blob/master/buddy-malloc.c

#define MIN_ALLOC_LOG_2 4 //16 B min aloc
#define MIN_ALLOC ((size_t)1 << MIN_ALLOC_LOG2)

#define MAX_ALLOC_LOG2 32
#define MAX_LEVELS (MAX_ALLOC_LOG2 - MIN_ALLOC_LOG_2)

#define BIN_POW(x) (1 << (x))

static List *base;
static uint64_t maxMemSize;
static uint8_t levels;
static List levelsList[MAX_LEVELS];

static uint8_t findIdealLevel(uint32_t bytes);
static List *findBuddy(List *node);
static List *getAddress(List *node);
static int getFirstAvailableLevel(uint8_t minLevel);
static void addNodeToLevel(List *list, List *node, uint8_t level);

void memInit(char *memBase, unsigned long memSize)
{
    if (memBase == NULL)
        return;

    base = (List *)memBase;
    maxMemSize = memSize;

    levels = (int)log2(memSize) - MIN_ALLOC_LOG_2 + 1;

    if (levels > MAX_LEVELS)
        levels = MAX_LEVELS;

    for (size_t i = 0; i < levels; i++)
    {
        listInit(&levelsList[i]);
        levelsList[i].free = 0;
        levelsList[i].level = i;
    }

    addNodeToLevel(&levelsList[levels - 1], base, levels - 1);
}

void *mallocCust(unsigned long nbytes)
{
    int realBytesNeeded = nbytes + sizeof(List);

    if (nbytes == 0 || realBytesNeeded > maxMemSize + 1)
    {
        return NULL;
    }

    uint8_t minLevel = findIdealLevel(realBytesNeeded);
    uint8_t actualLevel = getFirstAvailableLevel(minLevel);

    if (actualLevel == -1)
    {
        return NULL;
    }

    List *retNode;

    //Keep fragmenting until I find my ideal size
    for (retNode = listPop(&levelsList[actualLevel]); minLevel < actualLevel; actualLevel--)
    {
        retNode->level--;
        addNodeToLevel(&levelsList[actualLevel - 1], findBuddy(retNode), actualLevel - 1);
    }

    retNode->free = 0;

    return (void *)(retNode + 1);
}

void freeCust(void *freePointer)
{
    if (freePointer == NULL)
        return;

    List *freeNode = (List *)freePointer - 1;

    freeNode->free = 1;

    List *buddy = findBuddy(freeNode);

    //If i can join buddys, do so
    while (freeNode->level != levels - 1 && buddy->level == freeNode->level && buddy->free)
    {
        listRemove(buddy);
        freeNode = getAddress(freeNode);
        freeNode->level++;
        buddy = findBuddy(freeNode);
    }

    listPush(&levelsList[freeNode->level], freeNode);
}

static void printBlock(List *block, int idx)
{
    print("        Block number: %d\n", idx);
    print("            state: free\n");
}

void dumpMM()
{
    List *list, *aux;
    uint32_t index = 0;
    uint32_t availableSpace = 0;

    print("\nMEMORY DUMP (Buddy)\n");
    print("------------------------------------------------\n");
    print("Levels with free blocks:\n");
    print("-------------------------------\n");
    for (int i = levels - 1; i >= 0; i--)
    {
        list = &levelsList[i];
        if (!isEmpty(list))
        {
            print("    Level: %d\n", i + MIN_ALLOC_LOG_2);
            print("    Free blocks of size: 2^%d\n", i + MIN_ALLOC_LOG_2);

            for (aux = list->next, index = 1; aux != list; index++, aux = aux->next)
            {
                if (aux->free)
                {
                    printBlock(aux, index);
                    availableSpace += index * BIN_POW(i + MIN_ALLOC_LOG_2);
                }
            }

            print("-------------------------------\n");
        }
    }

    print("Available Space: %d\n", availableSpace);
}

static uint8_t findIdealLevel(uint32_t bytes)
{
    uint8_t bytesToLog = log2(bytes);

    //If smaller than min, then put it in the min
    if (bytesToLog < MIN_ALLOC_LOG_2)
        return 0;

    bytesToLog -= MIN_ALLOC_LOG_2;

    if (bytes && !(bytes & (bytes - 1))) //Perfect fit aux, else aux + 1
        return bytesToLog;

    return bytesToLog + 1;
}

static int getFirstAvailableLevel(uint8_t minLevel)
{
    uint8_t actualLevel;

    for (actualLevel = minLevel; actualLevel < levels && isEmpty(&levelsList[actualLevel]); actualLevel++)
        ;

    if (actualLevel > levels)
        return -1;

    return actualLevel;
}

static List *findBuddy(List *node)
{
    uint8_t level = node->level;
    uintptr_t currentOffset = (uintptr_t)node - (uintptr_t)base;
    uintptr_t newOffset = currentOffset ^ BIN_POW(MIN_ALLOC_LOG_2 + level);

    return (List *)(newOffset + (uintptr_t)base);
}

//www.archives.uce.com/105da903ea7e4ea183d6d3022e77e3a7?v=0c4be5ffdca74b0688ff3495045ab63e
static List *getAddress(List *node)
{
    uint8_t level = node->level;
    uintptr_t mask = BIN_POW(MIN_ALLOC_LOG_2 + level);
    mask = ~mask;

    uintptr_t currentOffset = (uintptr_t)node - (uintptr_t)base;
    uintptr_t newOffset = currentOffset & mask;

    return (List *)(newOffset + (uintptr_t)base);
}

static void addNodeToLevel(List *list, List *node, uint8_t level)
{
    node->free = 1;
    node->level = level;
    listPush(list, node);
}

#endif