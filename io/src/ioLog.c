#include "ioLog.h"
#include "io.h"

t_log* ioLog;

void ioLogCreate(){
    char* name = string_duplicate("IO_");
    string_append(&name, ioDevice);
    ioLog = logCreate(name);
    free(name);
}