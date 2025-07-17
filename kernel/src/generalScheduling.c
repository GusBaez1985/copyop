#include "generalScheduling.h"
#include "kernel.h"

// Las variables globales son:
// el pidCounter que es un contador global de PIDs que siempre va creciendo, para saber qué PID asignar a nuevos procesos;
// un array (vector o arreglo) de tamaño 7, que es de estados, cada elemento del array es un estado, a su vez, cada estado es una lista (una lista de los procesos (PCB) que están en ese estado);
// una lista de cpus, cada cpu que se conecta es un nuevo elemento de la lista,
// una lista de ios, cada io con diferente nombre que se conecta es un nuevo elemento de la lista
int pidCounter = 0;
t_list* states[7];
t_list* cpus;

sem_t semStates;
sem_t semCpus;

// Inicializa el array de estados, en realidad lo que hace es crear las listas con list_create:
void initializeStates(){
    sem_wait(&semStates);
    for (int i = 0; i < 7; i++){
        states[i] = list_create();
        if (!states[i]){
            log_info(kernelLog, "No se pudo inicializar el estado %d. Cerrando programa.", i);
            abort();
        }
    }
    sem_post(&semStates);
    log_info(kernelLog, "Los 7 estados fueron creados exitosamente.");
}

// Destruye las listas de los estados, y también destruye los PCBs que están dentro de cada lista llamando a la función destroyProcess:
void destroyStates(){
    sem_wait(&semStates);
    for (int i = 0; i < 7; i++)
        if (states[i])
            list_destroy_and_destroy_elements(states[i], (void*)destroyProcess);
    sem_post(&semStates);
}

// Inicializa la lista de cpus, o sea, llama a list_create para la lista:
void initializeCpus(){
    sem_wait(&semCpus);
    cpus = list_create();
    if (!cpus){
        log_info(kernelLog, "No se pudo inicializar la lista de CPUS. Cerrando programa.");
        abort();
    }
    sem_post(&semCpus);
    log_info(kernelLog, "La lista de CPUS fue creada exitosamente.");
}

// Destruye la lista de cpus y libera la memoria de cada nodo de la lista (o sea, de cada cpu):
void destroyCpus(){
    sem_wait(&semCpus);
    if (cpus)
        list_destroy_and_destroy_elements(cpus, free);
    sem_post(&semCpus);
}

// Esta función es llamada desde el servidor del Kernel cada vez que se conecta un CPU al Kernel.
// Como cada CPU se conecta dos veces, una con el socket de Dispatch, y otra con el de Interrupt, esta función termina siendo llamada dos veces,
// por eso tiene un if para diferenciar ambas situaciones según si el id del cpu ya está en la lista de cpus o no:
void loadCpu(int cpuId, int connectionSocketDispatch, int connectionSocketInterrupt){
    sem_wait(&semCpus);
    tCpu* cpu = findCpuById(cpuId);

    // Si el cpu ya estaba en la lista de cpus, solo se carga el socket nuevo que se conectó (el que tenga un valor mayor a -1):
    if (cpu){
        if (connectionSocketDispatch > -1)
            cpu->connectionSocketDispatch = connectionSocketDispatch;
        if (connectionSocketInterrupt > -1)
            cpu->connectionSocketInterrupt = connectionSocketInterrupt;

        log_info(kernelLog, "El CPU de id: %d, ya estaba cargado, ahora se carga el socket restante.", cpu->cpuId);
        log_info(kernelLog, "CPU de id: %d, socket de Dispatch: %d, socket de Interrupt: %d", cpu->cpuId, cpu->connectionSocketDispatch, cpu->connectionSocketInterrupt);
    }

    // Si el cpu no estaba en la lista de cpus, se crea el nodo (que es una estructura tCpu) reservándole memoria con malloc,
    // y se le cargan los sockets (el que no se conectó viene con un valor de -1 para indicar que no está presente aún),
    // Se setea el booleano de que está ejecutando un proceso en 0, y el PID que está ejecutando en -1 (un valor inválido que indica que no hay PID),
    // Luego se agrega a la lista de cpus:
    else{
        cpu = malloc(sizeof(tCpu));
        cpu->cpuId = cpuId;
        cpu->connectionSocketDispatch = connectionSocketDispatch;
        cpu->connectionSocketInterrupt = connectionSocketInterrupt;
        cpu->preempted = 0;
        cpu->isExecuting = 0;
        cpu->pidExecuting = -1;

        list_add(cpus, cpu);

        log_info(kernelLog, "El CPU de id: %d, no estaba cargado, ahora se carga el primer socket.", cpu->cpuId);
        log_info(kernelLog, "CPU de id: %d, socket de Dispatch: %d, socket de Interrupt: %d", cpu->cpuId, cpu->connectionSocketDispatch, cpu->connectionSocketInterrupt);
    }
    sem_post(&semCpus);
}

// Es una función que busca si un determinado id está ya presente en la lista de cpus, devuelve el cpu mismo si lo encuentra, o NULL si no.
// Usa una función declarada y definida internamente (cpuIdComparer) que como no es parte del estándar de C puede hacer que el IDE (el Visual Studio Code) no reconozca algunas funciones,
// pero compila igual porque es admitido como una extensión de GNU (GNU C):
tCpu* findCpuById(int id){
    bool cpuIdComparer(void* ptr){
        tCpu* cpu = (tCpu*) ptr;
        return cpu->cpuId == id;
    }
    return list_find(cpus, cpuIdComparer);
}

// Es una función que busca si un determinado socket de dispatch está ya presente en la lista de cpus, devuelve el cpu mismo si lo encuentra, o NULL si no.
// Usa una función declarada y definida internamente (cpuSocketComparer) que como no es parte del estándar de C puede hacer que el IDE (el Visual Studio Code) no reconozca algunas funciones,
// pero compila igual porque es admitido como una extensión de GNU (GNU C):
tCpu* findCpuByDispatchSocket(int connectionSocketDispatch){
    bool cpuSocketComparer(void* ptr){
        tCpu* cpu = (tCpu*) ptr;
        return cpu->connectionSocketDispatch == connectionSocketDispatch;
    }
    return list_find(cpus, cpuSocketComparer);
}

// Es una función que busca si un determinado socket de interrupt está ya presente en la lista de cpus, devuelve el cpu mismo si lo encuentra, o NULL si no.
// Usa una función declarada y definida internamente (cpuSocketComparer) que como no es parte del estándar de C puede hacer que el IDE (el Visual Studio Code) no reconozca algunas funciones,
// pero compila igual porque es admitido como una extensión de GNU (GNU C):
tCpu* findCpuByInterruptSocket(int connectionSocketInterrupt){
    bool cpuSocketComparer(void* ptr){
        tCpu* cpu = (tCpu*) ptr;
        return cpu->connectionSocketInterrupt == connectionSocketInterrupt;
    }
    return list_find(cpus, cpuSocketComparer);
}

tCpu* findCpuByPidExecuting(int pid){
    bool cpuPidComparer(void* ptr){
        tCpu* cpu = (tCpu*) ptr;
        return cpu->pidExecuting == pid;
    }
    return list_find(cpus, cpuPidComparer);
}

tCpu* findCpuWithLargestRemainingEstimatedCpuBurst(){
    t_list* nonPreemptedCpus = list_filter(cpus, cpuIsNotPreempted);
    t_list* nonPreemptedExecutingCpus = list_filter(nonPreemptedCpus, cpuIsExecuting);
    if (list_size(nonPreemptedExecutingCpus) > 1)
        return list_get_maximum(nonPreemptedExecutingCpus, cpuWithMaximumRemainingEstimatedCpuBurst);
    else if (list_size(nonPreemptedExecutingCpus) == 1)
        return list_get(nonPreemptedExecutingCpus, 0);
    else
        return NULL;
}

tCpu* findFreeCpu(){
    return list_find(cpus, cpuIsNotExecuting);
}

// Dado un pid de un proceso, y un estado, devuelve el proceso si se encuentra en ese estado, o devuelve NULL si no.
// Usa una función declarada y definida internamente (processPidComparer) que como no es parte del estándar de C puede hacer que el IDE (el Visual Studio Code) no reconozca algunas funciones,
// pero compila igual porque es admitido como una extensión de GNU (GNU C):
tPcb* findProcessByPid(int pid, enumStates state){
    bool processPidComparer(void* ptr){
        tPcb* process = (tPcb*) ptr;
        return process->pid == pid;
    }
    return list_find(states[state], processPidComparer);
}

// Dado un pid de un proceso, devuelve el estado en que se encuentra el proceso,
// o falla cerrando el programa si no está en ningún estado (por lo que no debe usarse con pids que no existan):
enumStates findProcessLocationByPid(int pid){
    if (findProcessByPid(pid, NEW))
        return NEW;
    else if (findProcessByPid(pid, SUSPENDED_READY))
        return SUSPENDED_READY;
    else if (findProcessByPid(pid, SUSPENDED_BLOCKED))
        return SUSPENDED_BLOCKED;
    else if (findProcessByPid(pid, READY))
        return READY;
    else if (findProcessByPid(pid, EXEC))
        return EXEC;
    else if (findProcessByPid(pid, BLOCKED))
        return BLOCKED;
    else if (findProcessByPid(pid, EXIT))
        return EXIT;
    log_info(kernelLog, "El PID del proceso no fue encontrado en ningún estado. Cerrando programa");
    abort();
}

// Dado un estado, retorna el primer proceso en la lista de ese estado que tenga el flag attemptingEntryToMemory en 0, retorna NULL si no encontró ninguno.
// Usa una función declarada y definida internamente (processFlagComparer) que como no es parte del estándar de C puede hacer que el IDE (el Visual Studio Code) no reconozca algunas funciones,
// pero compila igual porque es admitido como una extensión de GNU (GNU C):
tPcb* findFirstProcessWithFlag0(enumStates state){
    bool processFlagComparer(void* ptr){
        tPcb* process = (tPcb*) ptr;
        return process->attemptingEntryToMemory == 0;
    }
    return list_find(states[state], processFlagComparer);
}

// Es una función genérica que mueve un proceso de un estado al otro,
// recibe el pid del proceso a mover, y el enum del estado de origin, y el del estado de destino.
// Luego hace el log obligatorio:
void moveProcessFromListToAnother(int pid, enumStates originState, enumStates destinyState){
    tPcb* process = findProcessByPid(pid, originState);

    if (!list_remove_element(states[originState], process)){
        log_info(kernelLog, "El proceso no pudo ser encontrado en el estado.");
        abort();
    }

    list_add(states[destinyState], process);

    process->MT[originState] += (milliseconds() - process->lastTimeState);
    process->lastTimeState = milliseconds();
    process->ME[destinyState]++;

    char* originStateString = getStateString(originState);
    char* destinyStateString = getStateString(destinyState);

    log_info(kernelLog, "## (<%d>) Pasa del estado <%s> al estado <%s>", pid, originStateString, destinyStateString);

    free(originStateString);
    free(destinyStateString);
}

// Es una función auxiliar que devuelve un string (que luego debe ser liberado con free) con el nombre del estado, dado el enum del estado:
char* getStateString(enumStates state){
    switch(state){
        case NEW:
            return string_duplicate("NEW");
        case READY:
            return string_duplicate("READY");
        case EXEC:
            return string_duplicate("EXEC");
        case BLOCKED:
            return string_duplicate("BLOCKED");
        case SUSPENDED_BLOCKED:
            return string_duplicate("SUSPENDED_BLOCKED");
        case SUSPENDED_READY:
            return string_duplicate("SUSPENDED_READY");
        case EXIT:
            return string_duplicate("EXIT");
        default:
            return string_duplicate("ERROR");
    }
}

void sortReadyList(){
    if (list_size(states[READY]) > 1)
        list_sort(states[READY], hasSmallerRemainingEstimatedCpuBurst);
}

void updateCpuBurstTimesOfAll(){
    list_iterate(states[EXEC], updateCpuBurstTimes);
}

void updateCpuBurstTimes(void* voidProcess){
    tPcb* process = (tPcb*) voidProcess;
    process->elapsedCurrentCpuBurst += milliseconds() - process->lastTimeBurst;
    process->remainingEstimatedCpuBurst = process->estimatedCpuBurst - process->elapsedCurrentCpuBurst;
    process->lastTimeBurst = milliseconds();
}

void calculateEstimatedCpuBurst(tPcb* process){
    updateCpuBurstTimes((void*) process);

    process->estimatedCpuBurst = kernelConfig->ALFA * process->elapsedCurrentCpuBurst + (1 - kernelConfig->ALFA) * process->estimatedCpuBurst;
    process->elapsedCurrentCpuBurst = 0;
    process->remainingEstimatedCpuBurst = process->estimatedCpuBurst;
}

bool hasSmallerRemainingEstimatedCpuBurst(void* voidProcessA, void* voidProcessB){
    tPcb* processA = (tPcb*)voidProcessA;
    tPcb* processB = (tPcb*)voidProcessB;
    return processA->remainingEstimatedCpuBurst <= processB->remainingEstimatedCpuBurst;
}

void* cpuWithMaximumRemainingEstimatedCpuBurst(void* voidCpuA, void* voidCpuB){
    tCpu* cpuA = (tCpu*)voidCpuA;
    tCpu* cpuB = (tCpu*)voidCpuB;
    tPcb* processA = findProcessByPid(cpuA->pidExecuting, EXEC);
    tPcb* processB = findProcessByPid(cpuB->pidExecuting, EXEC);
    return processA->remainingEstimatedCpuBurst >= processB->remainingEstimatedCpuBurst ? cpuA : cpuB;
}

bool cpuIsNotPreempted(void* voidCpu){
    tCpu* cpu = (tCpu*) voidCpu;
    return !cpu->preempted;
}

bool cpuIsExecuting(void* voidCpu){
    tCpu* cpu = (tCpu*) voidCpu;
    return cpu->isExecuting;
}

bool cpuIsNotExecuting(void* voidCpu){
    tCpu* cpu = (tCpu*) voidCpu;
    return !cpu->isExecuting;
}


// Funciones de prueba, luego serán borradas:

// Imprime (loggea) la lista de estados y el contenido de cada uno, o sea, información de cada proceso en cada estado (es de prueba):
void printStatesContent(){
    for (int i = 0; i < 7; i++){
        log_info(kernelLog, "Estado %s:", getStateString(i));
        list_iterate(states[i], printProcessContent);
    }
}

// Imprime (loggea) información del proceso, es llamada por la función de imprimir estados (es de prueba):
void printProcessContent(void* voidPointerProcess){
    tPcb* process = (tPcb*)voidPointerProcess;
    log_info(kernelLog, "   PID: %d, PC: %d, SIZE: %d, FILE_NAME: %s", process->pid, process->pc, process->size, process->pseudocodeFileName);
}