#include "ioConfig.h"
#include "io.h"

t_config* ioConfigFile;
ioConfigStruct* ioConfig;

void ioConfigInitialize(){
    configInitialize(IO, &ioConfigFile, (void*)(&ioConfig), ioLog);
}

ioConfigStruct* getIoConfig(){
    return ioConfig;
}