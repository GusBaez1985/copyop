#include "cpu.h"
#include "cpuServer.h"
#include "mmu.h"  

// Mapear instrucciones
tInstructionType mapInstruction (char* instruction) {
    if (strcmp(instruction, "NOOP") == 0) return NOOP;
    if (strcmp(instruction, "WRITE") == 0) return WRITE;
    if (strcmp(instruction, "READ") == 0) return READ;
    if (strcmp(instruction, "GOTO") == 0) return GOTO;
    if (strcmp(instruction, "IO") == 0) return IO_INST;
    if (strcmp(instruction, "INIT_PROC") == 0) return INIT_PROC;
    if (strcmp(instruction, "DUMP_MEMORY") == 0) return DUMP_MEMORY;
    if (strcmp(instruction, "EXIT") == 0) return EXIT_INST;
    return UNKNOWN;
}

// Funcion para enviar solicitud de instruccion a memoria
void fetch(int pid, int pc){
    
    // armo solicitud
    tPackage* request = createPackage(CPU_TO_MEMORIA_FETCH_INSTRUCTION);
    addToPackage(request, &pid, sizeof(int));
    addToPackage(request, &pc, sizeof(int));
    sendPackage(request, connectionSocketMemory);
    log_info(cpuLog, "Solicitud de instrucción enviada a Memoria: PID=%d, PC=%d", pid, pc);
    //serverThreadForMemoria(connectionSocketMemory);
    void* serverThreadForMemoria(void* voidPointerConnectionSocket);

}

void requestFreeMemoryMock() {

    tPackage* request = createPackage(GET_MEMORIA_FREE_SPACE);
    sendPackage(request, connectionSocketMemory);
    log_info(cpuLog, "Envio Consulta de Memoria disponible");

    // Recibo respuesta de memoria disponible
    tPackage* response = receivePackage(connectionSocketMemory);
    
    if (response && response->operationCode == GET_MEMORIA_FREE_SPACE) {
        t_list* list = packageToList(response);
        int freeMemory = extractIntElementFromList(list, 0);
        log_info(cpuLog, "Memoria disponible mock: %d", freeMemory);
        list_destroy_and_destroy_elements(list, free); 
    } else {
        log_error(cpuLog, "No se pudo obtener la memoria libre mock.");
        destroyPackage(response);  
    }

    if (response)
        destroyPackage(response);  
}


tDecodedInstruction* decode(char* instruction_string) {
    tDecodedInstruction* decodedInstruction = malloc(sizeof(tDecodedInstruction));

    //! Prueba de escritorio 
    log_info(cpuLog, "Recibida instrucción para decodificar: '%s'", instruction_string);

    char** parts = string_split(instruction_string, " "); // Dividimos la instrucción por espacios
    
    //!1 prueba de escritorio
    log_info(cpuLog, "Instrucción parseada: Operación='%s', Parámetro 1='%s'", parts[0], parts[1]); 

    char* operation_str = parts[0]; // La primera parte es siempre la operación

    decodedInstruction->operation = mapInstruction(operation_str); // Mapeamos el string a nuestro enum
    decodedInstruction->total_params = 0;

    // Copiamos los parámetros (si los hay)
    int i = 1;
    while (parts[i] != NULL) {
        decodedInstruction->params[decodedInstruction->total_params] = strdup(parts[i]);
        decodedInstruction->total_params++;
        i++;
    }

    // Liberamos la memoria usada por string_split
    // OJO: No libera los punteros que copiamos, solo el array de punteros.
    for (int j = 0; parts[j] != NULL; j++) {
        free(parts[j]);
    }
    free(parts);

    log_info(cpuLog, "Instrucción decodificada: %d con %d parámetros", decodedInstruction->operation, decodedInstruction->total_params);
    return decodedInstruction;
}



void execute(tDecodedInstruction* decodedInstruction) {

    switch (decodedInstruction->operation){
        case NOOP:
            log_info(cpuLog, "EMPIEZA NOOP");
            usleep(2000000);
            log_info(cpuLog, "TERMINA NOOP");
            break;
        case READ: {
            //! Prueba de escritorio
            log_info(cpuLog, "Ejecutando: READ. Parámetros: %s, %s", decodedInstruction->params[0], decodedInstruction->params[1]);
            // 1. Obtenemos la dirección lógica y el tamaño de la instrucción
            uint32_t logical_address = atoi(decodedInstruction->params[0]);
            uint32_t size_to_read = atoi(decodedInstruction->params[1]);
            uint32_t physical_address;

            // 2. Llamar a MMU para que traduzca
            tMmuStatus status = translate_address(pid_actual, logical_address, &physical_address);

            if (status == MMU_SEG_FAULT) {
                // La traducción falló (Page Fault). Informar al Kernel.
                log_error(cpuLog, "PID: %u - SEGMENTATION FAULT al intentar leer la dirección lógica %u", pid_actual, logical_address);
                // TODO: Enviar paquete a Kernel con motivo de SEG_FAULT y devolver el PCB.
                break;
            }

            // 3. La traducción fue exitosa. Enviamos la DIRECCIÓN FÍSICA a Memoria.
            log_info(cpuLog, "PID: %u - Acción: LEER - Dir. Lógica: %u -> Dir. Física: %u - Tamaño: %u", 
                     pid_actual, logical_address, physical_address, size_to_read);
            
            tPackage* req = createPackage(CPU_TO_MEMORIA_READ);
            addToPackage(req, &pid_actual, sizeof(uint32_t));
            addToPackage(req, &physical_address, sizeof(uint32_t)); // enviar la física!
            addToPackage(req, &size_to_read, sizeof(uint32_t));
            sendPackage(req, connectionSocketMemory);
            
            // La respuesta de la memoria la maneja el hilo `serverThreadForMemoria`
            break;
        }

        case WRITE: {
            //! Prueba de escritorio
            log_info(cpuLog, "Ejecutando: WRITE. Parámetros: %s, %s", decodedInstruction->params[0], decodedInstruction->params[1]);
            // Obtenemos los parámetros de la instrucción decodificada.
            uint32_t logical_address = atoi(decodedInstruction->params[0]);
            char* data_to_write = decodedInstruction->params[1];
            uint32_t data_size = strlen(data_to_write) + 1; // +1 para el terminador '\0'
            uint32_t physical_address;

            // Traducimos la dirección lógica a física usando la MMU.
            tMmuStatus status = translate_address(pid_actual, logical_address, &physical_address);

            if (status == MMU_SEG_FAULT) {
                log_error(cpuLog, "PID: %u - SEGMENTATION FAULT al intentar escribir en la dirección lógica %u", pid_actual, logical_address);
                // TODO: Enviar paquete a Kernel con motivo de SEG_FAULT.
                break;
            }

            // Logueamos la acción antes de enviar el paquete.
            log_info(cpuLog, "PID: %u - Acción: ESCRIBIR - Dir. Lógica: %u -> Dir. Física: %u - Valor: %s", 
                    pid_actual, logical_address, physical_address, data_to_write);

            //  Creamos el paquete y lo enviamos a Memoria con los 4 parámetros.
            tPackage* req = createPackage(CPU_TO_MEMORIA_WRITE);
            addToPackage(req, &pid_actual, sizeof(uint32_t));
            addToPackage(req, &physical_address, sizeof(uint32_t)); // La dirección física
            addToPackage(req, &data_size, sizeof(uint32_t));      // El tamaño de los datos
            addToPackage(req, data_to_write, data_size);          // Los datos en sí
            
            sendPackage(req, connectionSocketMemory);
            
            // La CPU ahora debe esperar el MEMORIA_TO_CPU_WRITE_ACK en su hilo de servidor
            // para continuar con la siguiente instrucción.
            break;
        }
        case GOTO:
            break;
        case IO_INST:
            break;
        case INIT_PROC:
            break;
        case DUMP_MEMORY:
            break;
        case EXIT_INST:
        {
            log_info(cpuLog, "PID: %d - Syscall: EXIT. Notificando al Kernel.", pid_actual);
            tPackage* package_to_kernel = createPackage(CPU_DISPATCH_TO_KERNEL_EXIT);
            sendPackage(package_to_kernel, connectionSocketDispatchKernel);
            // Detenemos el ciclo de instrucción localmente
            ciclo_de_instruccion_activo = false;
            break;
        }

        default:
            break;
    }

    free(decodedInstruction);
}