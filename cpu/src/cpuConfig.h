#ifndef CPU_CONFIG_H
#define CPU_CONFIG_H

#include <commons/config.h>
#include <commons/log.h>
#include <config.h>

extern t_config* cpuConfigFile;
extern cpuConfigStruct* cpuConfig;

void cpuConfigInitialize();
cpuConfigStruct* getCpuConfig();

#endif