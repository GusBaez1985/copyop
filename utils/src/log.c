#include "log.h"
#include <stdlib.h>

// Crea un logger con el nombre del módulo pasado, y lo retorna. También crea un archivo .log con el nombre del módulo:
t_log* logCreate(char* name){
    char* logFileName;

    logFileName = string_duplicate(name);
    string_append(&logFileName, ".log");

    t_log* logger;
    logger = log_create(logFileName, name, true, LOG_LEVEL_INFO);

    if (!logger){
        free(logFileName);
        abort();
    }

    log_info(logger, "%s Logger creado exitosamente.", name);
    free(logFileName);
    return logger;
}

// Destruye el logger:
void logDestroy(t_log* logger){
    log_destroy(logger);
}