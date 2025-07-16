
#include "memoria.h"



int main(int argc, char* argv[]){
    // Se setea la señal de finalizar servidor en 0 (falso):
    finishServer = 0;
    // Se crean e inicializan el logger y config:
    memoriaLogCreate();
    memoriaConfigInitialize();


    // ! CheckPoint 3: Inicializo memoria
    // Reserva bloque de RAM
    // Crea bitmap de frames
    // (En memoriaServer.c) define createProcess() para usar bitmap
    initMemory();
    // Crear diccionario PID→t_memoriaProcess*  // se usa DICT para luego buscar por PID
    activeProcesses = dictionary_create();

    // La Memoria crea 1 hilo para escuchar conexiones, todos en el mismo puerto (PUERTO_ESCUCHA) asumo que es porque igualmente sus conexiones deben ser efímeras:
    createThreadForListeningConnections(memoriaLog, getMemoriaConfig()->PUERTO_ESCUCHA, &finishServer, &listeningSocket, &establishingMemoriaConnection);

    // Se pide que se ingrese un caracter para que no termine abruptamente, luego se da la señal de finalizar servidor, se cierran los sockets de escucha y se destruyen el logger y config:
    getchar();
    finishServer = 1;
    close(listeningSocket);
    usleep(2000000);

    // Limpieza de recursos Diccionario de procesos cargados
    dictionary_destroy_and_destroy_elements(activeProcesses, free);
    
    // Bitmap y RAM (asumimos que initMemory guardó el buffer)
    bitarray_destroy(frameBitmap);
    free(memory);


    logDestroy(memoriaLog);
    configDestroy(memoriaConfigFile, memoriaConfig);
    return 0;
}






