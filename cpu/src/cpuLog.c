#include "cpuLog.h"
#include "cpu.h"

t_log* cpuLog;

// Crea e inicializa el logger
void cpuLogCreate(){
    char* name = string_duplicate("CPU_");
    string_append(&name, string_itoa(cpuId));
    cpuLog = logCreate(name);
    free(name);
}