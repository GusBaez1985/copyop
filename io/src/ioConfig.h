#ifndef IO_CONFIG_H
#define IO_CONFIG_H

#include <commons/config.h>
#include <commons/log.h>
#include <config.h>

extern t_config* ioConfigFile;
extern ioConfigStruct* ioConfig;

void ioConfigInitialize();
ioConfigStruct* getIoConfig();

#endif