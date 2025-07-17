#ifndef UTILS_H
#define UTILS_H

#include <commons/config.h>
#include <commons/log.h>
#include <commons/string.h>
#include <commons/collections/list.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

// Enumerado para identificar los módulos:
typedef enum{
    KERNEL,
    CPU,
    MEMORIA,
    IO
} tModule;

// Enumerado para listar los códigos de operación que se usarán como protocolo para enviar y recibir información:
typedef enum{
    ERROR = -1,
	DO_NOTHING,

    CPU_DISPATCH_HANDSHAKE,
    CPU_INTERRUPT_HANDSHAKE,
    IO_HANDSHAKE,
    KERNEL_HANDSHAKE,

    KERNEL_OK,
    MEMORIA_OK,

    CPU_DISPATCH_TO_KERNEL_TEST,
    CPU_INTERRUPT_TO_KERNEL_TEST,
    IO_TO_KERNEL_TEST,
    KERNEL_TO_MEMORIA_TEST,
    CPU_DISPATCH_TO_MEMORIA_TEST,
    CPU_INTERRUPT_TO_MEMORIA_TEST,

    KERNEL_TO_IO_USE_REQUEST,
    KERNEL_TO_CPU_DISPATCH_TEST,
    KERNEL_TO_CPU_INTERRUPT_INTERRUPTION,
    KERNEL_TO_MEMORY_REQUEST_TO_LOAD_PROCESS,
    KERNEL_TO_MEMORY_REQUEST_TO_REMOVE_PROCESS,
    KERNEL_TO_MEMORY_DUMP_REQUEST,

    MEMORY_TO_KERNEL_PROCESS_LOAD_OK,
    MEMORY_TO_KERNEL_PROCESS_LOAD_FAIL,
    MEMORY_TO_KERNEL_PROCESS_REMOVED,
    MEMORY_TO_KERNEL_DUMP_COMPLETED,
    MEMORY_TO_KERNEL_DUMP_FAIL,

    CPU_DISPATCH_TO_KERNEL_EXIT,
    CPU_DISPATCH_TO_KERNEL_IO,
    CPU_DISPATCH_TO_KERNEL_INIT_PROC,
    CPU_DISPATCH_TO_KERNEL_DUMP_MEMORY,
    CPU_DISPATCH_TO_KERNEL_PREEMPTION_COMPLETED,

    IO_TO_KERNEL_COMPLETED,

    CPU_TO_MEMORIA_FETCH_INSTRUCTION,
    MEMORIA_TO_CPU_SEND_INSTRUCTION, 
    CPU_TO_MEMORIA_READ,
    MEMORIA_TO_CPU_READ_RESPONSE,
    CPU_TO_MEMORIA_WRITE,
    MEMORIA_TO_CPU_WRITE_ACK,
    KERNEL_TO_CPU_INIT_PROCESS,

    CPU_TO_MEMORIA_GET_PAGE_TABLE_ENTRY,
    MEMORIA_TO_CPU_PAGE_TABLE_ENTRY, // La respuesta de memoria
    GET_MEMORIA_FREE_SPACE
} tOperationCode;

// Estructura del Buffer que hay dentro de cada Paquete:
typedef struct{
    uint32_t size;
    void* stream;
} tBuffer;

// Estructura de un Paquete:
typedef struct{
    tOperationCode operationCode;
    tBuffer *buffer;
} tPackage;

// Estructura de Contexto de Ejecucion:
typedef struct {
    int pid;
    int program_counter;
} tContextoEjecucion;

// Tipos de Instrucción que la CPU puede decodificar y ejecutar
typedef enum {
    NOOP,
    WRITE,
    READ,
    GOTO,
    IO_INST,
    INIT_PROC,
    DUMP_MEMORY,
    EXIT_INST,
    UNKNOWN
} tInstructionType;

// Estructura para una instrucción decodificada
typedef struct {
    tInstructionType operation;
    int total_params;
    char* params[5];
} tDecodedInstruction;

char* getModuleName(tModule module);

tBuffer* createBuffer();
tPackage* createPackage(tOperationCode operationCode);
void addToPackage(tPackage* package, void* thing, uint32_t size);
void sendPackage(tPackage* package, int connectionSocket);
void* serializedPackage(tPackage* package);
void destroyPackage(tPackage* package);
void destroyBuffer(tBuffer* buffer);

tPackage* receivePackage(int connectionSocket);
tBuffer* receiveBuffer(int connectionSocket);
void* receiveStream(uint32_t size, int connectionSocket);

char* extractMessageFromPackage(tPackage* package);
int extractIntFromPackage(tPackage* package);

t_list* packageToList(tPackage* package);
char* extractStringElementFromList(t_list* list, int index);
int extractIntElementFromList(t_list* list, int index);

uint64_t extractUint64ElementFromList(t_list* list, int index);

long long milliseconds();

#endif