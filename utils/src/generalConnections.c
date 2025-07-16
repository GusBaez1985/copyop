#include "generalConnections.h"
#include <stdlib.h>
#include <pthread.h>

// Esta función se utiliza para crear un hilo que mantiene la conexión con otro módulo,
// La función que utiliza el hilo es la pasada como parámetro a esta función,
// y los argumentos de esa función para el hilo, son simplemente el socket de conexión (que hay que pasarlo como puntero):
void createThreadForConnectingToModule(int connectionSocket, void* (*connectionFunction)(void*)){
    pthread_t threadForConnection;

    int* connectionSocketPointer = malloc(sizeof(int));
    (*connectionSocketPointer) = connectionSocket;

    pthread_create(&threadForConnection, NULL, connectionFunction, (void*)connectionSocketPointer);
    pthread_detach(threadForConnection);
}