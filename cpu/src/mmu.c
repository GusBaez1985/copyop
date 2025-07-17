#include "mmu.h"
#include "cpu.h"
#include <math.h> // Para usar floor en el cálculo de la cantidad de tablas

t_queue* tlb;
t_list* tlb_as_list;
t_list* page_cache;



// Inicializa la MMU, seteando la TLB y la Caché de Páginas en el caso de que el archivo config así lo indique.
void mmu_init() {
    // Inicializar TLB si está habilitada
    if (cpuConfig->ENTRADAS_TLB > 0) {
        tlb = queue_create();
        tlb_as_list = tlb->elements;
        log_info(cpuLog, "MMU: TLB habilitada con %d entradas y algoritmo %s.", cpuConfig->ENTRADAS_TLB, cpuConfig->REEMPLAZO_TLB);
    } else {
        tlb = NULL;
        tlb_as_list = NULL;
        log_info(cpuLog, "MMU: TLB deshabilitada.");
    }
    
    // Inicializar Caché de Páginas si está habilitada
    if (cpuConfig->ENTRADAS_CACHE > 0) {
        page_cache = list_create();
        log_info(cpuLog, "MMU: Caché de Páginas habilitada con %d entradas y algoritmo %s.", cpuConfig->ENTRADAS_CACHE, cpuConfig->REEMPLAZO_CACHE);
    } else {
        page_cache = NULL;
        log_info(cpuLog, "MMU: Caché de Páginas deshabilitada.");
    }
}

tMmuStatus mmu_read(uint32_t logicalAddress, uint32_t size, void* buffer_out) {

    uint32_t page_size = tamanioPagina;
    uint32_t bytes_read = 0;

    while (bytes_read < size) {
        
        uint32_t current_logicalAddress = logicalAddress + bytes_read;
        uint32_t page_number = current_logicalAddress / page_size;
        uint32_t offset = current_logicalAddress % page_size;
        uint32_t size_to_read_in_page = page_size - offset;
        
        if (size_to_read_in_page > (size - bytes_read)) {
            size_to_read_in_page = size - bytes_read;
        }

        void* page_content = NULL;
        tPageCache* cache_hit = page_cache_lookup( 1 /* pcb_actual->pid */, page_number); // Revisar cómo obtener el PCB

        if (cache_hit) {

            // Cache Hit: Usar el contenido de la caché
            log_info(cpuLog, "PID: %u - Cache Hit - Página: %u",  1 /* pcb_actual->pid */, page_number);
            page_content = cache_hit->content;

        } else {

            // Cache Miss: Buscar marco y traer página de memoria
            log_info(cpuLog, "PID: %u - Cache Miss - Página: %u",  1 /* pcb_actual->pid */, page_number);
            uint32_t frame_number;
            
            // 1. Traducir dirección (TLB o Memoria)
            if (translate_address( 1 /* pcb_actual->pid */, page_number, &frame_number) != MMU_OK) {
                return MMU_SEG_FAULT;
            }

            // 2. Traer página de Memoria
            if (fetch_page_from_memory( 1 /* pcb_actual->pid */, page_number, frame_number, &page_content) != MMU_OK) {
                 // Si fetch falla, page_content será NULL, pero debemos liberarlo si se alocó memoria.
                 if(page_content != NULL) free(page_content);
                 return MMU_SEG_FAULT; // Suponemos que un error aquí es fatal
            }
            
            // 3. Agregar a la caché
            page_cache_add( 1 /* pcb_actual->pid */, page_number, page_content);
            log_info(cpuLog, "PID: %u - Cache Add - Página: %u",  1 /* pcb_actual->pid */, page_number);
        }

        // Copiar el dato desde el contenido de la página (obtenido de caché o memoria) al buffer de salida
        memcpy(buffer_out + bytes_read, page_content + offset, size_to_read_in_page);
        
        // Si el contenido fue traido de memoria, ya podemos liberarlo, pues ya está en la caché y en el buffer de salida.
        if (!cache_hit) {
            free(page_content);
        }

        bytes_read += size_to_read_in_page;
    }

    return MMU_OK;
}

tMmuStatus mmu_write(uint32_t logicalAddress, uint32_t size, void* buffer_in) {

    uint32_t page_size = tamanioPagina;
    uint32_t bytes_written = 0;
    
    while(bytes_written < size) {
        uint32_t current_logicalAddress = logicalAddress + bytes_written;
        //uint32_t page_number = current_logicalAddress / page_size;
        uint32_t offset = current_logicalAddress % page_size;
        uint32_t size_to_write_in_page = page_size - offset;




        if(size_to_write_in_page > (size - bytes_written)) {
            size_to_write_in_page = size - bytes_written;
        }

        // ! Avisar
        // se modifica por el cambio de firma de translate_address que ahora entrega llave en mano DF, no frame
        
        // uint32_t frame_number;
        // // Para escribir, siempre necesitamos la dirección física.
        // if (translate_address( 1 /* pcb_actual->pid */, page_number, &frame_number) != MMU_OK) {
        //     return MMU_SEG_FAULT;
        // }

        // uint32_t physical_address = frame_number * page_size + offset;

        
        // 1. Declaramos la variable para la dirección física.
        uint32_t physical_address;
        
        // 2. Llamamos a translate_address con la firma correcta:
        //    Le pasamos la dirección LÓGICA completa y esperamos la FÍSICA de vuelta.
        if (translate_address(pid_actual, current_logicalAddress, &physical_address) != MMU_OK) {
            log_error(cpuLog, "PID: %u - SEGMENTATION FAULT al intentar escribir en la dirección lógica %u", pid_actual, current_logicalAddress);
            // TODO: Enviar paquete a Kernel con motivo de SEG_FAULT.
            return MMU_SEG_FAULT;
        }

        // Enviar la escritura a memoria (Write-Through)
        if (memory_write(physical_address, size_to_write_in_page, buffer_in + bytes_written) != 0) { // Definir luego la función en cpuClient.c
            log_error(cpuLog, "PID: %u - Error al escribir en memoria física en la dirección %u",  1 /* pcb_actual->pid */, physical_address);
            return MMU_SEG_FAULT; // Asumimos que un error de escritura es un fallo grave
        }
        
        // Invalidar la entrada en la caché de páginas, ya que su contenido ahora es obsoleto.
        uint32_t page_number = current_logicalAddress / page_size;
        page_cache_invalidate( 1 /* pcb_actual->pid */, page_number);
        
        log_info(cpuLog, "PID: %u - Escritura en Memoria - Dir. Lógica: %u -> Dir. Física: %u, Tamaño: %u",
                 1 /* pcb_actual->pid */, current_logicalAddress, physical_address, size_to_write_in_page);

        bytes_written += size_to_write_in_page;
    }

    return MMU_OK;
}

void tlb_flush() {
    if (!tlb) return;
    
    void invalidate_entry(void* element) {
        ((tTlb*) element)->valid = false;
    }

    list_iterate(tlb_as_list, invalidate_entry);
    log_info(cpuLog, "TLB vaciada (flush).");
}

void page_cache_flush() {
    if (!page_cache) return;

    void cache_entry_destroyer(void* element) {
        tPageCache* entry = (tPageCache*) element;
        free(entry->content);
        free(entry);
    }

    list_clean_and_destroy_elements(page_cache, cache_entry_destroyer);
    log_info(cpuLog, "Caché de páginas vaciada (flush).");
}

void mmu_destroy() {

    if (tlb) {
        queue_destroy_and_destroy_elements(tlb, free);
    }

    if (page_cache) {
        page_cache_flush(); // Reutilizamos la lógica de flush para destruir elementos
        list_destroy(page_cache);
    }

    log_info(cpuLog, "MMU destruida.");
}


// cpu/src/mmu.c
// Necesitarás incluir math.h para la función pow()
#include <math.h> 

// También necesitarás una nueva función en cpuClient.h (la creamos en el siguiente paso)
#include "cpuClient.h"

tMmuStatus translate_address(uint32_t pid, uint32_t logical_address, uint32_t* physical_address) {

    // ! Prueba de escritorio
    log_info(cpuLog, "Iniciando traducción para dirección lógica: %u", logical_address);

    // 1. Calcular nro de página y desplazamiento
    uint32_t page_number = floor(logical_address / tamanioPagina); // menor parte entera
    uint32_t offset = logical_address % tamanioPagina; //resto

    // 2. Buscar en la TLB
    uint32_t frame_number;
    if (tlb_lookup(pid, page_number, &frame_number)) {
        // TLB Hit: Ya tenemos el marco, calculamos la dirección física y terminamos.
        *physical_address = frame_number * tamanioPagina + offset;

        log_info(cpuLog, "Traducción exitosa. Dirección física calculada: %u", *physical_address);
        return MMU_OK;
    }

    // 3. TLB Miss: Recorrer las tablas de páginas pidiendo cada entrada a Memoria.
    log_info(cpuLog, "PID: %u - TLB MISS - Página: %u", pid, page_number);

    // La "dirección" de la tabla de primer nivel es implícitamente el PID.
    // Memoria buscará las tablas de ese proceso. Le pasamos un 0 como placeholder
    // de la dirección de la tabla, ya que es la primera.
    uint32_t current_table_addr = 0; 

    for (int level = 1; level <= cantidadNiveles; level++) {
        // Fórmula del enunciado para calcular el índice de la tabla del nivel actual
        // entrada_nivel_X = floor(nro_página / cant_entradas_tabla^(N-X)) % cant_entradas_tabla
        int divisor = pow(entradasPorTabla, cantidadNiveles - level);
        int entry_index = (int)floor(page_number / divisor) % entradasPorTabla;

        // Pedimos a memoria el contenido de esa entrada
        // Esta es la función que vamos a crear en cpuClient
        int entry_content = memory_get_page_table_entry(pid, current_table_addr, level, entry_index);

        if (entry_content == -1) {
            log_error(cpuLog, "PID: %u - SEG_FAULT - Página: %u no encontrada en tabla de páginas (nivel %d).", pid, page_number, level);
            return MMU_SEG_FAULT;
        }

        if (level == cantidadNiveles) {
            // Es el último nivel, el contenido de la entrada es el número de marco.
            frame_number = entry_content;
        } else {
            // No es el último nivel, el contenido es la "dirección" de la siguiente tabla.
            current_table_addr = entry_content;
        }
    }

    log_info(cpuLog, "PID: %u - Acceso a Tabla de Páginas - Página: %u -> Marco: %u", pid, page_number, frame_number);
    
    // 4. Agregar la nueva traducción (página -> marco) a la TLB
    tlb_add(pid, page_number, frame_number);
    
    // 5. Calcular la dirección física final
    *physical_address = frame_number * tamanioPagina + offset;

    log_info(cpuLog, "Traducción exitosa. Dirección física calculada: %u", *physical_address);

    return MMU_OK;
}


tMmuStatus fetch_page_from_memory(uint32_t pid, uint32_t page_number, uint32_t frame_number, void** content) {
    uint32_t page_size = tamanioPagina;
    uint32_t physical_address = frame_number * page_size;
    
    void* page_buffer = malloc(page_size);
    if(page_buffer == NULL){
        log_error(cpuLog, "PID: %u - No se pudo alocar memoria para el buffer de página.", pid);
        return MMU_SEG_FAULT;
    }

    if (memory_read(physical_address, page_size, page_buffer) != 0) { // Definir luego la función en cpuClient.c
        log_error(cpuLog, "PID: %u - Error al leer el contenido de la página %u desde la dirección física %u", pid, page_number, physical_address);
        free(page_buffer);
        return MMU_SEG_FAULT;
    }
    
    *content = page_buffer;
    return MMU_OK;
}

bool tlb_lookup(uint32_t pid, uint32_t page, uint32_t* frame) {
    if (!tlb || list_is_empty(tlb_as_list)) return false;

    bool find_condition(void* element) {
        tTlb* entry = (tTlb*) element;
        return entry->pid == pid && entry->page == page && entry->valid;
    }

    tTlb* hit = list_find(tlb_as_list, find_condition);

    if (hit) {
        *frame = hit->frame;
        log_info(cpuLog, "PID: %u - TLB HIT - Página: %u -> Marco: %u", pid, page, *frame);
        
        if (strcmp(cpuConfig->REEMPLAZO_TLB, "LRU") == 0) {

            // --- CORRECCIÓN ---
            // Le pasamos directamente el puntero a la función de condición.
            list_remove_element(tlb_as_list, hit); // Asumimos que queremos liberar la entrada vieja si la encontramos
            list_add(tlb_as_list, hit);// Y agregamos la nueva (o la misma, si no liberamos)
            
            
            // --- FIN DE CORRECCIÓN ---

            //!Consultar Supuesta correc  necesita que le pases el puntero a la función de condición, no (void*)find_condition
            // Re-agregamos el elemento al final para marcarlo como el más recientemente usado.
            //list_remove_by_condition(tlb_as_list, (void*)find_condition);
            // list_add(tlb_as_list, hit);
        }
        return true;
    }
    
    log_info(cpuLog, "PID: %u - TLB MISS - Página: %u", pid, page);
    return false;
}

void tlb_add(uint32_t pid, uint32_t page, uint32_t frame) {
    if (!tlb) return;

    tTlb* new_entry;
    
    if (list_size(tlb_as_list) >= cpuConfig->ENTRADAS_TLB) {
        // TLB llena: aplicamos reemplazo (FIFO o LRU, la víctima es la primera de la lista).
        new_entry = list_remove(tlb_as_list, 0); 
        log_info(cpuLog, "TLB Reemplazo: Sale PID: %u, Página: %u", new_entry->pid, new_entry->page);
    } else {
        new_entry = malloc(sizeof(tTlb));
    }

    new_entry->pid = pid;
    new_entry->page = page;
    new_entry->frame = frame;
    new_entry->valid = true;
    list_add(tlb_as_list, new_entry);
    
    log_info(cpuLog, "TLB Add: Entra PID: %u, Página: %u, Marco: %u", pid, page, frame);
}

tPageCache* page_cache_lookup(uint32_t pid, uint32_t page) {
    if (!page_cache) return NULL;

    bool find_condition(void* element) {
        tPageCache* entry = (tPageCache*) element;
        return entry->pid == pid && entry->page == page;
    }
    return list_find(page_cache, find_condition);
}

void page_cache_add(uint32_t pid, uint32_t page, void* content) {
    if (!page_cache) return;

    if (list_size(page_cache) >= cpuConfig->ENTRADAS_CACHE) {
        // Caché llena: Reemplazo FIFO (elimina el más antiguo).
        tPageCache* evicted = list_remove(page_cache, 0);
        log_info(cpuLog, "Caché Reemplazo: Sale PID: %u, Página: %u", evicted->pid, evicted->page);
        free(evicted->content);
        free(evicted);
    }

    tPageCache* new_entry = malloc(sizeof(tPageCache));
    new_entry->pid = pid;
    new_entry->page = page;
    new_entry->content = malloc(tamanioPagina);
    memcpy(new_entry->content, content, tamanioPagina);

    list_add(page_cache, new_entry);
    log_info(cpuLog, "Caché Add: Entra PID: %u, Página: %u", pid, page);
}

void page_cache_invalidate(uint32_t pid, uint32_t page) {
    if (!page_cache) return;

    /*
    bool find_condition(void* element) {
        tPageCache* entry = (tPageCache*) element;
        return entry->pid == pid && entry->page == page;
    }

    void destroyer(void* element) {
        tPageCache* entry = (tPageCache*) element;
        free(entry->content);
        free(entry);
    }
    */
    /* if(list_remove_and_destroy_by_condition(page_cache, find_condition, destroyer)){
        log_info(cpuLog, "Caché Invalidate: PID: %u, Página: %u eliminada por escritura.", pid, page);
    } */
}

int memory_get_frame(int, int, void*) { return 0;}