#include "interfaces.h"
#include "kernel.h"

// Como variables globales: una lista de ios, cada io con diferente nombre que se conecta es un nuevo elemento de la lista
// el semáforo mutex asociado a esta lista:
t_list* ios;
sem_t semIos;

void initializeIos(){
    sem_wait(&semIos);
    ios = list_create();
    if (!ios){
        log_info(kernelLog, "No se pudo inicializar la lista de I/Os. Cerrando programa.");
        abort();
    }
    sem_post(&semIos);
    log_info(kernelLog, "La lista de I/Os fue creada exitosamente.");
}

void destroyIos(){
    sem_wait(&semIos);
    if (ios)
        list_destroy_and_destroy_elements(ios, (void*)destroyIo);
    sem_post(&semIos);
}

void destroyIo(tIo* io){
    if (io){
        if (io->ioDevice)
            free (io->ioDevice);
        list_destroy_and_destroy_elements(io->queuedElements, free);
        list_destroy_and_destroy_elements(io->pidsExecuting, free);
        list_destroy_and_destroy_elements(io->instances, free);
        free(io);
    }
}

void loadIo(char* ioDevice, int connectionSocket){
    sem_wait(&semIos);

    tIo* io = findIoByName(ioDevice);

    if (io)
        log_info(kernelLog, "La I/O con nombre %s ya estaba registrada en el sistema.", ioDevice);
    else{
        log_info(kernelLog, "La I/O no estaba registrada en el sistema. Registrando I/O con nombre %s.", ioDevice);
        io = malloc(sizeof(tIo));
        io->ioDevice = string_duplicate(ioDevice);
        io->queuedElements = list_create();
        io->pidsExecuting = list_create();
        io->instances = list_create();

        list_add(ios, io);
        log_info(kernelLog, "Agregada I/O con nombre %s a la lista de I/Os del sistema.", ioDevice);
    }

    log_info(kernelLog, "Registrando nueva instancia de la I/O con nombre %s.", ioDevice);
    tIoInstance* instance = malloc(sizeof(tIoInstance));
    instance->connectionSocket = connectionSocket;
    instance->isExecuting = 0;
    instance->pidExecuting = -1;
    list_add(io->instances, instance);
    log_info(kernelLog, "Agregada instancia de la I/O con nombre %s a la lista de instancias de esa I/O.", ioDevice);

    sem_post(&semIos);
}

void* ioCompleted(void* voidIoAndInstanceParams){
    sem_wait(&semStates);
    sem_wait(&semCpus);
    sem_wait(&semIos);

    tIoAndInstanceParams* ioAndInstanceParams = (tIoAndInstanceParams*)voidIoAndInstanceParams;
    tIo* io = ioAndInstanceParams->io;
    tIoInstance* instance = ioAndInstanceParams->instance;
    free(ioAndInstanceParams);

    enumStates processLocation = findProcessLocationByPid(instance->pidExecuting);
    tPcb* process = findProcessByPid(instance->pidExecuting, processLocation);

    // Marcar la instancia como libre y quitar el pid del proceso de la cola de pids ejecutandose en la io:
    setInstanceAsFree(io, instance, process->pid);

    // Si hay elementos encolados para la IO, mandarlos a la instancia que se acaba de liberar:
    sendNewProcessToInstance(io, instance);

    // Se administra qué hacer con el proceso que estaba en la IO antes:
    unblockProcess(process->pid, 1);

    sem_post(&semIos);
    sem_post(&semCpus);
    sem_post(&semStates);    
    return NULL;
}

void setInstanceAsFree(tIo* io, tIoInstance* instance, int pid){
    instance->isExecuting = 0;
    instance->pidExecuting = -1;
    bool hasPid(void* ptr){
        int* pidInQueue = (int*) ptr;
        return *pidInQueue == pid;
    }
    list_remove_and_destroy_by_condition(io->pidsExecuting, hasPid, free);
}

void sendNewProcessToInstance(tIo* io, tIoInstance* instance){
    if (!list_is_empty(io->queuedElements)){
        tQueuedElement* queuedElement = list_remove(io->queuedElements, 0);
        int* pidPointer = malloc(sizeof(int));
        *pidPointer = queuedElement->pid;
        list_add(io->pidsExecuting, pidPointer);
        instance->isExecuting = 1;
        instance->pidExecuting = *pidPointer;
        sendIoUsageRequest(instance->connectionSocket, queuedElement->pid, queuedElement->usageTime);
        free(queuedElement);
    }
}

// Función que encola un pid con su respectivo tiempo de uso, en una I/O:
void queuePidInIo(int pid, int usageTime, tIo* io){
    tQueuedElement* queuedElement = malloc(sizeof(tQueuedElement));
    queuedElement->pid = pid;
    queuedElement->usageTime = usageTime;
    list_add(io->queuedElements, queuedElement);
}

// Función que marca una instancia de una I/O como usandose, agrega el pid a la lista de pids ejecutandose en una I/O, y envía la solicitud a la instancia:
void useIoInstance(int pid, int usageTime, tIo* io, tIoInstance* instance){
    instance->isExecuting = 1;
    instance->pidExecuting = pid;
    int* pidPointer = malloc(sizeof(int));
    *pidPointer = pid;
    list_add(io->pidsExecuting, pidPointer);
    sendIoUsageRequest(instance->connectionSocket, pid, usageTime);
}

void instanceDisconnected(tIo* io, tIoInstance* instance){
    if (instance->isExecuting){
        moveBlockedProcessToExit(instance->pidExecuting);
        setInstanceAsFree(io, instance, instance->pidExecuting);
    }
    
    list_remove_element(io->instances, instance);
    free(instance);

    if (list_is_empty(io->instances)){
        if (!list_is_empty(io->queuedElements))
            list_iterate(io->queuedElements, moveBlockedProcessToExitForInterface);
        
        list_remove_element(ios, io);
        destroyIo(io);
    }
}

void moveBlockedProcessToExitForInterface(void* voidQueuedElement){
    tQueuedElement* queuedElement = (tQueuedElement*)voidQueuedElement;
    int pid = queuedElement->pid;
    moveBlockedProcessToExit(pid);
}





tIo* findIoByName(char* ioDevice){
    bool ioNameComparer(void* ptr){
        tIo* io = (tIo*) ptr;
        return string_equals_ignore_case(io->ioDevice, ioDevice);
    }
    return list_find(ios, ioNameComparer);
}

tIoInstance* findFreeInstanceByIo(tIo* io){
    bool isFreeInstance(void* ptr){
        tIoInstance* instance = (tIoInstance*) ptr;
        return !(instance->isExecuting);
    }
    return list_find(io->instances, isFreeInstance);
}

tIoInstance* findInstanceByIoAndSocket(tIo* io, int connectionSocket){
    bool instanceSocketComparer(void* ptr){
        tIoInstance* instance = (tIoInstance*) ptr;
        return instance->connectionSocket == connectionSocket;
    }
    return list_find(io->instances, instanceSocketComparer);
}

tIo* findIoBySocket(int connectionSocket){
    bool ioInstanceFinderBySocket(void* ptr){
        tIo* io = (tIo*) ptr;
        tIoInstance* instance = findInstanceByIoAndSocket(io, connectionSocket);
        if (instance) return true;
        else return false;
    }
    return list_find(ios, ioInstanceFinderBySocket);
}