#include "io.h"
#include "ioClient.h"

// Función del handshake inicial entre IO y Kernel.
// IO envía un paquete que solo tiene el código de operación IO_HANDSHAKE,
// Si la memoria lo recibió bien, debe contestar con otro paquete que solo tiene el código
// de operación KERNEL_OK:
int handshakeFromIoToKernel(int connectionSocket){
    tPackage* handshakePackage = createPackage(IO_HANDSHAKE);
    // Agrega el ioDevice al paquete a enviar
    addToPackage(handshakePackage, ioDevice, strlen(ioDevice) + 1);
    log_info(ioLog, "A punto de enviar mensaje de handshake desde IO a kernel.");
    sendPackage(handshakePackage, connectionSocket);
    log_info(ioLog, "Enviado mensaje de handshake desde IO a kernel.");

    tPackage* receivedPackage = receivePackage(connectionSocket);
    log_info(ioLog, "Recibida respuesta desde Kernel al handshake de IO.");
    tOperationCode receivedCode = receivedPackage->operationCode;

    destroyPackage(receivedPackage);

    return receivedCode == KERNEL_OK;
}