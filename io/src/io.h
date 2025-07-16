#ifndef IO_H
#define IO_H

extern char* ioDevice;

#include <commons/log.h>
#include <commons/config.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#include "ioLog.h"
#include "ioConfig.h"
#include "ioServer.h"
#include "ioClient.h"
#include <server.h>
#include <client.h>
#include <generalConnections.h>
#include <log.h>
#include <utils.h>

#endif