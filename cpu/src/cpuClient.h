#ifndef CPU_CLIENT_H
#define CPU_CLIENT_H

int handshakeFromDispatchToKernel(int connectionSocket);

int handshakeFromInterruptToKernel(int connectionSocket);

int handshakeFromDispatchToMemoria(int connectionSocket);

int handshakeFromInterruptToMemoria(int connectionSocket);

// cpu/src/cpuClient.h
int memory_get_page_table_entry(uint32_t pid, uint32_t table_addr, int level, int entry_index);

int memory_read(uint32_t physical_address, uint32_t size, void* buffer_out);
int memory_write(uint32_t physical_address, uint32_t size, void* buffer_in);

#endif