#include "io.h"

char* ioDevice = NULL;

int main(int argc, char* argv[]){
    // Setea en 2 el argc para obligar a pasar un dispositivo como parámetro
    if (argc != 2) {
        //log_info(ioLog, "El programa %s requiere el nombre de un dispositivo como argumento.", argv[0]);
        return EXIT_FAILURE;
    }

    // Guarda el nombre del dispositivo que se pasó como argumento
    ioDevice = argv[1];
    //log_info(ioLog, "El dispositivo ingresado es: %s", ioDevice);

    // Se crean e inicializan el logger y config:
    ioLogCreate();
    ioConfigInitialize();

    // ↓↓↓↓↓↓ VER DÓNDE PONER ESTO ↓↓↓↓↓↓
    time_t t = time(NULL);
    struct tm* tiempoLocal = localtime(&t);

    char TIEMPO_IO[32];
    strftime(TIEMPO_IO, sizeof(TIEMPO_IO), "%H:%M:%S", tiempoLocal);
    // ↑↑↑↑↑↑ VER DÓNDE PONER ESTO ↑↑↑↑↑↑

    log_info(ioLog, "## PID: %d - Inicio de IO - Tiempo: %s", getpid(), TIEMPO_IO);

    // Cada módulo IO le solicita crear una conexión al Kernel:
    createThreadForRequestingConnection(ioLog, getIoConfig()->IP_KERNEL, getIoConfig()->PUERTO_KERNEL, &handshakeFromIoToKernel, &serverIoThreadForKernel);

    // Se pide que se ingrese un caracter para que no termine abruptamente, y se destruyen el logger y config:
    getchar();
    log_info(ioLog, "## PID: %d - Fin de IO", getpid());
    logDestroy(ioLog);
    configDestroy(ioConfigFile, ioConfig);
    return 0;
}
