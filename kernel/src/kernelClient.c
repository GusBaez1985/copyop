#include "kernel.h"
#include "kernelClient.h"




// Función del handshake inicial entre Kernel y Memoria.
// El Kernel envía un paquete que solo tiene el código de operación KERNEL_HANDSHAKE,
// Si la memoria lo recibió bien, debe contestar con otro paquete que solo tiene el código
// de operación MEMORIA_OK:
int handshakeFromKernelToMemoria(int connectionSocket){
    tPackage* handshakePackage = createPackage(KERNEL_HANDSHAKE);
    log_info(kernelLog, "A punto de enviar mensaje de handshake desde Kernel a Memoria.");
    sendPackage(handshakePackage, connectionSocket);
    log_info(kernelLog, "Enviado mensaje de handshake desde Kernel a Memoria.");

    tPackage* receivedPackage = receivePackage(connectionSocket);
    log_info(kernelLog, "Recibida respuesta desde Memoria al handshake de Kernel.");
    tOperationCode receivedCode = receivedPackage->operationCode;

    destroyPackage(receivedPackage);

    return receivedCode == MEMORIA_OK;
}

void sendContextToCpu(int connectionSocket, int pid, int pc){
    tPackage* instructionPackage = createPackage(KERNEL_TO_CPU_DISPATCH_TEST);
    log_info(kernelLog, "A punto de enviar el paquete con el contexto de instruccion, desde kernel a cpu");

    // Agrego estas lineas a modo de test para enviar un PID y PC especificos de prueba
    addToPackage(instructionPackage, &pid, sizeof(uint32_t));
    addToPackage(instructionPackage, &pc, sizeof(uint32_t));
    
    sendPackage(instructionPackage, connectionSocket);
    log_info(kernelLog, "Paquete enviado");
}

void sendInterruptToCpu(tCpu* cpu){
    tPackage* interruptPackage = createPackage(KERNEL_TO_CPU_INTERRUPT_INTERRUPTION);
    log_info(kernelLog, "A punto de enviar el paquete informando una interrupcion, desde Kernel a CPU");

    sendPackage(interruptPackage, cpu->connectionSocketInterrupt);
    cpu->preempted = 1;
    log_info(kernelLog, "Paquete con interrupcion enviado");
}

// Función que envía a Memoria una solicitud de carga de un proceso, le envía el PID del proceso, el nobmre de su archivo de pseudocódigo, y el tamaño:
// La función recibe un puntero al proceso, no un pid:
void sendRequestToLoadProcessToMemory(tPcb* process){
    int connectionSocketMemory = requestingConnectionSameThread(kernelLog, kernelConfig->IP_MEMORIA, kernelConfig->PUERTO_MEMORIA, handshakeFromKernelToMemoria);

    tPackage* loadProcessPackage = createPackage(KERNEL_TO_MEMORY_REQUEST_TO_LOAD_PROCESS);
    addToPackage(loadProcessPackage, &process->pid, sizeof(uint32_t));
    addToPackage(loadProcessPackage, process->pseudocodeFileName, string_length(process->pseudocodeFileName) + 1);
    addToPackage(loadProcessPackage, &process->size, sizeof(uint32_t));

    log_info(kernelLog, "A punto de enviar el paquete con el proceso a cargar a Memoria");

    sendPackage(loadProcessPackage, connectionSocketMemory);

    process->attemptingEntryToMemory = 1;

    log_info(kernelLog, "Solicitud de cargar proceso enviada a Memoria");

    serverForMemoria(connectionSocketMemory);

    // ! ——< Aquí agregamos la llamada automática de prueba para romover el proceso>——
    /*
    // Una vez que tenemos el OK/FAIL de carga, invocamos la remoción:
    log_info(kernelLog, "Auto-REMOVE_PROC: pidiendo a Memoria que borre PID %d", process->pid);
    sendRequestToRemoveProcessToMemory(process->pid);
    */

}



// Función que envía a Memoria una solicitud de remover un proceso, le envía solo el PID del proceso.
// La función recibe un pid, no un puntero al proceso:
void sendRequestToRemoveProcessToMemory(int pid){
    int connectionSocketMemory = requestingConnectionSameThread(kernelLog, kernelConfig->IP_MEMORIA, kernelConfig->PUERTO_MEMORIA, handshakeFromKernelToMemoria);

    tPackage* removeProcessPackage = createPackage(KERNEL_TO_MEMORY_REQUEST_TO_REMOVE_PROCESS);
    addToPackage(removeProcessPackage, &pid, sizeof(uint32_t));

    log_info(kernelLog, "A punto de enviar el paquete con el proceso a remover a Memoria");

    sendPackage(removeProcessPackage, connectionSocketMemory);

    log_info(kernelLog, "Solicitud de remover proceso enviada a Memoria");

    serverForMemoria(connectionSocketMemory);
}

// Función para solicitarle a Memoria que haga el Dump Memory de un proceso:
void sendMemoryDumpRequest(int pid){
    int connectionSocketMemory = requestingConnectionSameThread(kernelLog, kernelConfig->IP_MEMORIA, kernelConfig->PUERTO_MEMORIA, handshakeFromKernelToMemoria);

    tPackage* requestPackage = createPackage(KERNEL_TO_MEMORY_DUMP_REQUEST);
    addToPackage(requestPackage, &pid, sizeof(uint32_t));

    log_info(kernelLog, "A punto de enviar el paquete con la solicitud de Dump a Memoria");

    sendPackage(requestPackage, connectionSocketMemory);

    log_info(kernelLog, "Solicitud de Dump a Memoria enviada");

    serverForMemoria(connectionSocketMemory);
}

// Funcion para solicitar el uso de dispositivos IO desde el kernel
void sendIoUsageRequest(int connectionSocket, int pid, int usageTime){
    tPackage* request = createPackage(KERNEL_TO_IO_USE_REQUEST);
    addToPackage(request, &pid, sizeof(uint32_t));
    addToPackage(request, &usageTime, sizeof(uint32_t));
    sendPackage(request, connectionSocket);
    log_info(kernelLog, "Solicitud de uso de I/O enviada: PID=%d, Tiempo=%d", pid, usageTime);
}