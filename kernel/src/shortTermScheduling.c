#include "shortTermScheduling.h"
#include "kernel.h"

// Es la función que maneja la syscall EXIT,
// por el momento solo le puse que mueva el proceso a exist y le pida a la memoria que lo elimine:
void* syscallExit(void* voidCpuId){
    sem_wait(&semStates);
    sem_wait(&semCpus);
    sem_wait(&semIos);

    int cpuId = *((int*)voidCpuId);
    free(voidCpuId);
    tCpu* cpu = findCpuById(cpuId);
    int pid = cpu->pidExecuting;

    // Se mueve el proceso a EXIT y se marca el procesador como que no está ejecutando nada
    // También se le pide a Memoria que elimine el proceso:
    moveProcessFromListToAnother(pid, EXEC, EXIT);
    cpu->pidExecuting = -1;
    cpu->isExecuting = 0;

    // Enviar algún proceso de Ready al CPU que quedó libre:
    sendSomeProcessToExec(cpu);

    sendRequestToRemoveProcessToMemory(pid);

    sem_post(&semIos);
    sem_post(&semCpus);
    sem_post(&semStates);    
    return NULL;
}

// Es la función que maneja la syscall IO:
void* syscallIo(void* voidListAndCpuParams){
    sem_wait(&semStates);
    sem_wait(&semCpus);
    sem_wait(&semIos);

    tListAndCpuIdParams* listAndCpuIdParams = (tListAndCpuIdParams*)voidListAndCpuParams;
    t_list* list = listAndCpuIdParams->list;
    int cpuId = listAndCpuIdParams->cpuId;
    free(listAndCpuIdParams);

    char* ioDevice = extractStringElementFromList(list, 0);
    int usageTime = extractIntElementFromList(list, 1);
    int pc = extractIntElementFromList(list, 2);

    tCpu* cpu = findCpuById(cpuId);
    int pid = cpu->pidExecuting;

    log_info(kernelLog, "## (<%d>) - Solicitó syscall: <IO>", pid);
    log_info(kernelLog, "La syscall IO recibida fue pedida por el proceso %d, que ejecutaba en la CPU %d", pid, cpu->cpuId);
    log_info(kernelLog, "Requiere usar el dispositivo de I/O %s, por %d milisegundos", ioDevice, usageTime);

    tIo* io = findIoByName(ioDevice);
    if (!io){
        // Si la I/O no existe, se mueve el proceso a EXIT,
        // y se le pide a Memoria que borre el proceso:
        moveProcessFromListToAnother(pid, EXEC, EXIT);

        // Se setean los flags de la cpu para indicar que no está ejecutando nada ahora:
        cpu->pidExecuting = -1;
        cpu->isExecuting = 0;
        // Enviar algún proceso de Ready a la CPU que quedó libre:
        sendSomeProcessToExec(cpu);

        sendRequestToRemoveProcessToMemory(pid);
    }
    else{
        // Si la I/O sí existe, se guarda su PC y mueve el proceso a BLOCKED:
        tPcb* process = findProcessByPid(pid, EXEC);
        process->pc = pc;
        blockProcess(pid, 1, ioDevice);
        calculateEstimatedCpuBurst(process);

        // En el planificador de mediano plazo acá se debería iniciar algun timer que de una notificacion para moverlo a SUSPENDED BLOCKED
        
        // Se busca una instancia libre en la I/O que pidió el proceso:
        tIoInstance* instance = findFreeInstanceByIo(io);
        if (instance)
            // Si se encuentra una instancia libre, se la marca como ejecutando este proceso,
            // se carga el pid del proceso en la lista de pids ejecutándose de la I/O general,
            // y se le envía una solicitud a la instancia de la I/O para que ejecute la rutina para este proceso:
            useIoInstance(pid, usageTime, io, instance);
        else
            // Si no se encuentra una instancia libre de la I/O pedida, se encola el pid del proceso en la cola de pendientes de la I/O:
            queuePidInIo(pid, usageTime, io);

        // Se setean los flags de la cpu para indicar que no está ejecutando nada ahora:
        cpu->pidExecuting = -1;
        cpu->isExecuting = 0;
        // Enviar algún proceso de Ready a la CPU que quedó libre:
        sendSomeProcessToExec(cpu);
    }

    list_destroy_and_destroy_elements(list, free);

    sem_post(&semIos);
    sem_post(&semCpus);
    sem_post(&semStates);    
    return NULL;
}

// Es la función que maneja la syscall INIT_PROC
void* syscallInitProc(void* voidListAndCpuParams){
    sem_wait(&semStates);
    sem_wait(&semCpus);
    sem_wait(&semIos);

    tListAndCpuIdParams* listAndCpuIdParams = (tListAndCpuIdParams*)voidListAndCpuParams;
    t_list* list = listAndCpuIdParams->list;
    int cpuId = listAndCpuIdParams->cpuId;
    free(listAndCpuIdParams);

    char* pseudo = extractStringElementFromList(list, 0);
    int size = extractIntElementFromList(list, 1);
    int pc = extractIntElementFromList(list, 2);

    // Se busca la CPU para encontrar el PID del proceso, con el cual se busca el proceso mismo, y se le guarda el PC:
    tCpu* cpu = findCpuById(cpuId);
    int pid = cpu->pidExecuting;
    tPcb* process = findProcessByPid(pid, EXEC);
    process->pc = pc;

    // Se ingresa a New el nuevo proceso. Este proceso no va a ocupar la CPU actual en caso de que entre a Ready y luego quiera entrar a Exec, porque la CPU no se marcó como libre.
    entryToNew(pseudo, size);

    // Se vuelve a enviar el proceso que hizo la syscall INIT_PROC al mismo CPU para que reanude su ejecución
    sendContextToCpu(cpu->connectionSocketDispatch, pid, pc);

    list_destroy_and_destroy_elements(list, free);

    sem_post(&semIos);
    sem_post(&semCpus);
    sem_post(&semStates);    
    return NULL;
}

void* syscallDumpMemory(void* voidListAndCpuParams){
    sem_wait(&semStates);
    sem_wait(&semCpus);
    sem_wait(&semIos);

    tListAndCpuIdParams* listAndCpuIdParams = (tListAndCpuIdParams*)voidListAndCpuParams;
    t_list* list = listAndCpuIdParams->list;
    int cpuId = listAndCpuIdParams->cpuId;
    free(listAndCpuIdParams);

    int pc = extractIntElementFromList(list, 0);

    tCpu* cpu = findCpuById(cpuId);
    tPcb* process = findProcessByPid(cpu->pidExecuting, EXEC);

    process->pc = pc;

    blockProcess(process->pid, 0, NULL);
    calculateEstimatedCpuBurst(process);

    cpu->isExecuting = 0;
    cpu->pidExecuting = -1;

    // iniciar algun temporizador en el planificador de mediano plazo

    // Se llama a la función que intenta meter otro proceso de READY al procesador que acaba de quedar libre:
    sendSomeProcessToExec(cpu);

    // Se llama a la función que solicita a Memoria el Dump:
    sendMemoryDumpRequest(process->pid);

    list_destroy_and_destroy_elements(list, free);

    sem_post(&semIos);
    sem_post(&semCpus);
    sem_post(&semStates);    
    return NULL;
}

void dumpCompleted(t_list* list){
    int pid = extractIntElementFromList(list, 0);

    // Se administra que pasa con el proceso al desbloquearse:
    unblockProcess(pid, 0);
}

// Función que administra un cpu que confirmó su desalojo, lo marca como libre, y pasa el proceso que tenía a READY, ordenando la lista de READY,
// también llama a una función para enviar algún proceso de READY a EXEC
void* preemptionCompleted(void* voidListAndCpuParams){
    sem_wait(&semStates);
    sem_wait(&semCpus);
    sem_wait(&semIos);

    tListAndCpuIdParams* listAndCpuIdParams = (tListAndCpuIdParams*)voidListAndCpuParams;
    t_list* list = listAndCpuIdParams->list;
    int cpuId = listAndCpuIdParams->cpuId;
    free(listAndCpuIdParams);
    
    int pc = extractIntElementFromList(list, 0);

    tCpu* cpu = findCpuById(cpuId);

    tPcb* process = findProcessByPid(cpu->pidExecuting, EXEC);
    process->pc = pc;
    moveProcessFromListToAnother(process->pid, EXEC, READY);
    log_info(kernelLog, "## (<%d>) - Desalojado por algoritmo SJF/SRT", process->pid);
    sortReadyList();

    cpu->pidExecuting = -1;
    cpu->isExecuting = 0;
    cpu->preempted = 0;

    sendSomeProcessToExec(cpu);

    list_destroy_and_destroy_elements(list, free);

    sem_post(&semIos);
    sem_post(&semCpus);
    sem_post(&semStates);    
    return NULL;
}

void blockProcess(int pid, int becauseOfIo, char* device){
    moveProcessFromListToAnother(pid, EXEC, BLOCKED);
    if (becauseOfIo)
        log_info(kernelLog, "## (<%d>) - Bloqueado por IO: %s", pid, device);
    else
        log_info(kernelLog, "## (<%d>) - Bloqueado por DUMP MEMORY", pid);
}

// Función que desbloquea un proceso, sea que estuviera en BLOCKED o en SUSPENDED_BLOCKED,
// también llama a otras funciones que deciden qué hacer dependiendo si el proceso se movió a READY o a SUSPENDED_READY:
void unblockProcess(int pid, int becauseOfIo){
    enumStates processLocation = findProcessLocationByPid(pid);
    tPcb* process = findProcessByPid(pid, processLocation);

    // Se administra qué hacer con el proceso:
    if (processLocation == BLOCKED){
        // Si estaba en BLOCKED se lo mueve a READY y se ejecuta la función que administra que pasa cuando un proceso entra a READY:
        moveProcessFromListToAnother(process->pid, BLOCKED, READY);
        if (becauseOfIo)
            log_info(kernelLog, "## (<%d>) finalizó IO y pasa a READY", pid);
        else
            log_info(kernelLog, "## (<%d>) finalizó DUMP MEMORY y pasa a READY", pid);
        entryToReady(process);
    }
    else{
        // Si estaba en SUSPENDED_BLOCKED se lo mueve a SUSPENDED_READY y se ejecuta la función que decide si pedirle a Memoria que cargue el proceso o no
        moveProcessFromListToAnother(process->pid, SUSPENDED_BLOCKED, SUSPENDED_READY);
        if (becauseOfIo)
            log_info(kernelLog, "## (<%d>) finalizó IO y pasa a SUSPENDED_READY", pid);
        else
            log_info(kernelLog, "## (<%d>) finalizó DUMP MEMORY y pasa a SUSPENDED_READY", pid);
        decideIfSendRequestToLoadProcessToMemory(process, SUSPENDED_READY);
    }
}

// Función que se llama cada vez que entra un proceso a READY (salvo cuando un proceso desalojado pasa a READY),
// decide qué hacer dependiendo del algoritmo de corto plazo, puede simplemente ordenar la lista de READY o enviar una interrupción a una CPU:
void entryToReady(tPcb* process){
    tCpu* freeCpu = findFreeCpu();
    if (freeCpu)
        sendSomeProcessToExec(freeCpu);
    else{
        if (string_equals_ignore_case(kernelConfig->ALGORITMO_CORTO_PLAZO, "SJF"))
            sortReadyList();
        else if (string_equals_ignore_case(kernelConfig->ALGORITMO_CORTO_PLAZO, "SRT")){
            updateCpuBurstTimesOfAll();
            tCpu* cpuWithLargestTime = findCpuWithLargestRemainingEstimatedCpuBurst();
            if (cpuWithLargestTime){
                tPcb* processWithLargestTime = findProcessByPid(cpuWithLargestTime->pidExecuting, EXEC);
                if (process->remainingEstimatedCpuBurst < processWithLargestTime->remainingEstimatedCpuBurst){
                    sendInterruptToCpu(cpuWithLargestTime);
                    updateCpuBurstTimes((void*)processWithLargestTime);
                }
            }
            sortReadyList();
        }
    }
}

// Función que toma el primer proceso de la lista de READY (debería estar ya ordenada de antemano), y lo mueve al cpu libre pasado como parámetro:
void sendSomeProcessToExec(tCpu* cpu){
    if (!list_is_empty(states[READY])){
        tPcb* process = (tPcb*)list_get(states[READY], 0);
        moveProcessFromListToAnother(process->pid, READY, EXEC);
        sendContextToCpu(cpu->connectionSocketDispatch, process->pid, process->pc);
        cpu->pidExecuting = process->pid;
        cpu->isExecuting = 1;

        process->lastTimeBurst = milliseconds();
    }
}