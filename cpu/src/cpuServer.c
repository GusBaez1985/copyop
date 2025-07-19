#include "cpu.h"
#include "cpuServer.h"


int finishServer;

// Es la función del hilo de CPU Dispatch de recepción de información desde Kernel.
// Dependiendo del código de operación del paquete, decide qué hacer.
// Es bloqueante en receivePackage:
void *serverDispatchThreadForKernel(void *voidPointerConnectionSocket)
{
    int connectionSocket = *((int *)voidPointerConnectionSocket);
    free(voidPointerConnectionSocket);


    connectionSocketDispatchKernel = connectionSocket;
    tPackage *package = NULL;
    t_list *list = NULL;

    log_info(cpuLog, "Hilo de servidor de CPU Dispatch para escuchar mensajes del Kernel creado exitosamente.");
    while (!finishServer)
    {
        package = receivePackage(connectionSocket);

        if (!package || finishServer)
        {
            if (package)
            {
                destroyPackage(package);
                package = NULL;
            }
            close(connectionSocket);
            break;
        }

        list = packageToList(package);

        switch (package->operationCode)
        {
            case KERNEL_TO_CPU_DISPATCH_TEST: {
                // Se recibe el contexto inicial del Kernel
                list = packageToList(package);
                pid_actual = extractIntElementFromList(list, 0);
                pc_actual = extractIntElementFromList(list, 1);
                list_destroy_and_destroy_elements(list, free);
                list = NULL;

                log_info(cpuLog, "Recibido contexto - PID: %d - PC inicial: %d. Iniciando ciclo de instrucción.", pid_actual, pc_actual);
                
                ciclo_de_instruccion_activo = true;
                
                // --- INICIA EL BUCLE DEL CICLO DE INSTRUCCIÓN ---
                while(ciclo_de_instruccion_activo) {
                    // 1. Fetch: Pedimos la instrucción a Memoria
                    fetch(pid_actual, pc_actual);

                    // 2. Esperar: El hilo se bloquea aquí hasta que el hilo de memoria 
                    //    termine de ejecutar la instrucción y haga sem_post()
                    sem_wait(&sem_ciclo_instruccion); 
                }
                
                log_info(cpuLog, "PID: %d - Ciclo de instrucción finalizado. Esperando nuevo contexto.", pid_actual);
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

        if (list)
        {
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
void *serverInterruptThreadForKernel(void *voidPointerConnectionSocket)
{
    int connectionSocket = *((int *)voidPointerConnectionSocket);
    free(voidPointerConnectionSocket);

    tPackage *package = NULL;
    t_list *list = NULL;

    log_info(cpuLog, "Hilo de servidor de CPU Interrupt para escuchar mensajes del Kernel creado exitosamente.");
    while (!finishServer)
    {
        package = receivePackage(connectionSocket);

        if (!package || finishServer)
        {
            if (package)
            {
                destroyPackage(package);
                package = NULL;
            }
            close(connectionSocket);
            break;
        }

        list = packageToList(package);

        switch (package->operationCode)
        {
        case DO_NOTHING:
            log_info(cpuLog, "RECIBIDO MENSAJE VACIO DESDE KERNEL.");
            break;
        case ERROR:
        default:
            break;
        }

        destroyPackage(package);
        package = NULL;

        if (list)
        {
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
void *serverThreadForMemoria(void *voidPointerConnectionSocket)
{
    int connectionSocket = *((int *)voidPointerConnectionSocket);
    free(voidPointerConnectionSocket);

    tPackage *package = NULL;
    t_list *list = NULL;

    log_info(cpuLog, "Hilo de servidor de CPU para escuchar mensajes de Memoria creado exitosamente.");
    while (!finishServer)
    {
        package = receivePackage(connectionSocket);

        if (!package || finishServer)
        {
            if (package)
            {
                destroyPackage(package);
                package = NULL;
            }
            close(connectionSocket);
            break;
        }

        list = packageToList(package);

        switch (package->operationCode)
        {


            case MEMORIA_TO_CPU_SEND_INSTRUCTION: {
                // Obtenemos el puntero a la instrucción, NO lo liberamos aquí.
                char* instruction_string = extractStringElementFromList(list, 0);
                log_info(cpuLog, "Instrucción recibida desde Memoria: %s", instruction_string);

                tDecodedInstruction* decoded = decode(instruction_string);

                // Verificamos si es EXIT para terminar el ciclo
                if (decoded->operation == EXIT_INST) {
                    log_info(cpuLog, "PID: %d - Instrucción EXIT recibida. Finalizando ejecución.", pid_actual);
                    ciclo_de_instruccion_activo = false;
                    // Aquí deberías enviar el contexto de vuelta al Kernel con motivo EXIT
                    execute(decoded); // execute se encarga de liberar la estructura 'decoded'
                } else {
                    execute(decoded); // execute se encarga de liberar la estructura 'decoded'
                    // Actualizamos el PC si la instrucción no fue GOTO
                    if (decoded->operation != GOTO) {
                        pc_actual++;
                    }
                }
                
                // Le avisamos al hilo de dispatch que el ciclo puede continuar
                sem_post(&sem_ciclo_instruccion);
                break; // Salimos del switch
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

        if (list)
        {
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