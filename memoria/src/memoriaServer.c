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

        switch(package->operationCode){
            case CPU_DISPATCH_TO_MEMORIA_TEST:
                char* message = extractMessageFromPackage(package);
                log_info(memoriaLog, "%s", message);
                free(message);
                break;


            case CPU_TO_MEMORIA_GET_PAGE_TABLE_ENTRY: {
                // Extraer los parámetros de la petición de la CPU.
                t_list* data = packageToList(package);
                int pid = extractIntElementFromList(data, 0);
                int level = extractIntElementFromList(data, 2);
                int entry_index = extractIntElementFromList(data, 3);
                list_destroy_and_destroy_elements(data, free);

                // Buscamos el proceso en nuestro diccionario de procesos activos.
                char* pidKey = string_itoa(pid);
                t_memoriaProcess* proc = dictionary_get(activeProcesses, pidKey);
                free(pidKey);

                int entry_content = -1; // Valor por defecto en caso de error.

                if (!proc) {
                    log_error(memoriaLog, "PID: %d no encontrado en memoria para búsqueda de tabla de páginas.", pid);
                } 
                else {
                    // 3. --- LÓGICA PARA BUSCAR ENTRADA ---
                    void* current_table = proc->pageTables[level - 1]; // Los arrays son base 0, los niveles base 1.
                    
                    if (!current_table) {
                        log_error(memoriaLog, "Error de acceso: Tabla de nivel %d para PID %d no existe.", level, pid);
                    } else {
                        // Si es el último nivel, la tabla contiene los números de marco (int).
                        // Si no, contiene punteros a la siguiente tabla (void*).
                        if (level == proc->levels) {
                            entry_content = ((int*)current_table)[entry_index];
                        } else {
                            // Aquí necesitamos castear a un puntero a puntero (un array de punteros)
                            // y luego obtener la dirección. Como la enviamos como un int, hacemos un cast.
                            entry_content = (int)(uintptr_t)((void**)current_table)[entry_index];
                        }
                    }
                }

                // 4. Logueamos la operación y respondemos a la CPU.
                log_info(memoriaLog, "PID: %d - Solicitud TP Nivel: %d, Entrada: %d -> Valor: %d", pid, level, entry_index, entry_content);

                tPackage* response = createPackage(MEMORIA_TO_CPU_PAGE_TABLE_ENTRY);
                addToPackage(response, &entry_content, sizeof(int));
                sendPackage(response, connectionSocket);
                
                break;
            }


            case CPU_TO_MEMORIA_FETCH_INSTRUCTION: {
                // Extrae el PID y el PC que CPU envió 
                t_list* data = packageToList(package);
                int pid = extractIntElementFromList(data, 0);
                int programCounter = extractIntElementFromList(data, 1);

                log_info(memoriaLog, "[FETCH] CPU solicita instrucción: PID=%d, PC=%d", pid, programCounter);

                // --- Convertir PID a clave para el diccionario ---
                // Esto lo usamos para recuperar nuestra estructura t_memoriaProcess
                char* pidKey = string_itoa(pid);
                t_memoriaProcess* proc = dictionary_get(activeProcesses, pidKey);
                free(pidKey);

                // --- Obtener la instrucción real ---
                // Antes (mock) hacíamos:
                //   char* inst = string_from_format("NOOP");
                // Ahora: buscamos en proc->instructions[programCounter]
                char* inst;
                if (proc != NULL && programCounter < proc->instructionCount) {
                    // Copiamos la línea exacta del pseudocódigo
                    inst = string_duplicate(proc->instructions[programCounter]);

                } else {
                    // Si no existe proceso o programCounter está fuera de rango, devolvemos EXIT
                    inst = string_from_format("EXIT");
                }
                // inst ahora contiene la instrucción a enviar al CPU
                log_info(memoriaLog, "INSTRUCCION A ENVIAR %s", inst);
                // --- Empaquetar y enviar la respuesta al CPU ---
                tPackage* rsp =  createPackage(MEMORIA_TO_CPU_SEND_INSTRUCTION);
                addToPackage(rsp, inst, strlen(inst) + 1);
                sendPackage(rsp, connectionSocket);
                destroyPackage(rsp);

                // --- Limpiar memoria auxiliar ---
                free(inst);

                break;
            
            }


            case CPU_TO_MEMORIA_READ: {
                t_list* params = packageToList(package);
                int pid = extractIntElementFromList(params, 0);
                int physical_address = extractIntElementFromList(params, 1);
                int size = extractIntElementFromList(params, 2);
                list_destroy_and_destroy_elements(params, free);

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
                t_list* params = packageToList(package);
                int pid = extractIntElementFromList(params, 0);
                int physical_address = extractIntElementFromList(params, 1); 
                int size = extractIntElementFromList(params, 2);
                void* data_to_write = malloc(size);
                memcpy(data_to_write, list_get(params, 3), size); 
                list_destroy_and_destroy_elements(params, free);

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
                // Extraer el PID
                int pid = extractIntElementFromList(list, 0);
                log_info(memoriaLog, "## (%d) - REMOVE_PROC recibido", pid);

                // Construir clave para el diccionario
                char* pidKey = string_itoa(pid);

                // Buscar el proceso en memoria
                t_memoriaProcess* proc = dictionary_get(activeProcesses, pidKey);

                tPackage* resp;
                if (proc) {
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



// Devuelve DF para una dirección lógica DL
int translateAddress(t_memoriaProcess* proc, int dl) {
    int pageSize           = getMemoriaConfig()->TAM_PAGINA;
    int levels             = proc->levels;
    int entriesPerTable    = proc->entriesPerTable;

    int pageNumber = dl / pageSize;
    int offset     = dl % pageSize;

    void* current = proc->pageTables[0];
    for (int lvl = 0; lvl < levels; lvl++) {
        int divisor = pow(entriesPerTable, levels - lvl - 1);
        int index   = pageNumber / divisor;
        pageNumber  %= divisor;

        if (lvl == levels - 1) {
            // Último nivel: current es un int*
            int frame = ((int*)current)[index];
            return frame * pageSize + offset;
        } else {
            // Desciende a la siguiente subtabla
            current = ((void**)current)[index];
            if (!current) {
                log_error(memoriaLog,
                  "PF: lvl=%d índice=%d ausente (DL=%d)", lvl, index, dl);
                return -1;  // page fault
            }
        }
    }
    return -1;  // nunca debería llegar
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


/*
 * createProcess:
 *   - Reserva los marcos necesarios en el bitmap según el tamaño requerido.
 *   - Lee el archivo de pseudocódigo y lo parte en líneas.
 *   - Crea y devuelve una estructura t_memoriaProcess con:
 *       pid               = identificador del proceso
 *       numPages          = cantidad de páginas reservadas
 *       frames            = array de índices de marcos asignados
 *       instructions      = arreglo de strings con cada instrucción
 *       instructionCount  = número de líneas/instrucciones
 *   - Devuelve NULL si no hay suficientes marcos libres o falla algún malloc.
 */

t_memoriaProcess* createProcess(int pid, int sizeBytes, const char* pseudocodeFileName) {
    // Parámetros de configuración 
    int pageSize         = getMemoriaConfig()->TAM_PAGINA;
    int totalMem         = getMemoriaConfig()->TAM_MEMORIA;
    int entriesPerTable  = getMemoriaConfig()->ENTRADAS_POR_TABLA;
    int levels           = getMemoriaConfig()->CANTIDAD_NIVELES;

    // Cálculo de páginas necesarias y marcos disponibles 
    int pagesNeeded = ceil((double)sizeBytes / pageSize);
    int totalFrames = totalMem / pageSize;

    // Contar marcos libres en el bitmap 
    int freeCount = 0;
    for (int i = 0; i < totalFrames; i++) {
        if (!bitarray_test_bit(frameBitmap, i)) {
            freeCount++;
        }
    }
    if (freeCount < pagesNeeded) {
        log_error(memoriaLog,"No hay espacio para pid=%d: necesita %d páginas, libres %d", pid, pagesNeeded, freeCount);
        return NULL;
    }

    // Reservar marcos y guardar índices 
    int* frames = malloc(sizeof(int) * pagesNeeded);
    if (!frames) {
        log_error(memoriaLog, "Fallo malloc(frames) para pid=%d", pid);
        return NULL;
    }
    int reserved = 0;
    for (int i = 0; i < totalFrames && reserved < pagesNeeded; i++) {
        if (!bitarray_test_bit(frameBitmap, i)) {
            bitarray_set_bit(frameBitmap, i);
            frames[reserved++] = i;
        }
    }

    // Crear la estructura del proceso y tablas multinivel
    t_memoriaProcess* proc = calloc(1, sizeof(t_memoriaProcess));
    if (!proc) {
        log_error(memoriaLog, "Fallo malloc(t_memoriaProcess) para pid=%d", pid);
        // rollback de frames
        for (int j = 0; j < reserved; j++)
            bitarray_clean_bit(frameBitmap, frames[j]);
        free(frames);
        return NULL;
    }
    proc->pid              = pid;
    proc->numPages         = pagesNeeded;
    proc->levels           = levels;
    proc->entriesPerTable  = entriesPerTable;
    proc->frames           = frames;

    // Reserva el array raíz de tablas: un puntero por nivel
    proc->pageTables = calloc(levels, sizeof(void*));
    if (!proc->pageTables) {
        log_error(memoriaLog,"Fallo calloc(pageTables) para pid=%d", pid);
        // rollback completo
        for (int j = 0; j < reserved; j++)
            bitarray_clean_bit(frameBitmap, frames[j]);
        free(frames);
        free(proc);
        return NULL;
    }

    // Nivel 0: array de punteros o de marcos si sólo hay 1 nivel
    if (levels == 1) {
        proc->pageTables[0] = calloc(entriesPerTable, sizeof(int));
    } else {
        proc->pageTables[0] = calloc(entriesPerTable, sizeof(void*));
    }

    //  Poblar las hojas con los marcos asignados 
    for (int p = 0; p < pagesNeeded; p++) {
        int frame       = frames[p];
        int pageNumber  = p;
        void* current   = proc->pageTables[0];

        for (int lvl = 0; lvl < levels; lvl++) {
            int div = pow(entriesPerTable, levels - lvl - 1);
            int idx = pageNumber / div;
            pageNumber %= div;

            if (lvl == levels - 1) {
                // Estamos en la hoja: escribimos el número de frame
                ((int*)current)[idx] = frame;
            } else {
                // Si no existe la subtabla, la creamos
                void** subtabs = current;
                if (!subtabs[idx]) {
                    // siguiente nivel será hoja si lvl+1 == levels-1
                    size_t sz = (lvl+1 == levels-1)
                              ? entriesPerTable * sizeof(int)
                              : entriesPerTable * sizeof(void*);
                    subtabs[idx] = calloc(entriesPerTable, sz);
                }
                // Descendemos
                current = subtabs[idx];
            }
        }
    }

    // Leer y dividir pseudocódigo en líneas 
    // BORRAR USO PARA DEBUG 
    log_info(memoriaLog, "  -> Leyendo archivo de pseudocódigo: '%s'", pseudocodeFileName);



    const char* basePath = getMemoriaConfig()->PATH_INSTRUCCIONES;
    size_t baseLen = strlen(basePath);
    char* fullPath;
    if (baseLen > 0 && basePath[baseLen - 1] == '/') {
        // ya termina en '/', solo concatenar
        fullPath = string_from_format("%s%s", basePath, pseudocodeFileName);
    } else {
        // no termina en '/', agregar '/'
        fullPath = string_from_format("%s/%s", basePath, pseudocodeFileName);
    }
    log_info(memoriaLog, "→ Leyendo '%s'", fullPath);

    // 5.2) Leer el contenido
    char* fileContent = read_file(fullPath);
    free(fullPath);



    //char* fileContent = read_file(pseudocodeFileName);
    if (!fileContent) {
        log_error(memoriaLog,
                  "No se pudo leer '%s' para pid=%d",
                  pseudocodeFileName, pid);
        // rollback parcial: liberamos frames y estructuras
        for (int j = 0; j < reserved; j++)
            bitarray_clean_bit(frameBitmap, frames[j]);
        free(proc->pageTables);
        free(frames);
        free(proc);
        return NULL;
    }
    proc->instructions = string_split(fileContent, "\n");
    free(fileContent);

    // Contamos líneas
    int count = 0;
    while (proc->instructions[count]) count++;
    proc->instructionCount = count;

    log_info(memoriaLog,"PID %d: %d niveles, %d entradas c/u, %d páginas, %d instrucciones",pid, levels, entriesPerTable, pagesNeeded, count);

    return proc;
}

