#include "longTermScheduling.h"
#include "kernel.h"

// Se crea un nuevo proceso, eso crea el PCB (le reserva memoria con malloc),
// le asigna un PID, inicializa los campos en valores iniciales
// Le carga el nombre de archivo de pseudocódigo y el tamaño en memoria.
// Retorna el proceso creado (o sea, un puntero a una estructura tPcb).
// La función no debe ser llamada manualmente por ahora, es la función addProcessToNew la que la llama automaticamente:
tPcb* createNewProcess(char* pseudocodeFileName, int size){
    tPcb* process = malloc(sizeof(tPcb));

    if (!process){
        log_info(kernelLog, "Fallo al crear un nuevo proceso, cerrando programa.");
        abort();
    }

    process->pid = pidCounter;
    pidCounter++;

    process->pc = 0;
    for (int i = 0; i < 7; i++){
        process->ME[i] = 0;
        process->MT[i] = 0LL;
    }
    process->lastTimeState = 0LL;
    process->lastTimeBurst = 0LL;
    process->pseudocodeFileName = string_duplicate(pseudocodeFileName);
    process->size = size;
    process->attemptingEntryToMemory = 0;
    process->estimatedCpuBurst = kernelConfig->ESTIMACION_INICIAL;
    process->remainingEstimatedCpuBurst = process->estimatedCpuBurst;
    process->elapsedCurrentCpuBurst = 0LL;

    log_info(kernelLog, "Proceso creado exitosamente.");

    return process;
}

// Destruye el proceso, o sea, libera la memoria de los punteros internos del struct, que son el pseudocodeFileName, luego libera la memoria del pcb mismo:
// Esta función no es necesario llamarla a mano, cuando se destruye cada estado,
// en su list_destroy_and_destroy_elements se le pasa esta función para que destruya cada proceso:
void destroyProcess(tPcb* process){
    if (process){
        if (process->pseudocodeFileName)
            free (process->pseudocodeFileName);
        free(process);
    }
}

// Agrega un proceso que aún no existe a New, por eso primero crea el proceso llamando a la función createNewProcess,
// luego agrega el proceso creado (o sea, el puntero a un struct tPcb) a la lista de New.
// Luego verifica, si no hay procesos encolados en la Suspended Ready, y en caso de que el algoritmo de entrada a Ready sea PMCP, o sea FIFO pero con la cola de New previamente vacia,
// le pide a Memoria que cargue el proceso:
void entryToNew(char* pseudocodeFileName, int size){
    tPcb* process = createNewProcess(pseudocodeFileName, size);

    list_add(states[NEW], process);

    process->lastTimeState = milliseconds();
    process->ME[NEW]++;

    log_info(kernelLog, "## (%d) Se crea el proceso - Estado: NEW", process->pid);

    if (list_is_empty(states[SUSPENDED_READY]))
        decideIfSendRequestToLoadProcessToMemory(process, NEW);
}

// Se llama cuando un proceso acaba de entrar a NEW o a SUSPENDED_READY, y decide si enviar la solicitud a Memoria o no dependiendo del algoritmo de ingreso a READY:
void decideIfSendRequestToLoadProcessToMemory(tPcb* process, enumStates state){
    if (string_equals_ignore_case(kernelConfig->ALGORITMO_INGRESO_A_READY, "FIFO")){
        if (list_size(states[state]) == 1){
            // Se le pide a Memoria cargar el proceso
            if (!process->attemptingEntryToMemory)
                sendRequestToLoadProcessToMemory(process);
        }
    }
    else{ // O sea, si el algoritmo es PMCP
        // Se le pide a Memoria cargar el proceso
        if (!process->attemptingEntryToMemory)
            sendRequestToLoadProcessToMemory(process);
    }
}

// Mueve el proceso que Memoria informó que se cargó correctamente a Memoria, al estado de Ready en el planificador:
void moveProcessToReady(t_list* list){
    int pid = extractIntElementFromList(list, 0);

    // Se busca donde está el proceso, y el proceso mismo:
    enumStates processLocation = findProcessLocationByPid(pid);
    tPcb* process = findProcessByPid(pid, processLocation);

    // Se setea el flag de que estaba intentando entrar a Memoria en 0 (porque ya entró a Memoria):
    process->attemptingEntryToMemory = 0;

    // Se mueve el proceso de donde estaba (New o Suspended Ready) a Ready:
    if (processLocation == NEW)
        moveProcessFromListToAnother(pid, NEW, READY);
    else
        moveProcessFromListToAnother(pid, SUSPENDED_READY, READY);

    entryToReady(process);

    // Se intenta cargar otro proceso a Memoria, desde Suspended Ready si hay ahí, y si no, desde New;
    tryToLoadMoreProcessesToMemory();
}

// Intenta cargar un proceso más a Memoria, si hay en Suspended Ready, toma el primero de ahí, si no hay en Suspended Ready pero sí en New, toma el primero de ahí:
void tryToLoadMoreProcessesToMemory(){
    tPcb* process;
    // Si hay procesos esperando en Suspended Ready:
    if (!list_is_empty(states[SUSPENDED_READY])){
        // Asumo que la lista está ordenada (debería estar ordenada) y envío el primer proceso de Suspended Ready con el flag attemptingEntryToMemory en 0 a que se cargue a Memoria:
        process = findFirstProcessWithFlag0(SUSPENDED_READY);
        if (process)
            sendRequestToLoadProcessToMemory(process);
    }
    // Si no hay procesos en Suspended Ready, intento hacer lo mismo pero con New:
    else if (!list_is_empty(states[NEW])){
        // Asumo que la lista está ordenada (debería estar ordenada) y envío el primer proceso de New con el flag attemptingEntryToMemory en 0 a que se cargue a Memoria:
        process = findFirstProcessWithFlag0(NEW);
        if (process)
            sendRequestToLoadProcessToMemory(process);
    }
}

// Si el algoritmo de ingreso a ready es PMCP, se ordena la lista segun ese criterio
// (si es FIFO no es necesario porque siempre están encolados en orden de llegada):
void sortPmcpList(t_list* list){
    if (string_equals_ignore_case(kernelConfig->ALGORITMO_INGRESO_A_READY, "PMCP")){
        int pid = extractIntElementFromList(list, 0);

        enumStates processLocation = findProcessLocationByPid(pid);
        tPcb* process = findProcessByPid(pid, processLocation);
        process->attemptingEntryToMemory = 0;

        if (list_size(states[processLocation]) > 1)
            list_sort(states[processLocation], hasSmallerSizeThan);
    }
}

// Función auxiliar que sirve de comparador entre dos procesos, para saber si el primero tiene un tamaño más chico que el segundo
bool hasSmallerSizeThan(void* voidProcessA, void* voidProcessB){
    tPcb* processA = (tPcb*)voidProcessA;
    tPcb* processB = (tPcb*)voidProcessB;
    return processA->size <= processB->size;
}

// Se elimina el proceso de la lista de Exit, luego se imprimen los logs con métricas, y finalmente se elimina el proceso mismo
// Luego se llama a una función para intentar introducir más procesos a Memoria
void removeProcess(t_list* list){

    int pid = extractIntElementFromList(list, 0);

    enumStates processLocation = findProcessLocationByPid(pid);
    if (processLocation != EXIT){
        log_info(kernelLog, "El proceso a remover no estaba en la cola de Exit, esto no debería pasar. Cerrando programa.");
        abort();
    }
    tPcb* process = findProcessByPid(pid, EXIT);
    list_remove_element(states[EXIT], process);

    process->MT[EXIT] += (milliseconds() - process->lastTimeState);

    log_info(kernelLog, "## (<%d>) - Finaliza el proceso", pid);

    log_info(kernelLog, "<%d> - Métricas de estado: NEW (%d) (%lld), READY (%d) (%lld), EXEC (%d) (%lld), BLOCKED (%d) (%lld), SUSPENDED_BLOCKED (%d) (%lld), SUSPENDED_READY (%d) (%lld), EXIT (%d) (%lld)", pid, process->ME[0], process->MT[0], process->ME[1], process->MT[1], process->ME[2], process->MT[2], process->ME[3], process->MT[3], process->ME[4], process->MT[4], process->ME[5], process->MT[5], process->ME[6], process->MT[6]);

    destroyProcess(process);

    tryToLoadMoreProcessesToMemory();
}

void moveProcessToExitDueToFailedDump(t_list* list){
    int pid = extractIntElementFromList(list, 0);

    moveBlockedProcessToExit(pid);
}

void moveBlockedProcessToExit(int pid){
    enumStates processLocation = findProcessLocationByPid(pid);
    if (processLocation != BLOCKED && processLocation != SUSPENDED_BLOCKED){
        log_info(kernelLog, "El proceso a remover no estaba en la cola de Blocked ni en la de Suspended Blocked, esto no debería pasar. Cerrando programa.");
        abort();
    }

    moveProcessFromListToAnother(pid, processLocation, EXIT);

    if (processLocation == BLOCKED)
        sendRequestToRemoveProcessToMemory(pid);
    //else
        // PARTE DEL PLANIFICADOR DE MEDIANO PLAZO:
        // SI EL PROCESO ESTABA EN SUSPENDED_BLOCKED SE LE DEBERÍA PEDIR A MEMORIA QUE BORRE LA ENTRADA DE SWAP DEL PROCESO
}