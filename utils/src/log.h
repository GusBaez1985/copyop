#ifndef LOG_H
#define LOG_H

#include "utils.h"

t_log* logCreate(char* name);
void logDestroy(t_log* logger);

#endif