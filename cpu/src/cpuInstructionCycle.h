#ifndef CPU_LOGIC_H
#define CPU_LOGIC_H

#include "utils.h"

void fetch(int, int);
void requestFreeMemoryMock();
tDecodedInstruction* decode(char*);
void execute(tDecodedInstruction*);

#endif