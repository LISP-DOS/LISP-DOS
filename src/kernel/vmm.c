#include "vmm.h"
#include "pmm.h"
#include "string.h"

extern uint64_t hhdm_offset;

static pagemap_t kernel_pml4 = NULL;

static inline uint64_t read_cr3(void) {
    uint64_t val;
    __asm__ volatile("mov %%cr3, %0" : "=r"(val));
    return val;
}

static inline void write_cr3(uint64_t val) {
    __asm__ volatile("mov %0, %%cr3" : : "r"(val) : "memory");
}

void vmm_init(void) {
    uint64_t cr3 = read_cr3();
    kernel_pml4 = (pagemap_t)(cr3 + hhdm_offset);
}

pagemap_t vmm_new_pagemap(void) {
    pagemap_t pml4_phys = (pagemap_t)pmm_alloc_page();
    if (!pml4_phys) return NULL;
    
    pagemap_t pml4_virt = (pagemap_t)((uint64_t)pml4_phys + hhdm_offset);
    memset(pml4_virt, 0, 4096);
    
    // Copiar metade superior (Kernel)
    for (int i = 256; i < 512; i++) {
        pml4_virt[i] = kernel_pml4[i];
    }
    
    return pml4_phys;
}

void vmm_switch_pagemap(pagemap_t map) {
    write_cr3((uint64_t)map);
}

void vmm_restore_kernel_pagemap(void) {
    // The kernel PML4 physical address is actually what we stored?
    // No, kernel_pml4 is virtual address. We need to convert it back to physical to write to CR3.
    write_cr3((uint64_t)kernel_pml4 - hhdm_offset);
}

void vmm_map_page(pagemap_t map, uint64_t vaddr, uint64_t paddr, uint64_t flags) {
    pagemap_t pml4 = (pagemap_t)((uint64_t)map + hhdm_offset);
    
    uint16_t pml4_idx = (vaddr >> 39) & 0x1FF;
    uint16_t pdpt_idx = (vaddr >> 30) & 0x1FF;
    uint16_t pd_idx   = (vaddr >> 21) & 0x1FF;
    uint16_t pt_idx   = (vaddr >> 12) & 0x1FF;
    
    if (!(pml4[pml4_idx] & PAGE_PRESENT)) {
        uint64_t pdpt_phys = (uint64_t)pmm_alloc_page();
        memset((void*)(pdpt_phys + hhdm_offset), 0, 4096);
        pml4[pml4_idx] = pdpt_phys | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    }
    
    uint64_t* pdpt = (uint64_t*)((pml4[pml4_idx] & ~0xFFF) + hhdm_offset);
    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) {
        uint64_t pd_phys = (uint64_t)pmm_alloc_page();
        memset((void*)(pd_phys + hhdm_offset), 0, 4096);
        pdpt[pdpt_idx] = pd_phys | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    }
    
    uint64_t* pd = (uint64_t*)((pdpt[pdpt_idx] & ~0xFFF) + hhdm_offset);
    if (!(pd[pd_idx] & PAGE_PRESENT)) {
        uint64_t pt_phys = (uint64_t)pmm_alloc_page();
        memset((void*)(pt_phys + hhdm_offset), 0, 4096);
        pd[pd_idx] = pt_phys | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
    }
    
    uint64_t* pt = (uint64_t*)((pd[pd_idx] & ~0xFFF) + hhdm_offset);
    pt[pt_idx] = (paddr & ~0xFFF) | flags | PAGE_PRESENT;
}

void vmm_free_pagemap(pagemap_t map) {
    if (!map) return;
    uint64_t* pml4 = (uint64_t*)((uint64_t)map + hhdm_offset);

    // Liberar apenas a metade inferior (0 - 255) que pertence ao user space
    for (int pml4_idx = 0; pml4_idx < 256; pml4_idx++) {
        if (pml4[pml4_idx] & PAGE_PRESENT) {
            uint64_t* pdpt = (uint64_t*)((pml4[pml4_idx] & ~0xFFF) + hhdm_offset);
            for (int pdpt_idx = 0; pdpt_idx < 512; pdpt_idx++) {
                if (pdpt[pdpt_idx] & PAGE_PRESENT) {
                    uint64_t* pd = (uint64_t*)((pdpt[pdpt_idx] & ~0xFFF) + hhdm_offset);
                    for (int pd_idx = 0; pd_idx < 512; pd_idx++) {
                        if (pd[pd_idx] & PAGE_PRESENT) {
                            uint64_t* pt = (uint64_t*)((pd[pd_idx] & ~0xFFF) + hhdm_offset);
                            for (int pt_idx = 0; pt_idx < 512; pt_idx++) {
                                if (pt[pt_idx] & PAGE_PRESENT) {
                                    pmm_free_page((void*)(pt[pt_idx] & ~0xFFF));
                                }
                            }
                            pmm_free_page((void*)(pd[pd_idx] & ~0xFFF));
                        }
                    }
                    pmm_free_page((void*)(pdpt[pdpt_idx] & ~0xFFF));
                }
            }
            pmm_free_page((void*)(pml4[pml4_idx] & ~0xFFF));
        }
    }
    pmm_free_page((void*)map);
}

int vmm_check_user_ptr(uint64_t vaddr, uint64_t size, int write_required) {
    if (size == 0) return 1;
    if (vaddr >= 0x0000800000000000ULL) return 0;
    if (vaddr + size < vaddr) return 0; // Overflow
    if (vaddr + size >= 0x0000800000000000ULL) return 0;
    
    uint64_t cr3 = read_cr3();
    uint64_t* pml4 = (uint64_t*)(cr3 + hhdm_offset);
    
    uint64_t start_page = vaddr & ~0xFFFULL;
    uint64_t end_page = (vaddr + size - 1) & ~0xFFFULL;
    
    for (uint64_t page = start_page; page <= end_page; page += 4096) {
        uint16_t pml4_idx = (page >> 39) & 0x1FF;
        uint16_t pdpt_idx = (page >> 30) & 0x1FF;
        uint16_t pd_idx   = (page >> 21) & 0x1FF;
        uint16_t pt_idx   = (page >> 12) & 0x1FF;
        
        if (!(pml4[pml4_idx] & PAGE_PRESENT)) return 0;
        if (!(pml4[pml4_idx] & PAGE_USER)) return 0;
        
        uint64_t* pdpt = (uint64_t*)((pml4[pml4_idx] & ~0xFFF) + hhdm_offset);
        if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) return 0;
        if (!(pdpt[pdpt_idx] & PAGE_USER)) return 0;
        
        uint64_t* pd = (uint64_t*)((pdpt[pdpt_idx] & ~0xFFF) + hhdm_offset);
        if (!(pd[pd_idx] & PAGE_PRESENT)) return 0;
        if (!(pd[pd_idx] & PAGE_USER)) return 0;
        
        uint64_t* pt = (uint64_t*)((pd[pd_idx] & ~0xFFF) + hhdm_offset);
        if (!(pt[pt_idx] & PAGE_PRESENT)) return 0;
        if (!(pt[pt_idx] & PAGE_USER)) return 0;
        if (write_required && !(pt[pt_idx] & PAGE_WRITE)) return 0;
    }
    return 1;
}

int vmm_check_user_string(uint64_t vaddr) {
    if (vaddr >= 0x0000800000000000ULL) return 0;
    uint64_t cr3 = read_cr3();
    uint64_t* pml4 = (uint64_t*)(cr3 + hhdm_offset);
    
    uint64_t current = vaddr;
    while (1) {
        if (current >= 0x0000800000000000ULL) return 0;
        
        uint64_t page = current & ~0xFFFULL;
        uint16_t pml4_idx = (page >> 39) & 0x1FF;
        uint16_t pdpt_idx = (page >> 30) & 0x1FF;
        uint16_t pd_idx   = (page >> 21) & 0x1FF;
        uint16_t pt_idx   = (page >> 12) & 0x1FF;
        
        if (!(pml4[pml4_idx] & PAGE_PRESENT)) return 0;
        if (!(pml4[pml4_idx] & PAGE_USER)) return 0;
        uint64_t* pdpt = (uint64_t*)((pml4[pml4_idx] & ~0xFFF) + hhdm_offset);
        if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) return 0;
        if (!(pdpt[pdpt_idx] & PAGE_USER)) return 0;
        uint64_t* pd = (uint64_t*)((pdpt[pdpt_idx] & ~0xFFF) + hhdm_offset);
        if (!(pd[pd_idx] & PAGE_PRESENT)) return 0;
        if (!(pd[pd_idx] & PAGE_USER)) return 0;
        uint64_t* pt = (uint64_t*)((pd[pd_idx] & ~0xFFF) + hhdm_offset);
        if (!(pt[pt_idx] & PAGE_PRESENT)) return 0;
        if (!(pt[pt_idx] & PAGE_USER)) return 0;
        
        char* ptr = (char*)current;
        uint64_t limit = (page + 4096) - current;
        for (uint64_t i = 0; i < limit; i++) {
            if (ptr[i] == '\0') return 1;
        }
        current = page + 4096;
    }
}
