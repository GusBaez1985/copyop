#ifndef INTERFACES_H
#define INTERFACES_H

#include <semaphore.h>
#include <commons/collections/list.h>

// Estructura de una instancia de una IO,
// tiene su socket de conexión, un booleano que indica si está ejecutando actualmente, y el pid del proceso usándola
typedef struct{
    int connectionSocket;
    int isExecuting;
    int pidExecuting;
} tIoInstance;

typedef struct{
    int pid;
    int usageTime;
} tQueuedElement;

// Estructua de una IO, así es como una IO está representada en Kernel,
// tiene el ioDevice que es el nombre de la IO,
// tiene una lista de pids encolados para esa IO,
// tiene una lista de pids ejecutando actualmente en esa IO,
// tiene una lista de instancias que la componen
typedef struct{
    char* ioDevice;
    t_list* queuedElements;
    t_list* pidsExecuting;
    t_list* instances;
} tIo;

void initializeIos();
void destroyIos();
void destroyIo(tIo* io);
void loadIo(char* ioDevice, int connectionSocket);
void* ioCompleted(void* voidIoAndInstanceParams);
void setInstanceAsFree(tIo* io, tIoInstance* instance, int pid);
void sendNewProcessToInstance(tIo* io, tIoInstance* instance);
void queuePidInIo(int pid, int usageTime, tIo* io);
void useIoInstance(int pid, int usageTime, tIo* io, tIoInstance* instance);

void instanceDisconnected(tIo* io, tIoInstance* instance);
void moveBlockedProcessToExitForInterface(void* voidQueuedElement);

tIo* findIoByName(char* ioDevice);
tIoInstance* findFreeInstanceByIo(tIo* io);
tIoInstance* findInstanceByIoAndSocket(tIo* io, int connectionSocket);
tIo* findIoBySocket(int connectionSocket);

extern t_list* ios;
extern sem_t semIos;

#endif