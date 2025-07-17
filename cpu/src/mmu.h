#ifndef CPU_MMU_H
#define CPU_MMU_H

#include <commons/collections/list.h>
#include <commons/collections/queue.h>
#include <stdint.h>

typedef struct {
    int page_number;
    int frame_number;
    bool presence;
    bool use; // para reemplazo (ej: LRU)
} tPageEntry;

typedef struct {
    int pid;
    t_list* page_table; // list of tPageEntry*
} tProcessMemory;

typedef struct {
    uint32_t pid;
    uint32_t page;
    uint32_t frame;
    bool valid;
} tTlb;

typedef struct {
    uint32_t pid;
    uint32_t page;
    void* content;
} tPageCache;

typedef struct {
    uint32_t physical_address;
    uint32_t size;
} tPhysicalAddress;

typedef enum {
    MMU_OK,
    MMU_SEG_FAULT // Ocurrió un fallo de segmentación
} tMmuStatus;

extern uint32_t page_size;

// TLB
extern t_queue* tlb;
extern t_list* tlb_as_list;

// Caché de Páginas
extern t_list* page_cache;

void mmu_init();
tMmuStatus mmu_read(uint32_t, uint32_t, void*);
tMmuStatus mmu_write(uint32_t, uint32_t, void*);

// Funciones de Traducción y Comunicación
tMmuStatus translate_address(uint32_t, uint32_t, uint32_t*);
tMmuStatus fetch_page_from_memory(uint32_t, uint32_t, uint32_t, void**);

// Funciones de TLB
bool tlb_lookup(uint32_t, uint32_t, uint32_t*);
void tlb_add(uint32_t, uint32_t, uint32_t);

// Funciones de Caché de Páginas
tPageCache* page_cache_lookup(uint32_t, uint32_t);
void page_cache_add(uint32_t, uint32_t, void*);
void page_cache_invalidate(uint32_t, uint32_t);

void tlb_flush();

void page_cache_flush();
void mmu_init(void);
void mmu_destroy();

#endif