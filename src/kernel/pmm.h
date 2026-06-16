#ifndef PMM_H
#define PMM_H
#include <stdint.h>
#include <stddef.h>

void pmm_init(void);
void* pmm_alloc_page(void);
void* pmm_alloc_pages(size_t count);
void pmm_free_page(void* page);

extern uint64_t hhdm_offset;

// Converte endereco fisico para virtual (HHDM)
static inline void* phystokv(uint64_t phys) {
    return (void*)(phys + hhdm_offset);
}

#endif
