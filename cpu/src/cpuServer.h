#ifndef CPU_SERVER_H
#define CPU_SERVER_H

void* serverDispatchThreadForKernel(void* voidPointerConnectionSocket);
void* serverInterruptThreadForKernel(void* voidPointerConnectionSocket);
void* serverThreadForMemoria(void* voidPointerConnectionSocket);

extern int connectionSocketMemory;
extern int finishServer;

#endif