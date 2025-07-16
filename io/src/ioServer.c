#include "io.h"
#include "ioServer.h"

int finishServer;

// Es la función del hilo de IO de recepción de información desde Kernel.
// Dependiendo del código de operación del paquete, decide qué hacer.
// Es bloqueante en receivePackage:
void* serverIoThreadForKernel(void* voidPointerConnectionSocket){
    int connectionSocket = *((int*)voidPointerConnectionSocket);
    free(voidPointerConnectionSocket);

    tPackage* package = NULL;
    t_list* list = NULL;

    log_info(ioLog, "Hilo de servidor para escuchar mensajes del Kernel creado exitosamente.");
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
            case KERNEL_TO_IO_USE_REQUEST: {
                int pid = extractIntElementFromList(list, 0);
                int duration = extractIntElementFromList(list, 1);

                log_info(ioLog, "## PID: %d - Inicio de IO - Tiempo: %d", pid, duration);
                sleep(duration);
                log_info(ioLog, "## PID: %d - Fin de IO", pid);
                break;
            }


            case DO_NOTHING:
                log_info(ioLog, "RECIBIDO MENSAJE VACIO DESDE KERNEL.");
                break;
            case ERROR:
            default:
                log_error(ioLog, "Código de operación no reconocido: %d", package->operationCode);
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