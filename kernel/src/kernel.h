#ifndef KERNEL_H
#define KERNEL_H

#include <commons/log.h>
#include <commons/config.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#include "kernelLog.h"
#include "kernelConfig.h"
#include "kernelServer.h"
#include "kernelClient.h"
#include "generalScheduling.h"
#include "longTermScheduling.h"
#include "shortTermScheduling.h"
#include "interfaces.h"
#include <server.h>
#include <client.h>
#include <generalConnections.h>
#include <log.h>
#include <utils.h>
#include <readline/readline.h>

#endif