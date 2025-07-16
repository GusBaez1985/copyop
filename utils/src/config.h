#ifndef CONFIG_H
#define CONFIG_H

#include "utils.h"

// Estructura del config del Kernel:
typedef struct{
    char* IP_MEMORIA;
    char* PUERTO_MEMORIA;
    char* PUERTO_ESCUCHA_DISPATCH;
    char* PUERTO_ESCUCHA_INTERRUPT;
    char* PUERTO_ESCUCHA_IO;
    char* ALGORITMO_CORTO_PLAZO;
    char* ALGORITMO_INGRESO_A_READY;
    double ALFA;
    int ESTIMACION_INICIAL;
    int TIEMPO_SUSPENSION;
    char* LOG_LEVEL;
} kernelConfigStruct;

// Estructura del config del CPU:
typedef struct{
    char*   IP_MEMORIA;
    char*   PUERTO_MEMORIA;
    char*   IP_KERNEL;
    char*   PUERTO_KERNEL_DISPATCH;
    char*   PUERTO_KERNEL_INTERRUPT;
    int     ENTRADAS_TLB;
    char*   REEMPLAZO_TLB;
    int     ENTRADAS_CACHE;
    char*   REEMPLAZO_CACHE;
    int     RETARDO_CACHE;
    char*   LOG_LEVEL;
} cpuConfigStruct;

// Estructura del config de la Memoria:
typedef struct{
    char* PUERTO_ESCUCHA;
    int TAM_MEMORIA;
    int TAM_PAGINA;
    int ENTRADAS_POR_TABLA;
    int CANTIDAD_NIVELES;
    int RETARDO_MEMORIA;
    char* PATH_SWAPFILE;
    int RETARDO_SWAP;
    char* LOG_LEVEL;
    char* DUMP_PATH;
    char* PATH_INSTRUCCIONES;
} memoriaConfigStruct;

// Estructura del config de IO:
typedef struct{
    char* IP_KERNEL;
    char* PUERTO_KERNEL;
    char* LOG_LEVEL;
} ioConfigStruct;

void configInitialize(tModule module, t_config** configFile, void** configStruct, t_log* logger);
void configCreate(tModule module, t_config** configFile, t_log* logger);
void configGetData(tModule module, t_config* configFile, void** configStruct, t_log* logger);
void configDestroy(t_config* configFile, void* configStruct);

#endif