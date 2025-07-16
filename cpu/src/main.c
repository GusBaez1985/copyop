#include "cpu.h"
#include "mmu.h" 
uint32_t pid_actual = 0;
int cpuId;
uint32_t tamanioPagina;
uint32_t entradasPorTabla;
uint32_t cantidadNiveles;
uint32_t last_read_size = 0;


int main(int argc, char* argv[]){

    // Valida y asigna el número de CPU
    if (argc < 2){
        cpuLogCreate();
        log_info(cpuLog, "Ingrese el número de CPU:");
        cpuId = atoi(readline("> "));
    }
    else {
        cpuId = atoi(argv[1]);
        cpuLogCreate();
    }

    cpuConfigInitialize();

    // Hilos para solicitar conexiones con Kernel: uno para Distpatch y otro para Interrupt
    createThreadForRequestingConnection(cpuLog, getCpuConfig()->IP_KERNEL, getCpuConfig()->PUERTO_KERNEL_DISPATCH, &handshakeFromDispatchToKernel, &serverDispatchThreadForKernel);
    createThreadForRequestingConnection(cpuLog, getCpuConfig()->IP_KERNEL, getCpuConfig()->PUERTO_KERNEL_INTERRUPT, &handshakeFromInterruptToKernel, &serverInterruptThreadForKernel);

    // Hilo para solicitar conexión con Memoria: solo para Distpatch
    createThreadForRequestingConnection(cpuLog, getCpuConfig()->IP_MEMORIA, getCpuConfig()->PUERTO_MEMORIA, &handshakeFromDispatchToMemoria, &serverThreadForMemoria);

    mmu_init();

    // Se pide que se ingrese un caracter para que no termine abruptamente, y se destruyen el logger y config:
    getchar();
    logDestroy(cpuLog);
    configDestroy(cpuConfigFile, cpuConfig);
    return 0;
}
