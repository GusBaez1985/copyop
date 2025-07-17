#include "cpuConfig.h"
#include "cpu.h"

t_config* cpuConfigFile;
cpuConfigStruct* cpuConfig;

// Crea e inicializa el config
void cpuConfigInitialize(){
    configInitialize(CPU, &cpuConfigFile, (void*)(&cpuConfig), cpuLog);
}

cpuConfigStruct* getCpuConfig(){
    return cpuConfig;
}