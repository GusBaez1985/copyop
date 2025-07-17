#include "client.h"
#include "generalConnections.h"
#include <commons/config.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>

// Crea un hilo para solicitarle una conexión a otro módulo. Recibe dos funciones por parámetros, la del handshake,
// y, una vez establecida la conexión, la funciones para recibir información.
void createThreadForRequestingConnection(t_log* logger, char* ip, char* port, int (*handshake)(int), void* (*connectionServerFunction)(void*)){
    pthread_t requestingConnectionThread;

    requestingConnectionParams* params = malloc(sizeof(requestingConnectionParams));

    params->logger = logger;
    params->ip = ip;
    params->port = port;
    params->handshake = handshake;
    params->connectionServerFunction = connectionServerFunction;

    pthread_create(&requestingConnectionThread, NULL, requestingConnection, (void*)params);
    pthread_detach(requestingConnectionThread);
}

// Es la función del hilo para solicitar una conexión.
// Crea el socket de conexión, luego ejecuta el handshake, y en caso de ser exitoso,
// llama a createThreadForConnectingToModule, la cual crea un hilo para mantener la conexión con el modulo (para recibir información):
void* requestingConnection(void* voidParams){
    requestingConnectionParams* params = voidParams;

    int connectionSocket = createConnectionSocket(params->ip, params->port);

    if (connectionSocket == -1){
        log_info(params->logger, "Fallo el intento de conexión hacia un socket de escucha.");
        return NULL;
    }

    if (!params->handshake(connectionSocket)){
        log_info(params->logger, "Fallo el handshake enviado.");
        return NULL;
    }

    createThreadForConnectingToModule(connectionSocket, params->connectionServerFunction);
    return NULL;
}

// Simplemente crea un socket de conexión hacia una determinada ip y puerto, y lo retorna:
int createConnectionSocket(char* ip, char* port){
    struct addrinfo hints, *servinfo;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    getaddrinfo(ip, port, &hints, &servinfo);

    int socketConnection = socket(servinfo->ai_family,
                                  servinfo->ai_socktype,
                                  servinfo->ai_protocol);

    freeaddrinfo(servinfo);

    int err = connect(socketConnection, servinfo->ai_addr, servinfo->ai_addrlen);

    if (err == -1)
        return err;

    return socketConnection;
}

int requestingConnectionSameThread(t_log* logger, char* ip, char* port, int (*handshake)(int)){
    int connectionSocket = createConnectionSocket(ip, port);

    if (connectionSocket == -1)
        log_info(logger, "Fallo el intento de conexión hacia un socket de escucha.");

    if (!handshake(connectionSocket))
        log_info(logger, "Fallo el handshake enviado.");

    return connectionSocket;
}