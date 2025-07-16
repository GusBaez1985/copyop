#ifndef KERNEL_SERVER_H
#define KERNEL_SERVER_H

#include <commons/collections/list.h>
#include "interfaces.h"

typedef struct{
    t_list* list;
    int cpuId;
} tListAndCpuIdParams;

typedef struct{
    tIo* io;
    tIoInstance* instance;
} tIoAndInstanceParams;

void* establishingKernelConnection(void* voidPointerConnectionSocket);

void* serverThreadForCpuDispatch(void* voidPointerConnectionSocket);

//void* serverThreadForCpuInterrupt(void* voidPointerConnectionSocket);

void* serverThreadForIo(void* voidPointerConnectionSocket);

void serverForMemoria();

void finishThisServer(int connectionSocket, int* finishServerFlag);

extern int finishServer;
extern int listeningSocketDispatch;
extern int listeningSocketInterrupt;
extern int listeningSocketIo;

#endif