#ifndef SERVER_H
#define SERVER_H

#include <commons/log.h>

// Estructura de parámetros para pasarle a la función listeningToConnections en el momento que se crea el hilo de escucha:
typedef struct{
    t_log* logger;
    char* port;
    void* (*establishingConnectionFunction)(void*);
    int* finishServer;
    int* listeningSocket;
} listeningToConnectionsParams;

void createThreadForListeningConnections(t_log* logger, char* port, int* finishServer, int* listeningSocket, void* (*establishingConnectionFunction)(void*));

void* listeningToConnections(void* voidPointerParams);

int createListeningSocket(t_log* logger, char* port);

int waitForClientAndReturnConnection(t_log* logger, int* listeningSocket);

#endif