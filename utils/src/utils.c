#include "utils.h"
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>

// Es una función auxiliar que retorna una cadena de caracteres con el nombre del módulo:
char* getModuleName(tModule module){
    switch(module){
        case KERNEL:
            return string_from_format("KERNEL");
        case CPU:
            return string_from_format("CPU");
        case MEMORIA:
            return string_from_format("MEMORIA");
        case IO:
            return string_from_format("IO");
        default:
            abort();
    }
}

// Crea un buffer con un tamaño de 0 y un stream (puntero a void) NULL, retorna el buffer creado:
tBuffer* createBuffer(){
    tBuffer* buffer = malloc(sizeof(tBuffer));
    buffer->size = 0;
    buffer->stream = NULL;
    return buffer;
}

// Crea un paquete (que se usa para enviar o recibir información), el paquete tendrá el código de operación
// pasado como parámetro a esta función, y se crea también un buffer vacío. Se retorna el paquete creado:
tPackage* createPackage(tOperationCode operationCode){
    tPackage* package = malloc(sizeof(tPackage));
    package->operationCode = operationCode;
    package->buffer = createBuffer();
    return package;
}

// Agrega un contenido al paquete pasado como parámetro, el contenido debe ser un puntero,
// y se debe pasar también el tamaño de memoria que reserva ese puntero:
void addToPackage(tPackage *package, void* thing, uint32_t size){
    void* newStream = realloc(package->buffer->stream, package->buffer->size + size + sizeof(uint32_t));

    if (!newStream)
        abort();

    package->buffer->stream = newStream;

    memcpy(package->buffer->stream + package->buffer->size, &size, sizeof(uint32_t));
    memcpy(package->buffer->stream + package->buffer->size + sizeof(uint32_t), thing, size);

    package->buffer->size += (sizeof(uint32_t) + size);
}

// Envía el paquete utilizando el socket de conexión pasado como parámetro.
// LA FUNCIÓN AUTOMÁTICAMENTE DESTRUYE EL PAQUETE, ESTO SE PUEDE CAMBIAR SI RESULTA INCONVENIENTE:
void sendPackage(tPackage *package, int connectionSocket){
    void* finalStream = serializedPackage(package);
    
    send(connectionSocket, finalStream, package->buffer->size + 2 * sizeof(uint32_t), 0);

    free(finalStream);
    destroyPackage(package);
}

// Retorna un puntero un stream o flujo (es un puntero a void), con toda la informacion del paquete serializada.
// Primero estará el código de operación, luego el tamaño de memoria que ocupa el stream, y luego el stream mismo:
void* serializedPackage(tPackage *package){
    void* stream = malloc(package->buffer->size + 2 * sizeof(uint32_t));

    memcpy(stream, &(package->operationCode), sizeof(uint32_t));
    memcpy(stream + sizeof(uint32_t), &(package->buffer->size), sizeof(uint32_t));
    memcpy(stream + 2 * sizeof(uint32_t), package->buffer->stream, package->buffer->size);

    return stream;
}

// Destruye el paquete, y a su vez llama a destruir el buffer interno:
void destroyPackage(tPackage *package){
    if (package){
        destroyBuffer(package->buffer);
        free(package);
    }
}

// Destruye el buffer:
void destroyBuffer(tBuffer *buffer){
    if (buffer){
        if (buffer->stream)
            free(buffer->stream);
        free(buffer);
    }
}

// Recibe un paquete a través de un socket de conexión, y lo retorna.
// Es una función bloqueante ya que posee un recv dentro:
tPackage* receivePackage(int connectionSocket){
    tOperationCode operationCode;
    ssize_t bytes_received = recv(connectionSocket, &operationCode, sizeof(uint32_t), MSG_WAITALL);
    if (bytes_received <= 0) {
        if (bytes_received == -1) {
            perror("Error en recv");
        } else {
            printf("Conexión cerrada por el otro extremo (recv devolvió 0)\n");
        }
        return NULL;
    }

    tPackage* package = createPackage(operationCode);
    destroyBuffer(package->buffer);

    package->buffer = receiveBuffer(connectionSocket);
    if (!package->buffer)
        return NULL;

    return package;
}

// Recibe un buffer a través de un socket de conexión, y lo retorna.
// Es una función bloqueante ya que posee un recv dentro.
// Solo la llama receivePackage, es parte de su lógica interna, no debería ser necesario usarla aparte:
tBuffer* receiveBuffer(int connectionSocket){
    uint32_t size;
    ssize_t bytes_received = recv(connectionSocket, &size, sizeof(uint32_t), MSG_WAITALL);
    if (bytes_received <= 0) {
        if (bytes_received == -1)
            perror("Error en recv");
        else
            printf("Conexión cerrada por el otro extremo (recv devolvió 0)\n");
        return NULL;
    }

    tBuffer* buffer = createBuffer();
    buffer->size = size;
    if (buffer->size > 0){
        buffer->stream = receiveStream(buffer->size, connectionSocket);
        if (!buffer->stream){
            destroyBuffer(buffer);
            return NULL;
        }
    }

    return buffer;
}

// Recibe un stream (puntero a void) a través de un socket de conexión, y lo retorna.
// Es una función bloqueante ya que posee un recv dentro.
// Solo la llama receiveBuffer, es parte de su lógica interna, no debería ser necesario usarla aparte:
void* receiveStream(uint32_t size, int connectionSocket){
    void* stream = malloc(size);

    ssize_t bytes_received = recv(connectionSocket, stream, size, MSG_WAITALL);
    if (bytes_received <= 0) {
        if (bytes_received == -1)
            perror("Error en recv");
        else
            printf("Conexión cerrada por el otro extremo (recv devolvió 0)\n");
        return NULL;
    }

    return stream;
}

// Extrae un mensaje de texto (puntero a char) de un paquete, y retorna ese puntero a char.
// Asume que el paquete contiene un mensaje, si el paquete contuviera otra cosa, daría algún error:
char* extractMessageFromPackage(tPackage* package){
    uint32_t size;
    char* message = NULL;

    memcpy(&size, package->buffer->stream, sizeof(uint32_t));
    message = malloc(size);
    memcpy(message, package->buffer->stream + sizeof(uint32_t), size);
    
    return message;
}

// Extrae un entero (int) desde un paquete recibido.
// El paquete contiene los datos empaquetados en este formato:
// [ tamaño (uint32_t) | dato real (en este caso un int) ]
// La función salta los primeros 4 bytes (tamaño) y copia los siguientes 4 bytes
// (el entero real) a la variable 'value', y lo retorna.
// Esta función asume que el paquete fue armado con addToPackage() y contiene un único int.
int extractIntFromPackage(tPackage* package) {
    int value;
    memcpy(&value, package->buffer->stream + sizeof(uint32_t), sizeof(int));
    return value;
}

// Esta función crea una lista que contendrá tantos elementos como había en el paquete
// Cada elemento es un puntero a void, lo que hay que tener en cuenta cuando se obtiene un elemento de dicha lista
// Devuelve esa lista, la cual después hay que encargarse de destruir, y destruir sus elementos
t_list* packageToList(tPackage* package){
	uint32_t offset = 0;
	t_list* list = list_create();
	uint32_t size;

	while(offset < package->buffer->size){
		memcpy(&size, package->buffer->stream + offset, sizeof(uint32_t));
		offset += sizeof(uint32_t);
		void* element = malloc(size);
		memcpy(element, package->buffer->stream + offset, size);
		offset += size;
		list_add(list, element);
	}

	return list;
}

// Esta función toma una lista, y un índice de la lista, y asume que en ese índice hay un mensaje (un puntero a char)
// Devuelve ese string
char* extractStringElementFromList(t_list* list, int index){
    return (char*)list_get(list, index);
}

// Esta función toma una lista, y un índice de la lista, y asume que en ese índice hay un número entero (un int)
// Devuelve ese int
int extractIntElementFromList(t_list* list, int index){
    return *((int*)list_get(list, index));
}


uint64_t extractUint64ElementFromList(t_list* list, int index){
    return *((uint64_t*)list_get(list, index));
}


long long milliseconds(){
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}