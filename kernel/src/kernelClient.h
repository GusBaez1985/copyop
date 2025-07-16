#ifndef KERNEL_CLIENT_H
#define KERNEL_CLIENT_H

#include "generalScheduling.h"
#include "interfaces.h"

int handshakeFromKernelToMemoria(int voidPointerConnectionSocket);

void sendContextToCpu(int connectionSocket, int pid, int pc);

void sendInterruptToCpu(tCpu* cpu);

void sendRequestToLoadProcessToMemory(tPcb* process);

void sendRequestToRemoveProcessToMemory(int pid);

void sendMemoryDumpRequest(int pid);

void sendIoUsageRequest(int connectionSocket, int pid, int usageTime);

#endif