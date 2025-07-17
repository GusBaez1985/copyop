#include "kernelLog.h"
#include "kernel.h"

t_log* kernelLog;

void kernelLogCreate(){
    kernelLog = logCreate("KERNEL");
}