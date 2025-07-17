#include "cpu.h"
#include "cpuClient.h"



// Función del handshake inicial entre CPU Dispatch y Kernel.
// El CPU Dispatch envía un paquete que solo tiene el código de operación CPU_DISPATCH_HANDSHAKE,
// Si el kernel lo recibió bien, debe contestar con otro paquete que solo tiene el código
// de operación KERNEL_OK:
int handshakeFromDispatchToKernel(int connectionSocket){
    tPackage* handshakePackage = createPackage(CPU_DISPATCH_HANDSHAKE);
    
    addToPackage(handshakePackage, &cpuId, sizeof(int));
    log_info(cpuLog, "A punto de enviar mensaje de handshake desde CPU Dispatch a kernel.");
    sendPackage(handshakePackage, connectionSocket);
    log_info(cpuLog, "Enviado mensaje de handshake desde CPU Dispatch a kernel.");

    tPackage* receivedPackage = receivePackage(connectionSocket);
    log_info(cpuLog, "Recibida respuesta desde Kernel al handshake de CPU Dispatch.");
    tOperationCode receivedCode = receivedPackage->operationCode;

    destroyPackage(receivedPackage);

    return receivedCode == KERNEL_OK;
}

// Función del handshake inicial entre CPU Interrupt y Kernel.
// El CPU Dispatch envía un paquete que solo tiene el código de operación CPU_INTERRUPT_HANDSHAKE,
// Si el kernel lo recibió bien, debe contestar con otro paquete que solo tiene el código
// de operación KERNEL_OK:
int handshakeFromInterruptToKernel(int connectionSocket){
    tPackage* handshakePackage = createPackage(CPU_INTERRUPT_HANDSHAKE);
    
    addToPackage(handshakePackage, &cpuId, sizeof(int));
    log_info(cpuLog, "A punto de enviar mensaje de handshake desde CPU Interrupt a kernel.");
    sendPackage(handshakePackage, connectionSocket);
    log_info(cpuLog, "Enviado mensaje de handshake desde CPU Interrupt a kernel.");

    tPackage* receivedPackage = receivePackage(connectionSocket);
    log_info(cpuLog, "Recibida respuesta desde Kernel al handshake de CPU Interrupt.");
    tOperationCode receivedCode = receivedPackage->operationCode;

    destroyPackage(receivedPackage);

    return receivedCode == KERNEL_OK;
}

// Función del handshake inicial entre CPU Dispatch y Memoria.
// El CPU Dispatch envía un paquete que solo tiene el código de operación CPU_DISPATCH_HANDSHAKE,
// Si la memoria lo recibió bien, debe contestar con otro paquete que solo tiene el código
// de operación MEMORIA_OK:
int handshakeFromDispatchToMemoria(int connectionSocket){
    tPackage* handshakePackage = createPackage(CPU_DISPATCH_HANDSHAKE);
    log_info(cpuLog, "A punto de enviar mensaje de handshake desde CPU Dispatch a Memoria.");
    sendPackage(handshakePackage, connectionSocket);
    log_info(cpuLog, "Enviado mensaje de handshake desde CPU Dispatch a Memoria.");

    tPackage* receivedPackage = receivePackage(connectionSocket);
    t_list* list = NULL;

    list = packageToList(receivedPackage);

    
    tamanioPagina = extractIntElementFromList(list, 0);
    entradasPorTabla = extractIntElementFromList(list, 1);
    cantidadNiveles = extractIntElementFromList(list, 2);



    log_info(cpuLog, "Recibida respuesta desde Memoria al handshake de CPU Dispatch.");
    log_info(cpuLog, "Recibida configuración de Memoria: TAM_PAGINA=%u, ENTRADAS_POR_TABLA=%u, CANTIDAD_NIVELES=%u",
         tamanioPagina, entradasPorTabla, cantidadNiveles);
    tOperationCode receivedCode = receivedPackage->operationCode;

    destroyPackage(receivedPackage);
    list_destroy_and_destroy_elements(list, free);
    list = NULL;

    connectionSocketMemory = connectionSocket;

    return receivedCode == MEMORIA_OK;
}

// Función del handshake inicial entre CPU Interrupt y Memoria.
// El CPU Dispatch envía un paquete que solo tiene el código de operación CPU_INTERRUPT_HANDSHAKE,
// Si la memoria lo recibió bien, debe contestar con otro paquete que solo tiene el código
// de operación MEMORIA_OK:
int handshakeFromInterruptToMemoria(int connectionSocket){
    tPackage* handshakePackage = createPackage(CPU_INTERRUPT_HANDSHAKE);
    log_info(cpuLog, "A punto de enviar mensaje de handshake desde CPU Interrupt a Memoria.");
    sendPackage(handshakePackage, connectionSocket);
    log_info(cpuLog, "Enviado mensaje de handshake desde CPU Interrupt a Memoria.");

    tPackage* receivedPackage = receivePackage(connectionSocket);
    log_info(cpuLog, "Recibida respuesta desde Memoria al handshake de CPU Interrupt.");
    tOperationCode receivedCode = receivedPackage->operationCode;

    destroyPackage(receivedPackage);

    return receivedCode == MEMORIA_OK;
}





/*
int memory_get_page_table_entry(uint32_t pid, uint32_t table_addr, int level, int entry_index) {
    tPackage* request = createPackage(CPU_TO_MEMORIA_GET_PAGE_TABLE_ENTRY);
    addToPackage(request, &pid, sizeof(uint32_t));
    addToPackage(request, &table_addr, sizeof(uint32_t)); // No se usa en nivel 1, pero se envía siempre para consistencia
    addToPackage(request, &level, sizeof(int));
    addToPackage(request, &entry_index, sizeof(int));

    sendPackage(request, connectionSocketMemory);

    // Esperamos la respuesta de Memoria
    tPackage* response = receivePackage(connectionSocketMemory);

    if (!response || response->operationCode != MEMORIA_TO_CPU_PAGE_TABLE_ENTRY) {
        log_error(cpuLog, "Error recibiendo la entrada de la tabla de páginas desde Memoria.");
        if(response) destroyPackage(response);
        return -1; // Código de error
    }

    int entry_content = extractIntFromPackage(response);
    destroyPackage(response);

    return entry_content;
}
*/
uint64_t memory_get_page_table_entry(uint32_t pid, uint64_t table_addr, int level, int entry_index) {
    tPackage* request = createPackage(CPU_TO_MEMORIA_GET_PAGE_TABLE_ENTRY);
    addToPackage(request, &pid, sizeof(uint32_t));
    addToPackage(request, &table_addr, sizeof(uint64_t)); // Enviamos como uint64_t
    addToPackage(request, &level, sizeof(int));
    addToPackage(request, &entry_index, sizeof(int));

    sendPackage(request, connectionSocketMemory);

    tPackage* response = receivePackage(connectionSocketMemory);

    if (!response || response->operationCode != MEMORIA_TO_CPU_PAGE_TABLE_ENTRY) {
        log_error(cpuLog, "Error recibiendo la entrada de la tabla de páginas desde Memoria.");
        if(response) destroyPackage(response);
        return -1;
    }

    t_list* data = packageToList(response);
    uint64_t entry_content = extractUint64ElementFromList(data, 0); // Usamos la nueva función
    
    list_destroy_and_destroy_elements(data, free);
    destroyPackage(response);

    return entry_content;
}


int memory_read(uint32_t physical_address, uint32_t size, void* buffer_out) {
    // Esta función ahora será síncrona para simplificar. Pide y espera la respuesta.
    tPackage* request = createPackage(CPU_TO_MEMORIA_READ);
    addToPackage(request, &pid_actual, sizeof(uint32_t));
    addToPackage(request, &physical_address, sizeof(uint32_t));
    addToPackage(request, &size, sizeof(uint32_t));
    
    sendPackage(request, connectionSocketMemory);

    // Esperamos la respuesta de Memoria
    tPackage* response = receivePackage(connectionSocketMemory);
    if (!response || response->operationCode != MEMORIA_TO_CPU_READ_RESPONSE) {
        log_error(cpuLog, "Error recibiendo la respuesta de LECTURA desde Memoria.");
        if(response) destroyPackage(response);
        return -1; // Error
    }

    // Copiamos los datos recibidos al buffer de salida
    t_list* data = packageToList(response);
    memcpy(buffer_out, list_get(data, 0), size);
    list_destroy_and_destroy_elements(data, free);
    destroyPackage(response);
    
    return 0; // Éxito
}

int memory_write(uint32_t physical_address, uint32_t size, void* buffer_in) {
    tPackage* request = createPackage(CPU_TO_MEMORIA_WRITE);
    addToPackage(request, &pid_actual, sizeof(uint32_t));
    addToPackage(request, &physical_address, sizeof(uint32_t));
    addToPackage(request, &size, sizeof(uint32_t));
    addToPackage(request, buffer_in, size);
    
    sendPackage(request, connectionSocketMemory);

    // Esperamos el ACK de Memoria
    tPackage* response = receivePackage(connectionSocketMemory);
    if (!response || response->operationCode != MEMORIA_TO_CPU_WRITE_ACK) {
        log_error(cpuLog, "Error recibiendo el ACK de ESCRITURA desde Memoria.");
        if(response) destroyPackage(response);
        return -1; // Error
    }

    destroyPackage(response);
    return 0; // Éxito
}