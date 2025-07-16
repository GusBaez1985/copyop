#ifndef MEMORIA_SERVER_H
#define MEMORIA_SERVER_H




#include <commons/bitarray.h>           // para t_bitarray y bitarray_*
#include <commons/collections/dictionary.h> // para t_dictionary



void* establishingMemoriaConnection(void* voidPointerConnectionSocket);

void* serverThreadForCpuDispatch(void* voidPointerConnectionSocket);

void* serverThreadForCpuInterrupt(void* voidPointerConnectionSocket);

void* serverThreadForKernel(void* voidPointerConnectionSocket);

void initMemory(void);



typedef struct {
    int pid;                // ID del proceso 
    int numPages;           // cuántas páginas (marcos) reservó
    int* frames;                // (solo si mantuvieras el mono-nivel)
    //  paginación multinivel 
    int   levels;            // CANTIDAD_NIVELES
    int   entriesPerTable;   // ENTRADAS_POR_TABLA
    void** pageTables;       // punteros a cada nivel: array de longitud 'levels'
    // pseudocódigo 
    char** instructions;
    int     instructionCount;

    // … otros campos (métricas, etc.) …
} t_memoriaProcess;



















t_memoriaProcess* createProcess(int pid, int sizeBytes, const char* pseudocodeFileName);

int translateAddress(t_memoriaProcess* proc, int dl);



extern int finishServer;
extern int listeningSocket;

#define MOCK_FREE_MEMORY 1024 // ! luego del mock borrar!

extern void*          memory;           // bloque de RAM simulada
extern t_bitarray*    frameBitmap;      // bitmap de marcos
extern t_dictionary* activeProcesses;  // contendra los procesos activos, al ser un diccionario puedo buscar por pid 



#endif