#include "cpu.h"
#include "cpuServer.h"

int connectionSocketMemory;
int finishServer;

// Es la función del hilo de CPU Dispatch de recepción de información desde Kernel.
// Dependiendo del código de operación del paquete, decide qué hacer.
// Es bloqueante en receivePackage:
void* serverDispatchThreadForKernel(void* voidPointerConnectionSocket){
    int connectionSocket = *((int*)voidPointerConnectionSocket);
    free(voidPointerConnectionSocket);

    tPackage* package = NULL;
    t_list* list = NULL;

    log_info(cpuLog, "Hilo de servidor de CPU Dispatch para escuchar mensajes del Kernel creado exitosamente.");
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
            case KERNEL_TO_CPU_DISPATCH_TEST:
                // Recibir el contexto de ejecución (sin deserializar por ahora)
                tContextoEjecucion contexto;

                // Se copian los valores de la lista (la cual se cargó con el contenido del paquete recibido), en la estructura contexto:
                contexto.pid = extractIntElementFromList(list, 0);
                contexto.program_counter = extractIntElementFromList (list, 1);

                // Log para ver que lo recibimos bien
                log_info(cpuLog, "Recibido contexto - PID: %d - PC: %d", contexto.pid, contexto.program_counter);

                fetch(contexto.pid, contexto.program_counter);

                break;



            case KERNEL_TO_CPU_INIT_PROCESS: {
                t_list* data = packageToList(package);
                // 1) Guardamos PID globalmente
                pid_actual = extractIntElementFromList(data, 0);
                // 2) Leemos el nombre del script
                char* pseudocodeFile = extractStringElementFromList(data, 1);
                // 3) Leemos el tamaño
                uint32_t sizeBytes = extractIntElementFromList(data, 2);
                list_destroy_and_destroy_elements(data, free);

                log_info(cpuLog,        "INIT_PROC desde Kernel: PID=%u, archivo=%s, size=%u",
                        pid_actual, pseudocodeFile, sizeBytes);

                // TODO: guarda pseudocodeFile y sizeBytes en tu PCB local
                free(pseudocodeFile);
                break;
            }

            case DO_NOTHING:
                log_info(cpuLog, "RECIBIDO MENSAJE VACIO DESDE KERNEL.");
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

// Es la función del hilo de CPU Interrupt de recepción de información desde Kernel.
// Dependiendo del código de operación del paquete, decide qué hacer.
// Es bloqueante en receivePackage:
void* serverInterruptThreadForKernel(void* voidPointerConnectionSocket){
    int connectionSocket = *((int*)voidPointerConnectionSocket);
    free(voidPointerConnectionSocket);

    tPackage* package = NULL;
    t_list* list = NULL;

    log_info(cpuLog, "Hilo de servidor de CPU Interrupt para escuchar mensajes del Kernel creado exitosamente.");
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
            case DO_NOTHING:
                log_info(cpuLog, "RECIBIDO MENSAJE VACIO DESDE KERNEL.");
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

// Es la función del hilo de CPU Dispatch de recepción de información desde Memoria.
// Dependiendo del código de operación del paquete, decide qué hacer.
// Es bloqueante en receivePackage:
void* serverThreadForMemoria(void* voidPointerConnectionSocket){
    int connectionSocket = *((int*)voidPointerConnectionSocket);
    free(voidPointerConnectionSocket);

    tPackage* package = NULL;
    t_list* list = NULL;

    log_info(cpuLog, "Hilo de servidor de CPU para escuchar mensajes de Memoria creado exitosamente.");
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
            
            
            
            case MEMORIA_TO_CPU_SEND_INSTRUCTION: {
                char* instruction = extractStringElementFromList(list, 0);

                log_info(cpuLog, "Instrucción recibida desde Memoria: %s", instruction);

                tDecodedInstruction* decodedInstruction = decode(instruction);
                execute(decodedInstruction);
                
                break;
            }

            //! esto ya no iria por la implementación síncrona en cpuClient.c, espera la respuesta de memoria
            // la función memory_read en cpuClient.c ahora hace el send y se bloquea con receivePackage dentro de la misma función.
            /*
            case MEMORIA_TO_CPU_READ_RESPONSE: {
                // 1) Convertimos el paquete en lista (packageToList ya malloc()ea cada elemento)
                t_list* elems = packageToList(package);
                if (!elems || list_is_empty(elems)) {
                    log_error(cpuLog, "READ_RESPONSE sin datos");
                } else {
                    // 2) El primer elemento es el buffer de 'last_read_size' bytes
                    // void* buffer = list_get(elems, 0);
                    log_info(cpuLog, "READ_RESPONSE recibido: %u bytes", last_read_size);
                    // TODO: procesar 'buffer' (copiar a registros, imprimir, etc.)
                }
                // 3) Liberar
                list_destroy_and_destroy_elements(elems, free);
                break;
            }*/



            case DO_NOTHING:
                log_info(cpuLog, "RECIBIDO MENSAJE VACIO DESDE KERNEL.");
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