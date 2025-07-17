#include "kernelServer.h"
#include "kernel.h"

int finishServer;
int listeningSocketDispatch = -1;
int listeningSocketInterrupt = -1;
int listeningSocketIo = -1;

// Esta función es un hilo efímero, que es creado por el hilo de escucha, es la función específica que usa el Kernel para
// discriminar qué módulo se le conectó, analizando el handshake recibido (es bloqueante porque se queda resperando a recibir el paquete del handshake),
// y contestando al mismo con otro paquete.
// En base al módulo que se le conectó, crea un hilo para mantener la conexión, lo hace llamando
// a la función createThreadForConnectingToModule.
// Una vez creados ese hilo, o en caso de que falle, este hilo desaparece:
void* establishingKernelConnection(void* voidPointerConnectionSocket){
    int connectionSocket = *((int*)voidPointerConnectionSocket);
    free(voidPointerConnectionSocket);

    tPackage* package = receivePackage(connectionSocket);

    if (!package || finishServer){
        if (package){
            destroyPackage(package);
            package = NULL;
        }
        close(connectionSocket);
        return NULL;
    }

    t_list* list = packageToList(package);

    tPackage* responsePackage = createPackage(KERNEL_OK);

    switch(package->operationCode){
        case CPU_DISPATCH_HANDSHAKE:
            log_info(kernelLog, "Recibido handshake desde CPU Dispatch.");

            //int cpuIdDispatch = extractIntFromPackage(package);
            int cpuIdDispatch = extractIntElementFromList(list, 0);
            log_info(kernelLog, "Recibí CPU con ID: %d", cpuIdDispatch);
            
            log_info(kernelLog, "A punto de enviar respuesta del handshake a CPU Dispatch.");
            sendPackage(responsePackage, connectionSocket);
            log_info(kernelLog, "Enviada respuesta del handshake a CPU Dispatch.");

            log_info(kernelLog, "A punto de cargar el socket de dispatch %d, para el cpu de id %d", connectionSocket, cpuIdDispatch);
            loadCpu(cpuIdDispatch, connectionSocket, -1);

            createThreadForConnectingToModule(connectionSocket, &serverThreadForCpuDispatch);
            // ESTO ES A MODO DE MOCK, PARA SIMULAR EL MOMENTO QUE SE ENVÍA EL CONTEXTO DE EJECUCIÓN:
            // sendContextToCpu(connectionSocket, 0, 0);
            break; 
        case CPU_INTERRUPT_HANDSHAKE: 
            log_info(kernelLog, "Recibido handshake desde CPU Interrupt.");

            //int cpuIdInterrupt = extractIntFromPackage(package);
            int cpuIdInterrupt = extractIntElementFromList(list, 0);
            log_info(kernelLog, "Recibí CPU con ID: %d", cpuIdInterrupt);

            log_info(kernelLog, "A punto de enviar respuesta del handshake a CPU Interrupt.");
            sendPackage(responsePackage, connectionSocket);
            log_info(kernelLog, "Enviada respuesta del handshake a CPU Interrupt.");

            log_info(kernelLog, "A punto de cargar el socket de interrupt %d, para el cpu de id %d", connectionSocket, cpuIdInterrupt);
            loadCpu(cpuIdInterrupt, -1, connectionSocket);

            //createThreadForConnectingToModule(connectionSocket, &serverThreadForCpuInterrupt);
            break; 
        case IO_HANDSHAKE:
            log_info(kernelLog, "Recibido handshake desde IO.");
            // Extrae ioDevice del paquete (si en el futuro recibe más cosas, corregir esto)
            //char* ioDevice = extractMessageFromPackage(package);
            char* ioDevice = extractStringElementFromList(list, 0);
            // Loguea lo recibido
            log_info(kernelLog, "Recibí el siguiente dispositivo: %s", ioDevice);
            log_info(kernelLog, "A punto de enviar respuesta del handshake a IO.");
            sendPackage(responsePackage, connectionSocket);
            log_info(kernelLog, "Enviada respuesta del handshake a IO.");

            loadIo(ioDevice, connectionSocket);

            // Llamada de prueba hardcodeada: simular solicitud de uso pid = 77, usageTime = 5 segundos
            // a futuro llamar desde planificador
            //sendIoUsageRequest(connectionSocket, 77, 10);

            createThreadForConnectingToModule(connectionSocket, &serverThreadForIo);
            break;
        case DO_NOTHING:
        case ERROR:
        default:
            destroyPackage(responsePackage);
            break;
    }
    if (package)
        destroyPackage(package);
    if (list)
        list_destroy_and_destroy_elements(list, free);
    return NULL;
}

// Es la función del hilo del Kernel de recepción de información desde CPU Dispatch.
// Dependiendo del código de operación del paquete, decide qué hacer.
// Es bloqueante en receivePackage:
void* serverThreadForCpuDispatch(void* voidPointerConnectionSocket){
    int connectionSocket = *((int*)voidPointerConnectionSocket);
    free(voidPointerConnectionSocket);

    sem_wait(&semCpus);
    tCpu* cpu = findCpuByDispatchSocket(connectionSocket);
    int cpuId = cpu->cpuId;
    sem_post(&semCpus);

    tPackage* package = NULL;
    t_list* list = NULL;
    tListAndCpuIdParams* listAndCpuParams;

    log_info(kernelLog, "Hilo de servidor para escuchar mensajes del CPU Dispatch creado exitosamente.");
    while(!finishServer){
        package = receivePackage(connectionSocket);

        if (!package || finishServer){
            if (package){
                destroyPackage(package);
                package = NULL;
            }
            close(connectionSocket);
            break;
        }

        list = packageToList(package);
        // !PRUEBA DE ESCRITORIO
        log_info(kernelLog, "CPU (ID %d) me notifica syscall: %d", cpuId, package->operationCode);
        
        switch(package->operationCode){
            case CPU_DISPATCH_TO_KERNEL_EXIT:
                pthread_t syscallExitThread;
                int* cpuIdPointer = malloc(sizeof(int));
                *cpuIdPointer = cpuId;
                pthread_create(&syscallExitThread, NULL, syscallExit, (void*)cpuIdPointer);
                pthread_detach(syscallExitThread);
                break;
            case CPU_DISPATCH_TO_KERNEL_IO:
                pthread_t syscallIoThread;
                listAndCpuParams = malloc(sizeof(tListAndCpuIdParams));
                listAndCpuParams->list = list_duplicate(list);
                listAndCpuParams->cpuId = cpuId;
                pthread_create(&syscallIoThread, NULL, syscallIo, (void*)listAndCpuParams);
                pthread_detach(syscallIoThread);
                break;
            case CPU_DISPATCH_TO_KERNEL_INIT_PROC:
                pthread_t syscallInitProcThread;
                listAndCpuParams = malloc(sizeof(tListAndCpuIdParams));
                listAndCpuParams->list = list_duplicate(list);
                listAndCpuParams->cpuId = cpuId;
                pthread_create(&syscallInitProcThread, NULL, syscallInitProc, (void*)listAndCpuParams);
                pthread_detach(syscallInitProcThread);
                break;
            case CPU_DISPATCH_TO_KERNEL_DUMP_MEMORY:
                pthread_t syscallDumpMemoryThread;
                listAndCpuParams = malloc(sizeof(tListAndCpuIdParams));
                listAndCpuParams->list = list_duplicate(list);
                listAndCpuParams->cpuId = cpuId;
                pthread_create(&syscallDumpMemoryThread, NULL, syscallDumpMemory, (void*)listAndCpuParams);
                pthread_detach(syscallDumpMemoryThread);
                break;
            case CPU_DISPATCH_TO_KERNEL_PREEMPTION_COMPLETED:
                pthread_t preemptionCompletedThread;
                listAndCpuParams = malloc(sizeof(tListAndCpuIdParams));
                listAndCpuParams->list = list_duplicate(list);
                listAndCpuParams->cpuId = cpuId;
                pthread_create(&preemptionCompletedThread, NULL, preemptionCompleted, (void*)listAndCpuParams);
                pthread_detach(preemptionCompletedThread);
                break;
            case DO_NOTHING:
                log_info(kernelLog, "RECIBIDO MENSAJE VACIO DESDE CPU DISPATCH.");
                break;
            case ERROR:
            default:
                break;
        }

        destroyPackage(package);
        package = NULL;

        if (list) {
            list_destroy(list);
            list = NULL;
        }
    }
    if (package)
        destroyPackage(package);
    if (list)
        list_destroy_and_destroy_elements(list, free);
    return NULL;
}

// Comento esta función porque este hilo no debería existir, el Kernel nunca va a recibir nada a través del socket de Interrupt de una CPU
/*
// Es la función del hilo del Kernel de recepción de información desde CPU Interrupt.
// Dependiendo del código de operación del paquete, decide qué hacer.
// Es bloqueante en receivePackage:
void* serverThreadForCpuInterrupt(void* voidPointerConnectionSocket){
    int connectionSocket = *((int*)voidPointerConnectionSocket);
    free(voidPointerConnectionSocket);

    sem_wait(&semCpus);
    tCpu* cpu = findCpuByInterruptSocket(connectionSocket);
    int cpuId = cpu->cpuId;
    sem_post(&semCpus);
    
    tPackage* package = NULL;
    t_list* list = NULL;

    log_info(kernelLog, "Hilo de servidor para escuchar mensajes del CPU Interrupt creado exitosamente.");
    while(!finishServer){
        package = receivePackage(connectionSocket);

        if (!package || finishServer){
            if (package){
                destroyPackage(package);
                package = NULL;
            }
            close(connectionSocket);
            break;
        }

        list = packageToList(package);

        switch(package->operationCode){
            case CPU_INTERRUPT_TO_KERNEL_TEST:
                char* message = extractMessageFromPackage(package);
                log_info(kernelLog, "%s", message);
                free(message);
                break;
            case DO_NOTHING:
                log_info(kernelLog, "RECIBIDO MENSAJE VACIO DESDE CPU INTERRUPT.");
                break;
            case ERROR:
            default:
                break;
        }

        destroyPackage(package);
        package = NULL;

        if (list) {
            list_destroy_and_destroy_elements(list, free);
            list = NULL;
        }
    }

    if (package)
        destroyPackage(package);
    if (list)
        list_destroy_and_destroy_elements(list, free);
    return NULL;
}
*/

// Es la función del hilo del Kernel de recepción de información desde IO.
// Dependiendo del código de operación del paquete, decide qué hacer.
// Es bloqueante en receivePackage:
void* serverThreadForIo(void* voidPointerConnectionSocket){
    int connectionSocket = *((int*)voidPointerConnectionSocket);
    free(voidPointerConnectionSocket);

    sem_wait(&semIos);
    tIo* io = findIoBySocket(connectionSocket);
    tIoInstance* instance = findInstanceByIoAndSocket(io, connectionSocket);
    sem_post(&semIos);

    tPackage* package = NULL;
    t_list* list = NULL;
    tIoAndInstanceParams* ioAndInstanceParams;

    log_info(kernelLog, "Hilo de servidor para escuchar mensajes del IO creado exitosamente.");
    while(!finishServer){
        package = receivePackage(connectionSocket);

        if (!package || finishServer){
            if (package){
                destroyPackage(package);
                package = NULL;
            }
            else{
                sem_wait(&semStates);
                sem_wait(&semCpus);
                sem_wait(&semIos);

                instanceDisconnected(io, instance);

                sem_post(&semIos);
                sem_post(&semCpus);
                sem_post(&semStates);
            }
            close(connectionSocket);
            break;
        }

        list = packageToList(package);

        switch(package->operationCode){
            case IO_TO_KERNEL_COMPLETED:
                pthread_t ioCompletedThread;
                ioAndInstanceParams = malloc(sizeof(tIoAndInstanceParams));
                ioAndInstanceParams->io = io;
                ioAndInstanceParams->instance = instance;
                pthread_create(&ioCompletedThread, NULL, ioCompleted, (void*)ioAndInstanceParams);
                pthread_detach(ioCompletedThread);
                break;
            case DO_NOTHING:
                log_info(kernelLog, "RECIBIDO MENSAJE VACIO DESDE IO.");
                break;
            case ERROR:
            default:
                break;
        }

        destroyPackage(package);
        package = NULL;

        if (list) {
            list_destroy_and_destroy_elements(list, free);
            list = NULL;
        }
    }

    if (package)
        destroyPackage(package);
    if (list)
        list_destroy_and_destroy_elements(list, free);
    return NULL;
}

// Es la función de Kernel de recepción de información desde Memoria.
// Dependiendo del código de operación del paquete, decide qué hacer.
// Es bloqueante en receivePackage
// No es de un hilo a diferencia del resto ya que la memoria maneja conexiones efímeras con Kernel:
void serverForMemoria(int connectionSocketMemory){
    sem_post(&semIos);
    sem_post(&semCpus);
    sem_post(&semStates);

    tPackage* package = NULL;
    t_list* list = NULL;

    package = receivePackage(connectionSocketMemory);
    close(connectionSocketMemory);

    if (!package){
        sem_wait(&semStates);
        sem_wait(&semCpus);
        sem_wait(&semIos);
        return;
    }

    list = packageToList(package);

    switch(package->operationCode){
        case MEMORY_TO_KERNEL_PROCESS_LOAD_OK:
            sem_wait(&semStates);
            sem_wait(&semCpus);
            sem_wait(&semIos);

            moveProcessToReady(list);

            sem_post(&semIos);
            sem_post(&semCpus);
            sem_post(&semStates);
            break;
        case MEMORY_TO_KERNEL_PROCESS_LOAD_FAIL:
            sem_wait(&semStates);
            sem_wait(&semCpus);
            sem_wait(&semIos);

            sortPmcpList(list);

            sem_post(&semIos);
            sem_post(&semCpus);
            sem_post(&semStates);
            break;
        case MEMORY_TO_KERNEL_PROCESS_REMOVED:
            sem_wait(&semStates);
            sem_wait(&semCpus);
            sem_wait(&semIos);

            removeProcess(list);

            sem_post(&semIos);
            sem_post(&semCpus);
            sem_post(&semStates);
            break;
        case MEMORY_TO_KERNEL_DUMP_COMPLETED:
            sem_wait(&semStates);
            sem_wait(&semCpus);
            sem_wait(&semIos);

            dumpCompleted(list);

            sem_post(&semIos);
            sem_post(&semCpus);
            sem_post(&semStates);
            break;
        case MEMORY_TO_KERNEL_DUMP_FAIL:
            sem_wait(&semStates);
            sem_wait(&semCpus);
            sem_wait(&semIos);

            moveProcessToExitDueToFailedDump(list);

            sem_post(&semIos);
            sem_post(&semCpus);
            sem_post(&semStates);
            break;
        case DO_NOTHING:
            log_info(kernelLog, "RECIBIDO MENSAJE VACIO DESDE MEMORIA.");
            break;
        case ERROR:
        default:
            break;
    }

    destroyPackage(package);
    if (list)
        list_destroy_and_destroy_elements(list, free);

    sem_wait(&semStates);
    sem_wait(&semCpus);
    sem_wait(&semIos);
}

void finishThisServer(int connectionSocket, int* finishServerFlag){
    close(connectionSocket);
    *finishServerFlag = 1;
}