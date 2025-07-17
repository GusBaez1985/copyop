#include "kernel.h"

int main(int argc, char* argv[]){
    // Se setea la señal de finalizar servidor en 0 (falso):
    finishServer = 0;
    // Se crean e inicializan el logger y config:
    kernelLogCreate();
    kernelConfigInitialize();
    // Se inicializan los semáforos para ser mutex, para la lista de estados, para la lista de CPUs, y para la lista de Ios:
    sem_init(&semStates, 0, 1);
    sem_init(&semCpus, 0, 1);
    sem_init(&semIos, 0, 1);

    char* pseudo = NULL;
    char* charSize = NULL;
    int size;
    if (argc < 3){
        log_info(kernelLog, "Ingrese el nombre del archivo de pseudocodigo del proceso inicial:");
        pseudo = readline("> ");
        fflush(stdout);
        log_info(kernelLog, "Ingrese el tamaño del proceso inicial:");
        charSize = readline("> ");
        fflush(stdout);
        size = atoi(charSize);
    }
    else{
        pseudo = string_duplicate(argv[1]);
        size = atoi(string_duplicate(argv[2]));
    }

    //Se inicializan los estados:
    initializeStates();
    initializeCpus();
    initializeIos();

    // El Kernel crea 3 hilos para escuchar conexiones, uno en el PUERTO_ESCUCHA_DISPATCH, otro en el PUERTO_ESCUCHA_INTERRUPT, y otro en el PUERTO_ESCUCHA_IO:
    createThreadForListeningConnections(kernelLog, getKernelConfig()->PUERTO_ESCUCHA_DISPATCH, &finishServer, &listeningSocketDispatch, &establishingKernelConnection);
    createThreadForListeningConnections(kernelLog, getKernelConfig()->PUERTO_ESCUCHA_INTERRUPT, &finishServer, &listeningSocketInterrupt, &establishingKernelConnection);
    createThreadForListeningConnections(kernelLog, getKernelConfig()->PUERTO_ESCUCHA_IO, &finishServer, &listeningSocketIo, &establishingKernelConnection);

    // El Kernel le solicita crear una conexión a la memoria (en el enunciado dice que las conexiones con memoria son efímeras, así que este comportamiento hay que corregirlo más adelante):
    //createThreadForRequestingConnection(kernelLog, getKernelConfig()->IP_MEMORIA, getKernelConfig()->PUERTO_MEMORIA, &handshakeFromKernelToMemoria, &serverThreadForMemoria);

    // Espera un enter por teclado sin texto escrito para poder continuar:
    sleep(2);
    log_info(kernelLog, "Presionar ENTER para iniciar el planificador.");
    char* enter;
    enter = readline(NULL);
    free(enter);

    // Esto debería ocurrir al presionar ENTER, y con el archivo de pseudocodigo y tamaño pasado a los parámetros del main:
    //entryToNew(argv[1], atoi(argv[2]));
    sem_wait(&semStates);
    sem_wait(&semCpus);
    sem_wait(&semIos);
    entryToNew(pseudo, size);
    sem_post(&semIos);
    sem_post(&semCpus);
    sem_post(&semStates);

    // Prueba de impresión de los PID en cada estado:
    sem_wait(&semStates);
    sem_wait(&semCpus);
    sem_wait(&semIos);
    printStatesContent();
    sem_post(&semIos);
    sem_post(&semCpus);
    sem_post(&semStates);

    // Se pide que se ingrese un caracter para que no termine abruptamente, luego se da la señal de finalizar servidor, se cierran los sockets de escucha y se destruyen el logger y config:
    getchar();
    finishServer = 1;
    close(listeningSocketDispatch);
    close(listeningSocketInterrupt);
    close(listeningSocketIo);
    destroyStates();
    destroyCpus();
    destroyIos();
    sem_destroy(&semStates);
    sem_destroy(&semCpus);
    sem_destroy(&semIos);
    logDestroy(kernelLog);
    configDestroy(kernelConfigFile, kernelConfig);
    if (pseudo)
        free(pseudo);
    if (charSize)
        free(charSize);
    return 0;
}