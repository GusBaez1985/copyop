#ifndef KERNEL_CONFIG_H
#define KERNEL_CONFIG_H

#include <commons/config.h>
#include <commons/log.h>
#include <config.h>

extern t_config* kernelConfigFile;
extern kernelConfigStruct* kernelConfig;

void kernelConfigInitialize();
kernelConfigStruct* getKernelConfig();

#endif