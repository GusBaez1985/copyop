#ifndef IO_SERVER_H
#define IO_SERVER_H

void* serverIoThreadForKernel (void* voidPointerConnectionSocket);

extern int finishServer;

#endif