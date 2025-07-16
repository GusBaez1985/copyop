#ifndef GENERAL_CONNECTIONS_H
#define GENERAL_CONNECTIONS_H

void createThreadForConnectingToModule(int connectionSocket, void* (*connectionFunction)(void*));

#endif