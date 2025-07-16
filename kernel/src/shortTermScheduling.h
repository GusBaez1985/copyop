#ifndef SHORT_TERM_SCHEDULING_H
#define SHORT_TERM_SCHEDULING_H

#include <commons/collections/list.h>
#include "generalScheduling.h"

void* syscallExit(void* voidCpuId);
void* syscallIo(void* voidListAndCpuParams);
void* syscallInitProc(void* voidListAndCpuParams);
void* syscallDumpMemory(void* voidListAndCpuParams);

void dumpCompleted(t_list* list);
void* preemptionCompleted(void* voidListAndCpuParams);

void blockProcess(int pid, int becauseOfIo, char* device);
void unblockProcess(int pid, int becauseOfIo);
void entryToReady(tPcb* process);
void sendSomeProcessToExec(tCpu* cpu);

#endif