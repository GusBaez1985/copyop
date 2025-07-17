#ifndef CPU_H
#define CPU_H

#include <commons/log.h>
#include <commons/config.h>
#include <readline/readline.h>
#include <semaphore.h> 
#include <stdbool.h>   

#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>

#include "cpuLog.h"
#include "cpuConfig.h"
#include "cpuServer.h"
#include "cpuClient.h"
#include "cpuInstructionCycle.h"

#include <log.h>
#include <utils.h>
#include <server.h>
#include <client.h>
#include <generalConnections.h>

extern int cpuId;
extern uint32_t tamanioPagina;
extern uint32_t entradasPorTabla;
extern uint32_t cantidadNiveles;
extern uint32_t pid_actual;
extern uint32_t last_read_size;





extern uint32_t pc_actual;
extern bool ciclo_de_instruccion_activo;
extern sem_t sem_ciclo_instruccion;

#endif