#include "memoria.h"
#include "memoriaServer.h"


int finishServer;
int listeningSocket;

void*          memory;           // bloque de RAM simulada
t_bitarray*    frameBitmap;      // bitmap de marcos libres/ocupados
t_dictionary*  activeProcesses;  // diccionario PID → t_memoriaProcess*

// Esta función es un hilo efímero, que es creado por el hilo de escucha, es la función específica que usa la Memoria para
// discriminar qué módulo se le conectó, analizando el handshake recibido (es bloqueante porque se queda resperando a recibir el paquete del handshake),
// y contestando al mismo con otro paquete.
// En base al módulo que se le conectó, crea un hilo para mantener la conexión, lo hace llamando
// a la función createThreadForConnectingToModule.
// Una vez creados ese hilo, o en caso de que falle, este hilo desaparece:
void* establishingMemoriaConnection(void* voidPointerConnectionSocket){
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

    tPackage* responsePackage = createPackage(MEMORIA_OK);

    switch(package->operationCode){
        case CPU_DISPATCH_HANDSHAKE:
            log_info(memoriaLog, "Recibido handshake desde CPU Dispatch.");
            log_info(memoriaLog, "A punto de enviar respuesta del handshake a CPU Dispatch.");

            // Extraemos los valores del config de memoria
            uint32_t tamanioPagina = getMemoriaConfig()->TAM_PAGINA;
            uint32_t entradasPorTabla = getMemoriaConfig()->ENTRADAS_POR_TABLA;
            uint32_t cantidadNiveles = getMemoriaConfig()->CANTIDAD_NIVELES;


            log_info(memoriaLog, "Enviando a CPU: TAM_PAGINA=%u, ENTRADAS_POR_TABLA=%u, CANTIDAD_NIVELES=%u",tamanioPagina, entradasPorTabla, cantidadNiveles);


            addToPackage(responsePackage, &tamanioPagina, sizeof(uint32_t));
            addToPackage(responsePackage, &entradasPorTabla, sizeof(uint32_t));
            addToPackage(responsePackage, &cantidadNiveles, sizeof(uint32_t));

            sendPackage(responsePackage, connectionSocket);
            log_info(memoriaLog, "Enviada respuesta del handshake a CPU Dispatch.");

            createThreadForConnectingToModule(connectionSocket, &serverThreadForCpuDispatch);
            break;
        case CPU_INTERRUPT_HANDSHAKE:
            log_info(memoriaLog, "Recibido handshake desde CPU Interrupt.");
            log_info(memoriaLog, "A punto de enviar respuesta del handshake a CPU Interrupt.");
            sendPackage(responsePackage, connectionSocket);
            log_info(memoriaLog, "Enviada respuesta del handshake a CPU Interrupt.");

            createThreadForConnectingToModule(connectionSocket, &serverThreadForCpuInterrupt);
            break;
        case KERNEL_HANDSHAKE:
            log_info(memoriaLog, "Recibido handshake desde Kernel.");
            log_info(memoriaLog, "A punto de enviar respuesta del handshake a Kernel.");
            sendPackage(responsePackage, connectionSocket);
            log_info(memoriaLog, "Enviada respuesta del handshake a Kernel.");

            createThreadForConnectingToModule(connectionSocket, &serverThreadForKernel);
            break;
        
        // Flujo memoria Kernel (INIT_PROC Planificador de Corto Plazo)
 

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

// Es la función del hilo de Memoria de recepción de información desde CPU Dispatch.
// Dependiendo del código de operación del paquete, decide qué hacer.
// Es bloqueante en receivePackage:
void* serverThreadForCpuDispatch(void* voidPointerConnectionSocket){
    int connectionSocket = *((int*)voidPointerConnectionSocket);
    free(voidPointerConnectionSocket);

    tPackage* package = NULL;
    t_list* list = NULL;

    log_info(memoriaLog, "Hilo de servidor para escuchar mensajes del CPU Dispatch creado exitosamente.");
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
        // ! Prueba de escritorio
        log_info(memoriaLog, "CPU me pide operación: %d", package->operationCode);
        switch(package->operationCode){
            
            case CPU_DISPATCH_TO_MEMORIA_TEST:
                char* message = extractMessageFromPackage(package);
                log_info(memoriaLog, "%s", message);
                free(message);
                break;

            case CPU_TO_MEMORIA_FETCH_INSTRUCTION: {

                log_info(memoriaLog, "Aplicando retardo de memoria para FETCH_INSTRUCTION...");
                usleep(getMemoriaConfig()->RETARDO_MEMORIA * 1000);
                // Extrae el PID y el PC que CPU envió 
                t_list* data = packageToList(package);
                int pid = extractIntElementFromList(data, 0);
                int programCounter = extractIntElementFromList(data, 1);
                // Es importante destruir la lista 'data' aquí para no tener memory leaks.
                list_destroy_and_destroy_elements(data, free); 

                log_info(memoriaLog, "[FETCH] CPU solicita instrucción: PID=%d, PC=%d", pid, programCounter);

                char* pidKey = string_itoa(pid);
                t_memoriaProcess* proc = dictionary_get(activeProcesses, pidKey);
                free(pidKey);

                char* inst;
                if (proc != NULL && programCounter < proc->instructionCount) {
                    inst = string_duplicate(proc->instructions[programCounter]);
                } else {
                    inst = string_from_format("EXIT");
                }
                
                log_info(memoriaLog, "INSTRUCCION A ENVIAR %s", inst);
                
                tPackage* rsp =  createPackage(MEMORIA_TO_CPU_SEND_INSTRUCTION);
                addToPackage(rsp, inst, strlen(inst) + 1);
                
                sendPackage(rsp, connectionSocket);


                free(inst);
                break;
            }


            case CPU_TO_MEMORIA_GET_PAGE_TABLE_ENTRY: {
                log_info(memoriaLog, "Aplicando retardo de memoria para acceso a Tabla de Páginas...");
                usleep(getMemoriaConfig()->RETARDO_MEMORIA * 1000);
                list = packageToList(package);
                int pid = extractIntElementFromList(list, 0);
                uint64_t table_addr_from_cpu = extractUint64ElementFromList(list, 1); // Usamos la nueva función
                int level_requested = extractIntElementFromList(list, 2);
                int entry_index = extractIntElementFromList(list, 3);
                
                log_info(memoriaLog, "PID: %d -> Petición de TP [Nivel: %d, Entrada: %d, Addr de Tabla: %lu]", pid, level_requested, entry_index, (unsigned long)table_addr_from_cpu);

                char* pidKey = string_itoa(pid);
                t_memoriaProcess* proc = dictionary_get(activeProcesses, pidKey);
                free(pidKey);

                uint64_t content_to_send = -1; // Usamos uint64_t para la respuesta

                if (!proc) {
                    log_error(memoriaLog, "¡ERROR CRÍTICO! PID: %d no encontrado.", pid);
                } else {
                    proc->accesos_a_tabla_paginas++;
                    void* table_to_read;
                    if (level_requested == 1) {
                        table_to_read = proc->pageTables;
                    } else {
                        table_to_read = (void*)(uintptr_t)table_addr_from_cpu;
                    }

                    if (table_to_read == NULL) {
                        log_error(memoriaLog, "¡ERROR CRÍTICO! Intento de acceso a tabla de nivel %d para PID %d, pero el puntero es NULO.", level_requested, pid);
                    } else {
                        if (level_requested < proc->levels) {
                            void* next_table_ptr = ((void**)table_to_read)[entry_index];
                            content_to_send = (uint64_t)(uintptr_t)next_table_ptr;
                        } else {
                            content_to_send = (uint64_t)((int*)table_to_read)[entry_index];
                        }
                    }
                }

                log_info(memoriaLog, "PID: %d -> Respuesta de TP [Nivel: %d, Entrada: %d] -> Contenido: %lu", pid, level_requested, entry_index, (unsigned long)content_to_send);

                tPackage* response = createPackage(MEMORIA_TO_CPU_PAGE_TABLE_ENTRY);
                addToPackage(response, &content_to_send, sizeof(uint64_t)); // Enviamos como uint64_t
                sendPackage(response, connectionSocket);
                
                break;
            }


            case CPU_TO_MEMORIA_READ: {
                log_info(memoriaLog, "Aplicando retardo de memoria para LECTURA...");
                usleep(getMemoriaConfig()->RETARDO_MEMORIA * 1000);
                
                t_list* params = packageToList(package);
                int pid = extractIntElementFromList(params, 0);
                int physical_address = extractIntElementFromList(params, 1);
                int size = extractIntElementFromList(params, 2);
                list_destroy_and_destroy_elements(params, free);

                // treamos para metricas
                char* pidKey = string_itoa(pid);
                t_memoriaProcess* proc = dictionary_get(activeProcesses, pidKey);
                free(pidKey);
                if (proc) {
                    proc->lecturas_en_memoria++;
                    log_info(memoriaLog, "PID: %d - Métrica de lectura incrementada a: %d", pid, proc->lecturas_en_memoria);
                }
                
                log_info(memoriaLog, "PID: %d - Acción: LEER - Dir. Física: %d - Tamaño: %d", pid, physical_address, size);

                // Crear un buffer temporal para leer desde nuestra memoria principal
                void* buffer_out = malloc(size);
                memcpy(buffer_out, memory + physical_address, size);

                // Enviar la respuesta a la CPU
                tPackage* response = createPackage(MEMORIA_TO_CPU_READ_RESPONSE);
                addToPackage(response, buffer_out, size);
                sendPackage(response, connectionSocket);

                free(buffer_out); // Liberamos el buffer temporal
                break;
            }

            case CPU_TO_MEMORIA_WRITE: {
                log_info(memoriaLog, "Aplicando retardo de memoria para ESCRITURA...");
                usleep(getMemoriaConfig()->RETARDO_MEMORIA * 1000);
                t_list* params = packageToList(package);
                int pid = extractIntElementFromList(params, 0);
                int physical_address = extractIntElementFromList(params, 1); 
                int size = extractIntElementFromList(params, 2);
                void* data_to_write = malloc(size);
                memcpy(data_to_write, list_get(params, 3), size); 
                list_destroy_and_destroy_elements(params, free);

                char* pidKey = string_itoa(pid);
                t_memoriaProcess* proc = dictionary_get(activeProcesses, pidKey);
                free(pidKey);

                if (proc) {
                    proc->escrituras_en_memoria++;
                    log_info(memoriaLog, "PID: %d - Métrica de escritura incrementada a: %d", pid, proc->escrituras_en_memoria);
                }


                log_info(memoriaLog, "PID: %d - Acción: ESCRIBIR - Dir. Física: %d - Tamaño: %d", pid, physical_address, size);

                memcpy(memory + physical_address, data_to_write, size);
                free(data_to_write);

                tPackage* response = createPackage(MEMORIA_TO_CPU_WRITE_ACK);
                sendPackage(response, connectionSocket);
                break;
            }



            case GET_MEMORIA_FREE_SPACE: {
                int freeSpace = MOCK_FREE_MEMORY;

                // Armar el paquete de respuesta
                tPackage* response = createPackage(GET_MEMORIA_FREE_SPACE);
                addToPackage(response, &freeSpace, sizeof(int));
                sendPackage(response, connectionSocket);
                log_info(memoriaLog, "Respondido espacio libre mock: %d bytes", freeSpace);
                break;
            }

            case DO_NOTHING:
                log_info(memoriaLog, "RECIBIDO MENSAJE VACIO DESDE CPU DISPATCH.");
                break;
            case ERROR:
            default:
                break;
        }

        if (list) {
            list_destroy_and_destroy_elements(list, free);
            list = NULL;
        }

        // 2. Ahora que las copias fueron liberadas, podemos destruir el paquete original de forma segura.
        if (package) {
            destroyPackage(package);
            package = NULL;
        }

    }

    if (package)
        destroyPackage(package);
    if (list)
        list_destroy_and_destroy_elements(list, free);
    return NULL;
}

// Es la función del hilo de Memoria de recepción de información desde CPU Interrupt.
// Dependiendo del código de operación del paquete, decide qué hacer.
// Es bloqueante en receivePackage:
void* serverThreadForCpuInterrupt(void* voidPointerConnectionSocket){
    int connectionSocket = *((int*)voidPointerConnectionSocket);
    free(voidPointerConnectionSocket);

    tPackage* package = NULL;
    t_list* list = NULL;

    log_info(memoriaLog, "Hilo de servidor para escuchar mensajes del CPU Interrupt creado exitosamente.");
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
            case CPU_INTERRUPT_TO_MEMORIA_TEST:
                char* message = extractMessageFromPackage(package);
                log_info(memoriaLog, "%s", message);
                free(message);
                break;
            case DO_NOTHING:
                log_info(memoriaLog, "RECIBIDO MENSAJE VACIO DESDE CPU INTERRUPT.");
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

// Es la función del hilo de Memoria de recepción de información desde Kernel.
// Dependiendo del código de operación del paquete, decide qué hacer.
// Es bloqueante en receivePackage:


// Modificado para conexion efimera

void* serverThreadForKernel(void* voidPointerConnectionSocket){
    int connectionSocket = *((int*)voidPointerConnectionSocket);
    free(voidPointerConnectionSocket);

    //  Log de arranque
    log_info(memoriaLog, "Hilo Kerel→Memoria iniciado, esperando petición del Kernel.");

    //  Recibir UNA ÚNICA petición (INIT o REMOVE)
    tPackage* package = receivePackage(connectionSocket);
    if (package && !finishServer) {
        t_list* list = packageToList(package);

        switch (package->operationCode) {
            case KERNEL_TO_MEMORY_REQUEST_TO_LOAD_PROCESS:
                log_info(memoriaLog, "Aplicando retardo de memoria para KERNEL_TO_MEMORY_REQUEST_TO_LOAD_PROCESS...");
                usleep(getMemoriaConfig()->RETARDO_MEMORIA * 1000);
                // extraigo pid y tamaño del paquete
                int pid = extractIntElementFromList(list , 0);
                char* pseudocodeFileName = extractStringElementFromList(list, 1);
                int sizeBytes = extractIntElementFromList(list, 2); 

                log_info(memoriaLog, "## (%d) - INIT_PROC recibido - archivo=%s - tamaño=%d bytes", pid, pseudocodeFileName, sizeBytes);
                // Intentar crear el proceso en Memoria, reservando marcos y tabla

                t_memoriaProcess* proc = createProcess(pid, sizeBytes, pseudocodeFileName);


                // Enviar la respuesta al Kernel
                tPackage* resp;
                if (proc) {
                    dictionary_put(
                                    activeProcesses,           // diccionario global de Memoria
                                    string_itoa(pid),          // clave: el PID convertido a cadena
                                    proc                       // valor: puntero a t_memoriaProcess*
                                );
                    // Si tuvo éxito, enviamos OK junto al PID
                    resp = createPackage(MEMORY_TO_KERNEL_PROCESS_LOAD_OK);
                    log_info(memoriaLog, "## (%d) - Proceso cargado en memoria con éxito", pid);
                } else {
                    // Si no hay espacio, enviamos FAIL junto al PID
                    resp = createPackage(MEMORY_TO_KERNEL_PROCESS_LOAD_FAIL);
                    log_error(memoriaLog,"## (%d) - Falló carga en memoria (espacio insuficiente)", pid);
                }

                addToPackage(resp, &pid, sizeof(uint32_t));
                sendPackage(resp, connectionSocket);
                break;


            case KERNEL_TO_MEMORY_REQUEST_TO_REMOVE_PROCESS: {
                // REMOVE_PROC: extraer pid, liberar marcos y responder OK/FAIL
                int pid = extractIntElementFromList(list, 0);
                log_info(memoriaLog, "## (%d) - REMOVE_PROC recibido", pid);
                
                char* pidKey = string_itoa(pid);
                t_memoriaProcess* proc = dictionary_get(activeProcesses, pidKey);

                tPackage* resp;
                if (proc) {
                    log_info(memoriaLog,
                             "## PID: <%d> Proceso Destruido - Métricas Acc.T.Pag: <%d>; Inst. Sol.: <%d>; SWAP IN: <%d>; SWAP OUT: <%d>; Lec.Mem.: <%d>; Esc.Mem. <%d>",
                             proc->pid,
                             proc->accesos_a_tabla_paginas,
                             proc->instructionCount,
                             proc->subidas_desde_swap,
                             proc->bajadas_a_swap,
                             proc->lecturas_en_memoria,
                             proc->escrituras_en_memoria);

                    // Liberar marcos en bitmap
                    for (int i = 0; i < proc->numPages; i++) {
                        bitarray_clean_bit(frameBitmap, proc->frames[i]);
                    }
                    // Liberar estructura
                    free(proc->frames);
                    free(proc);

                    // Quitar del diccionario
                    dictionary_remove(activeProcesses, pidKey);

                    // Responder OK
                    resp = createPackage(MEMORY_TO_KERNEL_PROCESS_REMOVED);
                    log_info(memoriaLog, "## (%d) - Proceso removido con éxito", pid);
                    
                } else {
                    // Si no existía, FAIL
                    resp = createPackage(MEMORY_TO_KERNEL_PROCESS_REMOVED);
                    log_error(memoriaLog, "## (%d) - REMOVE_PROC fallo: PID no encontrado", pid);
                }

                //Enviar respuesta al Kernel
                addToPackage(resp, &pid, sizeof(uint32_t));
                sendPackage(resp, connectionSocket);
                // destroyPackage(resp);

                // 6) Liberar la clave
                free(pidKey);
                break;
            }



            default:
                log_warning(memoriaLog, "Código inesperado de Kernel: %d", package->operationCode);
                break;
        }
        destroyPackage(package);

        list_destroy(list);
        list = NULL;

    }

    // 3) Cerrar la conexión y terminar el hilo
    close(connectionSocket);
    log_info(memoriaLog, "Hilo Kernel→Memoria finalizado.");
    return NULL;
}


/* 
* initMemory:
*   • Reserva un bloque contiguo de RAM en 'memory'
*     según memoriaConfig->TAM_MEMORIA.
*   • Crea 'frameBitmap' para manejar marcos de
*     tamaño memoriaConfig->TAM_PAGINA.
*/ 
void initMemory(void) {
    // busca total de memoria del archivo de conf
    size_t totalBytes = getMemoriaConfig()->TAM_MEMORIA;
    memory = malloc(totalBytes);
    if (!memory) {
        log_error(memoriaLog, "Fallo malloc RAM (%zu bytes)", totalBytes);
        exit(EXIT_FAILURE);
    }
    
    log_info(memoriaLog, "RAM simulada reservada: %zu bytes", totalBytes);
    //tama
    size_t pageSize   = getMemoriaConfig()->TAM_PAGINA;
    size_t frameCount = totalBytes / pageSize;
    size_t bitmapBytes = (frameCount + 7) / 8;
    void* buffer = malloc(bitmapBytes);
    frameBitmap = bitarray_create_with_mode(buffer, frameCount, MSB_FIRST);
    log_info(memoriaLog, "Bitmap inicializado: %zu marcos", frameCount);
}




/*
 Lee todo el contenido de un archivo en un buffer NUL-terminado.
 Devuelve NULL si falla (archivo no existe o malloc falla).
*/
char* read_file(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) {
        log_error(memoriaLog, "No se pudo abrir '%s'", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);

    char* buf = malloc(len + 1);
    if (!buf) {
        log_error(memoriaLog, "Fallo malloc(%ld) para '%s'", len + 1, path);
        fclose(f);
        return NULL;
    }

    size_t readBytes = fread(buf, 1, len, f);
    buf[readBytes] = '\0';
    fclose(f);
    return buf;
}






t_memoriaProcess* createProcess(int pid, int sizeBytes, const char* pseudocodeFileName) {
    int pageSize = getMemoriaConfig()->TAM_PAGINA;
    int entriesPerTable = getMemoriaConfig()->ENTRADAS_POR_TABLA;
    int levels = getMemoriaConfig()->CANTIDAD_NIVELES;
    int pagesNeeded = (sizeBytes > 0) ? (int)ceil((double)sizeBytes / pageSize) : 1;
    if (sizeBytes == 0) pagesNeeded = 0;

    int freeCount = 0;
    for (int i = 0; i < (getMemoriaConfig()->TAM_MEMORIA / pageSize); i++) {
        if (!bitarray_test_bit(frameBitmap, i)) freeCount++;
    }
    if (freeCount < pagesNeeded) {
        log_error(memoriaLog, "No hay espacio para pid=%d: necesita %d páginas, libres %d", pid, pagesNeeded, freeCount);
        return NULL;
    }

    int* frames = malloc(sizeof(int) * pagesNeeded);
    int reserved = 0;
    for (int i = 0; i < (getMemoriaConfig()->TAM_MEMORIA / pageSize) && reserved < pagesNeeded; i++) {
        if (!bitarray_test_bit(frameBitmap, i)) {
            bitarray_set_bit(frameBitmap, i);
            frames[reserved++] = i;
        }
    }

    t_memoriaProcess* proc = calloc(1, sizeof(t_memoriaProcess));
    proc->pid = pid;
    proc->numPages = pagesNeeded;
    proc->levels = levels;
    proc->entriesPerTable = entriesPerTable;
    proc->frames = frames;
    // Inicializar  métricas
    proc->accesos_a_tabla_paginas = 0;
    proc->lecturas_en_memoria = 0;
    proc->escrituras_en_memoria = 0;
    proc->bajadas_a_swap = 0;
    proc->subidas_desde_swap = 0;



    if (levels > 0) {
        size_t root_table_size = (levels == 1) ? sizeof(int) : sizeof(void*);
        proc->pageTables = calloc(entriesPerTable, root_table_size);
    } else {
        proc->pageTables = NULL;
    }

    for (int p = 0; p < pagesNeeded; p++) {
        int frame_to_assign = frames[p];
        int page_number_logic = p;
        void* current_table = proc->pageTables;

        for (int lvl = 1; lvl <= levels; lvl++) {
            int entries_in_lower_levels = (int)pow(entriesPerTable, levels - lvl);
            int index = floor(page_number_logic / entries_in_lower_levels);
            
            if (lvl < levels) {
                void** sub_table_pointers = (void**)current_table;
                if (sub_table_pointers[index] == NULL) {
                    size_t next_table_size = ((lvl + 1) == levels) ? sizeof(int) : sizeof(void*);
                    sub_table_pointers[index] = calloc(entriesPerTable, next_table_size);
                }
                current_table = sub_table_pointers[index];
            } else {
                ((int*)current_table)[index] = frame_to_assign;
            }
            page_number_logic %= entries_in_lower_levels;
        }
    }

    const char* basePath = getMemoriaConfig()->PATH_INSTRUCCIONES;
    char* fullPath = string_from_format("%s%s", basePath, pseudocodeFileName);
    char* fileContent = read_file(fullPath);
    free(fullPath);

    if (!fileContent) {
        // ... (Rollback)
        return NULL;
    }
    proc->instructions = string_split(fileContent, "\n");
    free(fileContent);

    int count = 0;
    while (proc->instructions[count]) count++;
    proc->instructionCount = count;

    log_info(memoriaLog,"PID %d: %d niveles, %d entradas c/u, %d páginas, %d instrucciones",pid, levels, entriesPerTable, pagesNeeded, count);
    return proc;
}
