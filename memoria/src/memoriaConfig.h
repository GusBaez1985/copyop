#ifndef MEMORIA_CONFIG_H
#define MEMORIA_CONFIG_H

#include <commons/config.h>
#include <commons/log.h>
#include <config.h>

extern t_config* memoriaConfigFile;
extern memoriaConfigStruct* memoriaConfig;

void memoriaConfigInitialize();
memoriaConfigStruct* getMemoriaConfig();

#endif