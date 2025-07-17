#include "server.h"
#include "generalConnections.h"
#include <commons/config.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>

// Esta función crea un hilo (el que será el hilo de escucha) donde se va a ejecutar la función listeningToConnections, luego se hace detach en ese hilo para que corra independiente:
void createThreadForListeningConnections(t_log* logger, char* port, int* finishServer, int* listeningSocket, void* (*establishingConnectionFunction)(void*)){
    pthread_t listeningServer;

    listeningToConnectionsParams* params = malloc(sizeof(listeningToConnectionsParams));

    params->logger = logger;
    params->port = port;
    params->establishingConnectionFunction = establishingConnectionFunction;
    params->finishServer = finishServer;
    params->listeningSocket = listeningSocket;

    pthread_create(&listeningServer, NULL, listeningToConnections, (void*)params);
    pthread_detach(listeningServer);
}

// Es la función del hilo de escucha, recibe la estructura de parámetros, y se crea un socket en modo escucha,
// luego ejecuta un loop donde llamará a la función waitForClientAndReturnConnection que es la que tendrá el accept bloqueante dentro.
// Luego dentro del loop, en caso de detectar una conexión entrante, llama a otra función (createThreadForConnectingToModule)
// que crea un nuevo hilo donde se manejará qué hacer con esa conexión entrante:
void* listeningToConnections(void* voidParams){
    listeningToConnectionsParams* params = voidParams;

    (*params->listeningSocket) = createListeningSocket(params->logger, params->port);
    
    while(!(*(params->finishServer))){
        int connectionSocket = waitForClientAndReturnConnection(params->logger, params->listeningSocket);
        if (connectionSocket == -1)
            break;

        createThreadForConnectingToModule(connectionSocket, params->establishingConnectionFunction);
    }
    free(params);
    return NULL;
}

// Simplemente crea el socket y lo pone en modo escucha (lo prepara para que esté en modo escucha pero todavía no escucha activamente), luego lo retorna:
int createListeningSocket(t_log* logger, char* port){
    int listeningSocket;

	struct addrinfo hints, *servinfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(NULL, port, &hints, &servinfo);

	listeningSocket = socket(servinfo->ai_family,
                        	 servinfo->ai_socktype,
                        	 servinfo->ai_protocol);

	setsockopt(listeningSocket, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int));

	bind(listeningSocket, servinfo->ai_addr, servinfo->ai_addrlen);

	listen(listeningSocket, SOMAXCONN);

	freeaddrinfo(servinfo);
	log_info(logger, "Listo para escuchar a mi cliente");

    return listeningSocket;
}

// Escucha las conexiones entrantes, es bloqueante ya que el accept es bloqueante, solo se desbloquea
// más allá del accept cuando entra una conexión nueva:
int waitForClientAndReturnConnection(t_log* logger, int* listeningSocket){
	int connectionSocket = accept((*listeningSocket), NULL, NULL);
    if (connectionSocket < 0)
        log_info(logger, "Se desconectó el servidor de escucha");
	else
        log_info(logger, "Se conecto un cliente.");

	return connectionSocket;
}