#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_PRESENT (1 << 0)
#define PAGE_WRITE   (1 << 1)
#define PAGE_USER    (1 << 2)

typedef uint64_t* pagemap_t;

void vmm_init(void);
pagemap_t vmm_new_pagemap(void);
void vmm_switch_pagemap(pagemap_t map);
void vmm_restore_kernel_pagemap(void);
void vmm_map_page(pagemap_t map, uint64_t vaddr, uint64_t paddr, uint64_t flags);
void vmm_free_pagemap(pagemap_t map);
int vmm_check_user_ptr(uint64_t vaddr, uint64_t size, int write_required);
int vmm_check_user_string(uint64_t vaddr);

#endif
