#ifndef LONG_TERM_SCHEDULING_H
#define LONG_TERM_SCHEDULING_H

#include "generalScheduling.h"

tPcb* createNewProcess(char* pseudocodeFileName, int size);

void destroyProcess(tPcb* process);

void entryToNew(char* pseudocodeFileName, int size);

void decideIfSendRequestToLoadProcessToMemory(tPcb* process, enumStates state);

void moveProcessToReady(t_list* list);

void tryToLoadMoreProcessesToMemory();

void sortPmcpList(t_list* list);

bool hasSmallerSizeThan(void* voidProcessA, void* voidProcessB);

void removeProcess(t_list* list);

void moveProcessToExitDueToFailedDump(t_list* list);

void moveBlockedProcessToExit(int pid);

#endif
