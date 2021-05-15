// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <schedule.h>
#include <lib.h>
#include <memoryManager.h>
#include <stddef.h>
#include <stringLib.h>
#include <interrupts.h>

//Consider separating the process/PCB creation part from the
//Schedueling part into two different files
//But how to deal with crossed dependencies? (Process-> schedule, Schedule -> process)

#define STACK_SIZE (4 * 1024)
#define INIT_PRIO 1
#define INIT_PRIO_AUG 2
#define CYCLE_CAP 40

int argsCopy(char **buffer, char **argv, int argc)
{
      for (int i = 0; i < argc; i++)
      {
            buffer[i] = mallocCust(sizeof(char) * (strlen(argv[i]) + 1));
            strcpy(argv[i], buffer[i]);
      }
      return 1;
}

typedef enum
{
      READY,
      BLOCKED,
      KILLED
} State;

// How to deal with fds? array? hmmm
typedef struct
{
      uint64_t pid;
      uint64_t ppid;
      int fg;
      int fd[2];
      char name[30];
      void *rsp;
      void *rbp;
      int priority;
} PCB;

typedef struct
{
      uint64_t gs;
      uint64_t fs;
      uint64_t r15;
      uint64_t r14;
      uint64_t r13;
      uint64_t r12;
      uint64_t r11;
      uint64_t r10;
      uint64_t r9;
      uint64_t r8;
      uint64_t rsi;
      uint64_t rdi;
      uint64_t rbp;
      uint64_t rdx;
      uint64_t rcx;
      uint64_t rbx;
      uint64_t rax;

      uint64_t rip;
      uint64_t cs;
      uint64_t eflags;
      uint64_t rsp;
      uint64_t ss;
      uint64_t base;
} StackFrame;

// http://datastructs.io/home/c/queue
typedef struct ProcessNode
{
      PCB pcb;
      State state;
      struct ProcessNode *next;
} ProcessNode;

typedef struct pList
{
      uint32_t size;
      uint32_t readyProcessCount;
      ProcessNode *first;
      ProcessNode *last;
} ProcessList;

static void setNewSF(void (*entryPoint)(int, char **), int argc, char **argv, void *rbp);
static int createPCB(PCB *process, char *name, int fg, int *fd);
static uint64_t getNewPid();
static void wrapper(void (*entryPoint)(int, char **), int argc, char **argv);
static void exit();
static void freeProcess(ProcessNode *process);
static ProcessNode *getProcessOfPID(uint64_t pid);

// http://datastructs.io/home/c/queue
static void processQueue(ProcessNode *newProcess);
static ProcessNode *processDequeue();
static int queueIsEmpty();

static uint64_t newPidVal = 0;
static ProcessList *processes;
static ProcessNode *currentProcess;
static uint64_t cyclesLeft;
static ProcessNode *idleProcess;

static void haltFunc(int argc, char **argv)
{
      while (1)
            _hlt();
}

void initScheduler()
{
      processes = mallocCust(sizeof(ProcessList));
      //Almost impossible, but it needs to be checked nonetheless
      if (processes == NULL)
      {
            return;
      }
      processes->first = NULL;
      processes->last = processes->first;
      processes->size = 0;
      processes->readyProcessCount = 0;

      //Create an idling process and store it in case no process is available
      // (Must be popped bcz of queue)
      char *argv[] = {"Halt Process"};
      addProcess(&haltFunc, 1, argv, 0, 0);
      idleProcess = processDequeue();
}

void *scheduler(void *oldRSP)
{
      //If I have a current process, his new rsp is the one i just got
      // If killed free, else queue it to find it later.
      if (currentProcess)
      {
            // If should continue, do so
            if (currentProcess->state == READY && cyclesLeft > 0)
            {
                  cyclesLeft--;
                  return oldRSP;
            }

            //ELse, save last state
            currentProcess->pcb.rsp = oldRSP;

            if (currentProcess->pcb.pid != idleProcess->pcb.pid && currentProcess->state == KILLED)
            {
                  ProcessNode *parent = getProcessOfPID(currentProcess->pcb.ppid);
                  //Free parents awaiting
                  if (parent != NULL && currentProcess->pcb.fg && parent->state == BLOCKED)
                  {
                        unblockProcess(parent->pcb.pid);
                  }
                  freeProcess(currentProcess);
            }
            else
                  processQueue(currentProcess);
      }
      // If I still have something to process, do so (if I kill al processses int his loop it might bring trouble)
      // CONSIDER TRACKING READY PROCESSES ALSO
      if (processes->size > 0)
      {
            currentProcess = processDequeue();
            while (currentProcess->state != READY)
            {
                  // Same logic as first iff
                  if (currentProcess->state == KILLED)
                  {
                        freeProcess(currentProcess);
                  }
                  if (currentProcess->state == BLOCKED)
                  {
                        processQueue(currentProcess);
                  }
                  currentProcess = processDequeue();
            }
      }
      // Just idle if no processes are available
      else
      {
            currentProcess = idleProcess;
      }

      // Asign new quantum, as we have changed our process
      cyclesLeft = currentProcess->pcb.priority;
      return currentProcess->pcb.rsp;
}

int addProcess(void (*entryPoint)(int, char **), int argc, char **argv, int fg, int *fd)
{
      if (entryPoint == NULL)
            return -1;

      ProcessNode *newProcess = mallocCust(sizeof(ProcessNode));

      if (newProcess == NULL)
            return -1;

      //What if createPCB and setNewSft where in another file? Consider
      if (createPCB(&newProcess->pcb, argv[0], fg, fd) == -1)
      {
            freeCust(newProcess);
            return -1;
      }

      char **argvAux = mallocCust(sizeof(char *) * argc);
      if (argvAux == 0)
            return -1;
      argsCopy(argvAux, argv, argc);

      setNewSF(entryPoint, argc, argvAux, newProcess->pcb.rbp);

      newProcess->state = READY;
      processQueue(newProcess);
      if (newProcess->pcb.fg && newProcess->pcb.ppid)
            blockProcess(newProcess->pcb.ppid);

      return newProcess->pcb.pid;
}

static int createPCB(PCB *process, char *name, int fg, int *fd)
{
      strcpy(name, process->name);
      process->pid = getNewPid();
      //currentProcess running is his parent
      process->ppid = currentProcess == NULL ? 0 : currentProcess->pcb.pid;
      if (fg > 1 || fg < 0)
            return -1;
      // If i have a parent and he is not in the foreground, then i can not be in the foreground either
      process->fg = currentProcess == NULL ? fg : (currentProcess->pcb.fg ? fg : 0);
      process->rbp = mallocCust(STACK_SIZE);
      process->priority = process->fg ? INIT_PRIO_AUG : INIT_PRIO;
      process->fd[0] = fd ? fd[0] : 0;
      process->fd[1] = fd ? fd[1] : 1;

      if (process->rbp == NULL)
            return -1;

      // ALIGNMENT ??????
      process->rbp = (void *)((char *)process->rbp + STACK_SIZE - 1);
      process->rsp = (void *)((StackFrame *)process->rbp - 1);
      return 0;
}

static void setNewSF(void (*entryPoint)(int, char **), int argc, char **argv, void *rbp)
{
      // Consider saving this data in some global variables, doesnt look quite right ATM
      //ALIGNMENT? Ask profs
      StackFrame *frame = (StackFrame *)rbp - 1;
      frame->gs = 0x001;
      frame->fs = 0x002;
      frame->r15 = 0x003;
      frame->r14 = 0x004;
      frame->r13 = 0x005;
      frame->r12 = 0x006;
      frame->r11 = 0x007;
      frame->r10 = 0x008;
      frame->r9 = 0x009;
      frame->r8 = 0x00A;
      frame->rsi = (uint64_t)argc;
      frame->rdi = (uint64_t)entryPoint;
      frame->rbp = 0x00D;
      frame->rdx = (uint64_t)argv;
      frame->rcx = 0x00F;
      frame->rbx = 0x010;
      frame->rax = 0x011;
      frame->rip = (uint64_t)wrapper;
      frame->cs = 0x008;
      frame->eflags = 0x202;
      frame->rsp = (uint64_t)(&frame->base);
      frame->ss = 0x000;
      frame->base = 0x000;
}

static uint64_t getNewPid()
{
      return newPidVal++;
}

static void processQueue(ProcessNode *newProcess)
{
      if (queueIsEmpty())
      {
            processes->first = newProcess;
            processes->last = processes->first;
      }
      else
      {
            processes->last->next = newProcess;
            newProcess->next = NULL;
            processes->last = newProcess;
      }

      if (newProcess->state == READY)
      {
            processes->readyProcessCount++;
      }

      processes->size++;
}

static ProcessNode *processDequeue()
{
      if (queueIsEmpty())
            return NULL;

      ProcessNode *p = processes->first;
      processes->first = processes->first->next;
      processes->size--;

      if (p->state == READY)
      {
            processes->readyProcessCount--;
      }

      return p;
}

static int queueIsEmpty()
{
      return processes->size == 0;
}

static void freeProcess(ProcessNode *process)
{
      // Warning?
      freeCust((void *)((char *)process->pcb.rbp - STACK_SIZE + 1));
      freeCust((void *)process);
}

//Set as killed and move onto the next
static void exit()
{
      killProcess(currentProcess->pcb.pid);
      callTimerTick();
}

//Still dont quite get how this works but it does. Praise the sun!
static void wrapper(void (*entryPoint)(int, char **), int argc, char **argv)
{
      entryPoint(argc, argv);
      for (int i = 0; i < argc; i++)
            freeCust(argv[i]);
      freeCust(argv);

      exit();
}

static ProcessNode *getProcessOfPID(uint64_t pid)
{
      if (currentProcess != NULL && currentProcess->pcb.pid == pid)
      {
            return currentProcess;
      }

      for (ProcessNode *p = processes->first; p != NULL; p = p->next)
      {
            if (p->pcb.pid == pid)
                  return p;
      }

      return NULL;
}

static uint64_t setNewState(uint64_t pid, State newState)
{

      ProcessNode *process = getProcessOfPID(pid);

      if (process == NULL || process->state == KILLED)
            return -1;

      if (newState == READY && process->state != READY)
            processes->readyProcessCount++;

      else if (newState != READY && process->state == READY)
            processes->readyProcessCount--;

      process->state = newState;

      return process->pcb.pid;
}

uint64_t killProcess(uint64_t pid)
{
      if (pid <= 1)
      {
            return -1;
      }
      int aux = setNewState(pid, KILLED);

      if (pid == currentProcess->pcb.pid)
            callTimerTick();

      return aux;
}

uint64_t blockProcess(uint64_t pid)
{
      int aux = setNewState(pid, BLOCKED);

      if (pid == currentProcess->pcb.pid)
            callTimerTick();
      return aux;
}

uint64_t unblockProcess(uint64_t pid)
{
      return setNewState(pid, READY);
}

char *stateToStr(State state)
{
      switch (state)
      {
      case READY:
            return "Ready";
            break;
      case BLOCKED:
            return "Blocked";
      default:
            return "Awaiting death";
            break;
      };
}

void printProcess(ProcessNode *process)
{

      if (process != NULL)
      {
            printInt(process->pcb.pid);
            printString("        ");
            printString(process->pcb.name);
            printString("       ");
            printString(stateToStr(process->state));
            printString("      ");
            printInt((uint64_t)process->pcb.rsp);
            printString("     ");
            printInt((uint64_t)process->pcb.rbp);
            printString("       ");
            printInt((int)process->pcb.fg);
            printStringLn("");
      }
}

void processDisplay()
{
      printStringLn("PID      NAME        STATE      RSP          RBP          FG");

      if (currentProcess != NULL)
            printProcess(currentProcess);

      ProcessNode *curr = processes->first;
      while (curr)
      {
            printProcess(curr);
            curr = curr->next;
      }
}

int getCurrPID()
{
      return currentProcess ? currentProcess->pcb.pid : -1;
}

void setNewCycle(uint64_t pid, int priority)
{

      if (priority < 0)
            priority = 0;
      if (priority > CYCLE_CAP)
            priority = CYCLE_CAP;

      ProcessNode *p = getProcessOfPID(pid);

      if (p != NULL)
            p->pcb.priority = priority;
}

void killFgProcess()
{
      if (currentProcess != NULL && currentProcess->pcb.fg && currentProcess->state == READY)
      {
            killProcess(currentProcess->pcb.pid);
            return;
      }
}

void yield()
{
      cyclesLeft = 0;
      callTimerTick();
}