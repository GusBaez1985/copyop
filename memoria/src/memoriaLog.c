#include "memoriaLog.h"
#include "memoria.h"

t_log* memoriaLog;

void memoriaLogCreate(){
    memoriaLog = logCreate("MEMORIA");
}