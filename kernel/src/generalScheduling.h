#ifndef GENERAL_SCHEDULING_H
#define GENERAL_SCHEDULING_H

#include <commons/collections/list.h>
#include <stdint.h>
#include <semaphore.h>

// Enum que sirve para los índices de la lista states, para saber qué estado está en cada índice:
typedef enum{
    NEW,
    READY,
    EXEC,
    BLOCKED,
    SUSPENDED_BLOCKED,
    SUSPENDED_READY,
    EXIT
} enumStates;

// Estructura del Proceso, que es el PCB, los primeros 4 elementos son los obligatorios, los otros son agregados por utilidad
// El segundo grupo de elementos son el nombre del archivo de pseudocódigo, el tamaño del proceso en Memoria, y un flag que indica si el proceso está intentando ser cargado en Memoria (para no enviárselo a Memoria dos veces)
// El tercer grupo son variables para utilizar en los algoritmos SJF y SRT, para estimar las ráfagas de CPU y medir cuánto les queda y lleva
// El cuarto grupo son para saber información de si el proceso solicitó o no una IO, cuál, y cuánto tiempo pidió
typedef struct{
    int pid;
    int pc;
    int ME[7];
    long long MT[7];

    long long lastTimeState;
    long long lastTimeBurst;

    char* pseudocodeFileName;
    uint32_t size;
    int attemptingEntryToMemory;
    
    long long estimatedCpuBurst;
    long long remainingEstimatedCpuBurst;
    long long elapsedCurrentCpuBurst;
} tPcb;

// Estructura de un CPU, así es como está representado cada CPU en Kernel,
// con su id, sus sockets (ambos, el de Dispatch y el de Interrupt), un "booleano" que representa si está ejecutando algo o no, 
// otro booleano que indica si a la CPU se le envió una interrupcion para desalojarla y se está esperando su respuesta,
// y el pid que está ejecutando:
typedef struct{
    int cpuId;
    int connectionSocketDispatch;
    int connectionSocketInterrupt;
    int preempted;
    int isExecuting;
    int pidExecuting;
} tCpu;

void initializeStates();
void destroyStates();

void initializeCpus();
void destroyCpus();
void loadCpu(int cpuId, int connectionSocketDispatch, int connectionSocketInterrupt);

tCpu* findCpuById(int id);
tCpu* findCpuByDispatchSocket(int connectionSocketDispatch);
tCpu* findCpuByInterruptSocket(int connectionSocketInterrupt);
tCpu* findCpuByPidExecuting(int pid);
tCpu* findCpuWithLargestRemainingEstimatedCpuBurst();
tCpu* findFreeCpu();
tPcb* findProcessByPid(int pid, enumStates state);
enumStates findProcessLocationByPid(int pid);
tPcb* findFirstProcessWithFlag0(enumStates state);


void moveProcessFromListToAnother(int pid, enumStates originState, enumStates destinyState);

char* getStateString(enumStates state);

void sortReadyList();
void updateCpuBurstTimesOfAll();
void updateCpuBurstTimes(void* voidProcess);
void calculateEstimatedCpuBurst(tPcb* process);
bool hasSmallerRemainingEstimatedCpuBurst(void* voidProcessA, void* voidProcessB);
void* cpuWithMaximumRemainingEstimatedCpuBurst(void* voidCpuA, void* voidCpuB);
bool cpuIsNotPreempted(void* voidCpu);
bool cpuIsExecuting(void* voidCpu);
bool cpuIsNotExecuting(void* voidCpu);

// Funciones de prueba, luego serán borradas:
void printStatesContent();
void printProcessContent(void* process);



extern int pidCounter;
extern t_list* states[7];
extern t_list* cpus;

extern sem_t semStates;
extern sem_t semCpus;

#endif