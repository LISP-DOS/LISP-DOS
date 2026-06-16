#include "pmm.h"
#include <limine.h>
#include "../hal/vga.h"

__attribute__((used, section(".requests")))
volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

__attribute__((used, section(".requests")))
volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0
};

#define PAGE_SIZE 4096

uint64_t hhdm_offset = 0;

static uint8_t* pmm_bitmap = NULL;
static uint64_t pmm_free_memory = 0;
static uint64_t pmm_highest_page = 0;
static uint64_t pmm_last_allocated_index = 0;

static inline void bitmap_set(uint64_t bit) {
    pmm_bitmap[bit / 8] |= (1 << (bit % 8));
}

static inline void bitmap_clear(uint64_t bit) {
    pmm_bitmap[bit / 8] &= ~(1 << (bit % 8));
}

static inline int bitmap_test(uint64_t bit) {
    return (pmm_bitmap[bit / 8] & (1 << (bit % 8))) != 0;
}

void pmm_init(void) {
    if (hhdm_request.response != NULL) {
        hhdm_offset = hhdm_request.response->offset;
    } else {
        vga_print("Erro: Limine nao forneceu o HHDM.\n");
        for (;;) __asm__("hlt");
    }

    struct limine_memmap_response *mmap = memmap_request.response;
    if (mmap == NULL) {
        vga_print("Erro: Limine nao forneceu o Mapa de Memoria.\n");
        for (;;) __asm__("hlt");
    }

    uint64_t highest_address = 0;

    for (uint64_t i = 0; i < mmap->entry_count; i++) {
        struct limine_memmap_entry *entry = mmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            uint64_t top = entry->base + entry->length;
            if (top > highest_address) highest_address = top;
        }
    }

    pmm_highest_page = highest_address / PAGE_SIZE;
    uint64_t bitmap_size = pmm_highest_page / 8;
    if (bitmap_size == 0) bitmap_size = 1;

    // Encontra lugar para o bitmap
    for (uint64_t i = 0; i < mmap->entry_count; i++) {
        struct limine_memmap_entry *entry = mmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE && entry->length >= bitmap_size) {
            pmm_bitmap = (uint8_t*)phystokv(entry->base);
            entry->base += bitmap_size;
            entry->length -= bitmap_size;
            break;
        }
    }

    for (uint64_t i = 0; i < bitmap_size; i++) {
        pmm_bitmap[i] = 0xFF; // Marca tudo como ocupado
    }

    for (uint64_t i = 0; i < mmap->entry_count; i++) {
        struct limine_memmap_entry *entry = mmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            for (uint64_t j = 0; j < entry->length; j += PAGE_SIZE) {
                bitmap_clear((entry->base + j) / PAGE_SIZE);
                pmm_free_memory += PAGE_SIZE;
            }
        }
    }
}

void* pmm_alloc_page(void) {
    for (uint64_t i = pmm_last_allocated_index; i < pmm_highest_page; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            pmm_last_allocated_index = i;
            pmm_free_memory -= PAGE_SIZE;
            return (void*)(i * PAGE_SIZE);
        }
    }

    for (uint64_t i = 0; i < pmm_last_allocated_index; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            pmm_last_allocated_index = i;
            pmm_free_memory -= PAGE_SIZE;
            return (void*)(i * PAGE_SIZE);
        }
    }

    return NULL;
}

void* pmm_alloc_pages(size_t count) {
    if (count == 0) return NULL;
    if (count == 1) return pmm_alloc_page();

    uint64_t contiguous = 0;
    uint64_t start_index = pmm_last_allocated_index;

    // Procura por `count` paginas contiguas a partir do last_allocated_index
    for (uint64_t i = pmm_last_allocated_index; i < pmm_highest_page; i++) {
        if (!bitmap_test(i)) {
            if (contiguous == 0) start_index = i;
            contiguous++;
            if (contiguous == count) {
                for (uint64_t j = 0; j < count; j++) bitmap_set(start_index + j);
                pmm_last_allocated_index = start_index + count;
                pmm_free_memory -= PAGE_SIZE * count;
                return (void*)(start_index * PAGE_SIZE);
            }
        } else {
            contiguous = 0;
        }
    }

    // Procura desde o inicio se nao achou
    contiguous = 0;
    for (uint64_t i = 0; i < pmm_last_allocated_index; i++) {
        if (!bitmap_test(i)) {
            if (contiguous == 0) start_index = i;
            contiguous++;
            if (contiguous == count) {
                for (uint64_t j = 0; j < count; j++) bitmap_set(start_index + j);
                pmm_last_allocated_index = start_index + count;
                pmm_free_memory -= PAGE_SIZE * count;
                return (void*)(start_index * PAGE_SIZE);
            }
        } else {
            contiguous = 0;
        }
    }

    return NULL;
}

void pmm_free_page(void* page) {
    uint64_t addr = (uint64_t)page;
    uint64_t bit = addr / PAGE_SIZE;
    if (bitmap_test(bit)) {
        bitmap_clear(bit);
        pmm_free_memory += PAGE_SIZE;
    }
}
