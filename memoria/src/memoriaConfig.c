#include "memoriaConfig.h"
#include "memoria.h"

t_config* memoriaConfigFile;
memoriaConfigStruct* memoriaConfig;

void memoriaConfigInitialize(){
    configInitialize(MEMORIA, &memoriaConfigFile, (void*)(&memoriaConfig), memoriaLog);
}

memoriaConfigStruct* getMemoriaConfig(){
    return memoriaConfig;
}