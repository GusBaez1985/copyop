#ifndef CLIENT_H
#define CLIENT_H

#include <commons/log.h>

typedef struct{
    t_log* logger;
    char* ip;
    char* port;
    void* (*connectionServerFunction)(void*);
    int (*handshake)(int);
} requestingConnectionParams;

void createThreadForRequestingConnection(t_log* logger, char* ip, char* port, int (*handshake)(int), void* (*connectionServerFunction)(void*));
void* requestingConnection(void* voidParams);
int createConnectionSocket(char* ip, char* port);

int requestingConnectionSameThread(t_log* logger, char* ip, char* port, int (*handshake)(int));

#endif