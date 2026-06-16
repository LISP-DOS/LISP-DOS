#include "heap.h"
#include "pmm.h"
#include "../hal/vga.h"

// Bump Allocator simples

static uint8_t* current_page = NULL;
static size_t current_page_offset = 0;

void heap_init(void) {
    // Inicialização sob demanda no kmalloc
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;
    
    // Alinha para 8 bytes
    size = (size + 7) & ~7;
    
    if (size > 4096) {
        size_t pages = (size + 4095) / 4096;
        void* phys = pmm_alloc_pages(pages);
        if (!phys) return NULL;
        uint8_t* ptr = (uint8_t*)phystokv((uint64_t)phys);
        for (size_t i = 0; i < pages * 4096; i++) ptr[i] = 0;
        return ptr;
    }
    
    if (current_page == NULL || current_page_offset + size > 4096) {
        void* phys = pmm_alloc_page();
        if (phys == NULL) return NULL;
        current_page = (uint8_t*)phystokv((uint64_t)phys);
        current_page_offset = 0;
        
        // Zera a memória (calloc behavior)
        for (int i = 0; i < 4096; i++) current_page[i] = 0;
    }
    
    void* ptr = current_page + current_page_offset;
    current_page_offset += size;
    return ptr;
}

void kfree(void* ptr) {
    // Bump allocator: free não faz nada.
    // Aceitável para o clone inicial até o próximo refactor.
    (void)ptr;
}
