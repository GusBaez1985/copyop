#include "config.h"

// Función inicial para inicializar un config, llama a otras dos funciones:
void configInitialize(tModule module, t_config** configFile, void** configStruct, t_log* logger){
    configCreate(module, configFile, logger);
    configGetData(module, (*configFile), configStruct, logger);
}

// Es la función que crea el config, guiándose por el nombre del módulo pasado.
// Debe estar creado ya el archivo físico .config:
void configCreate(tModule module, t_config** configFile, t_log* logger){
    char* moduleName;
    char* configFileName;

    moduleName = getModuleName(module);

    configFileName = string_duplicate(moduleName);
    string_append(&configFileName, ".config");

    (*configFile) = config_create(configFileName);

    if (!(*configFile)){
        log_info(logger, "No se pudo crear el archivo config de %s. Abortando Programa.", moduleName);
        free(moduleName);
        free(configFileName);
        abort();
    }

    log_info(logger, "%s Config creado exitosamente.", moduleName);
    free(moduleName);
    free(configFileName);
}

// Dependiendo del módulo pasado como parámetro (con el enumerado tModule), crea una estructura para el módulo que sea,
// y carga los campos de esa estructura con los datos que hay en el archivo .config real:
void configGetData(tModule module, t_config* configFile, void** configStruct, t_log* logger){
    switch(module){
        case KERNEL:
            kernelConfigStruct* kernelConfig = malloc(sizeof(kernelConfigStruct));
            kernelConfig->IP_MEMORIA = config_get_string_value(configFile, "IP_MEMORIA");
            kernelConfig->PUERTO_MEMORIA = config_get_string_value(configFile, "PUERTO_MEMORIA");
            kernelConfig->PUERTO_ESCUCHA_DISPATCH = config_get_string_value(configFile, "PUERTO_ESCUCHA_DISPATCH");
            kernelConfig->PUERTO_ESCUCHA_INTERRUPT = config_get_string_value(configFile, "PUERTO_ESCUCHA_INTERRUPT");
            kernelConfig->PUERTO_ESCUCHA_IO = config_get_string_value(configFile, "PUERTO_ESCUCHA_IO");
            kernelConfig->ALGORITMO_CORTO_PLAZO = config_get_string_value(configFile, "ALGORITMO_CORTO_PLAZO");
            kernelConfig->ALGORITMO_INGRESO_A_READY = config_get_string_value(configFile, "ALGORITMO_INGRESO_A_READY");
            kernelConfig->ALFA = config_get_double_value(configFile, "ALFA");
            kernelConfig->ESTIMACION_INICIAL = config_get_int_value(configFile, "ESTIMACION_INICIAL");
            kernelConfig->TIEMPO_SUSPENSION = config_get_int_value(configFile, "TIEMPO_SUSPENSION");
            kernelConfig->LOG_LEVEL = config_get_string_value(configFile, "LOG_LEVEL");
            (*configStruct) = kernelConfig;
            break;
        case CPU:
            cpuConfigStruct* cpuConfig = malloc(sizeof(cpuConfigStruct));
            cpuConfig->IP_MEMORIA = config_get_string_value(configFile, "IP_MEMORIA");
            cpuConfig->PUERTO_MEMORIA = config_get_string_value(configFile, "PUERTO_MEMORIA");
            cpuConfig->IP_KERNEL = config_get_string_value(configFile, "IP_KERNEL");
            cpuConfig->PUERTO_KERNEL_DISPATCH = config_get_string_value(configFile, "PUERTO_KERNEL_DISPATCH");
            cpuConfig->PUERTO_KERNEL_INTERRUPT = config_get_string_value(configFile, "PUERTO_KERNEL_INTERRUPT");
            cpuConfig->ENTRADAS_TLB = config_get_int_value(configFile, "ENTRADAS_TLB");
            cpuConfig->REEMPLAZO_TLB = config_get_string_value(configFile, "REEMPLAZO_TLB");
            cpuConfig->ENTRADAS_CACHE = config_get_int_value(configFile, "ENTRADAS_CACHE");
            cpuConfig->REEMPLAZO_CACHE = config_get_string_value(configFile, "REEMPLAZO_CACHE");
            cpuConfig->RETARDO_CACHE = config_get_int_value(configFile, "RETARDO_CACHE");
            cpuConfig->LOG_LEVEL = config_get_string_value(configFile, "LOG_LEVEL");
            (*configStruct) = cpuConfig;
            break;
        case MEMORIA:
            memoriaConfigStruct* memoriaConfig = malloc(sizeof(memoriaConfigStruct));
            memoriaConfig->PUERTO_ESCUCHA = config_get_string_value(configFile, "PUERTO_ESCUCHA");
            memoriaConfig->TAM_MEMORIA = config_get_int_value(configFile, "TAM_MEMORIA");
            memoriaConfig->TAM_PAGINA = config_get_int_value(configFile, "TAM_PAGINA");
            memoriaConfig->ENTRADAS_POR_TABLA = config_get_int_value(configFile, "ENTRADAS_POR_TABLA");
            memoriaConfig->CANTIDAD_NIVELES = config_get_int_value(configFile, "CANTIDAD_NIVELES");
            memoriaConfig->RETARDO_MEMORIA = config_get_int_value(configFile, "RETARDO_MEMORIA");
            memoriaConfig->PATH_SWAPFILE = config_get_string_value(configFile, "PATH_SWAPFILE");
            memoriaConfig->RETARDO_SWAP = config_get_int_value(configFile, "RETARDO_SWAP");
            memoriaConfig->LOG_LEVEL = config_get_string_value(configFile, "LOG_LEVEL");
            memoriaConfig->DUMP_PATH = config_get_string_value(configFile, "DUMP_PATH");
            memoriaConfig->PATH_INSTRUCCIONES = config_get_string_value(configFile, "PATH_INSTRUCCIONES");
            (*configStruct) = memoriaConfig;
            break;
        case IO:
            ioConfigStruct* ioConfig = malloc(sizeof(ioConfigStruct));
            ioConfig->IP_KERNEL = config_get_string_value(configFile, "IP_KERNEL");
            ioConfig->PUERTO_KERNEL = config_get_string_value(configFile, "PUERTO_KERNEL");
            ioConfig->LOG_LEVEL = config_get_string_value(configFile, "LOG_LEVEL");
            (*configStruct) = ioConfig;
            break;
        default:
            break;
    }
    
    log_info(logger, "Datos obtenidos del config exitosamente.");
}

// Destruye el config:
void configDestroy(t_config* configFile, void* configStruct){
    if (configStruct)
        free(configStruct);
    if (configFile)
        config_destroy(configFile);
}