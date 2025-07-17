#include "kernelConfig.h"
#include "kernel.h"

t_config* kernelConfigFile;
kernelConfigStruct* kernelConfig;

void kernelConfigInitialize(){
    configInitialize(KERNEL, &kernelConfigFile, (void*)(&kernelConfig), kernelLog);
}

kernelConfigStruct* getKernelConfig(){
    return kernelConfig;
}