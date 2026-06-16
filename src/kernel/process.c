#include "fat16.h"
#include "../hal/vga.h"
#include <stdint.h>
#include <stddef.h>
#include "elf.h"
#include "vmm.h"
#include "pmm.h"
#include "string.h"
#include "heap.h"

extern void tss_set_kernel_stack(uint64_t stack);
extern uint64_t hhdm_offset;

uint64_t current_user_rsp = 0;
uint64_t current_kernel_stack = 0;

extern void asm_jump_usermode(uint64_t entry_point, uint64_t user_stack, uint64_t args_string);
extern void asm_return_to_kernel(void);

void jump_usermode(uint64_t entry_point, uint64_t user_stack, uint64_t args_string) {
    static uint8_t kstack[4096];
    tss_set_kernel_stack((uint64_t)kstack + 4096);
    current_kernel_stack = (uint64_t)kstack + 4096;
    
    asm_jump_usermode(entry_point, user_stack, args_string);
}

void process_exec(const char *filename, const char *args) {
    uint32_t file_size = fat16_get_file_size(filename);
    int use_tarfs = 0;
    if (file_size == 0) {
        extern uint64_t tarfs_get_file_size(const char*);
        file_size = tarfs_get_file_size(filename);
        if (file_size == 0) {
            vga_print("Exec: Arquivo nao encontrado ou vazio!\n");
            return;
        }
        use_tarfs = 1;
    }
    
    uint64_t file_pages = (file_size + 4095) / 4096;
    void* phys_buf = pmm_alloc_pages(file_pages);
    if (!phys_buf) {
        vga_print("Exec: Memoria insuficiente para ler arquivo!\n");
        return;
    }
    uint8_t* file_buf = (uint8_t*)phystokv((uint64_t)phys_buf);
    
    int bytes_read = 0;
    if (use_tarfs) {
        extern int tarfs_read_file(const char*, uint8_t*);
        bytes_read = tarfs_read_file(filename, file_buf);
    } else {
        bytes_read = fat16_read_file(filename, file_buf);
    }
    
    if (bytes_read <= 0) {
        for(uint64_t i=0; i<file_pages; i++) pmm_free_page((void*)((uint64_t)phys_buf + i*4096));
        return;
    }
    
    Elf64_Ehdr* ehdr = (Elf64_Ehdr*)file_buf;
    if (ehdr->e_ident[0] != 0x7f || ehdr->e_ident[1] != 'E' || 
        ehdr->e_ident[2] != 'L' || ehdr->e_ident[3] != 'F') {
        vga_print("Exec: Formato ELF invalido!\n");
        for(uint64_t i=0; i<file_pages; i++) pmm_free_page((void*)((uint64_t)phys_buf + i*4096));
        return;
    }
    
    if (ehdr->e_phoff >= file_size || ehdr->e_phoff + (ehdr->e_phnum * sizeof(Elf64_Phdr)) > file_size) {
        vga_print("Exec: Limites do ELF Phdr invalidos (Risco de Seguranca)!\n");
        for(uint64_t i=0; i<file_pages; i++) pmm_free_page((void*)((uint64_t)phys_buf + i*4096));
        return;
    }
    
    if (ehdr->e_entry >= 0x0000800000000000ULL) {
        vga_print("Exec: Entry point invalido (Acesso Restrito ao Kernel)!\n");
        for(uint64_t i=0; i<file_pages; i++) pmm_free_page((void*)((uint64_t)phys_buf + i*4096));
        return;
    }
    
    pagemap_t process_pml4 = vmm_new_pagemap();
    if (!process_pml4) {
        vga_print("Exec: Erro ao criar Page Map!\n");
        for(uint64_t i=0; i<file_pages; i++) pmm_free_page((void*)((uint64_t)phys_buf + i*4096));
        return;
    }
    
    Elf64_Phdr* phdr = (Elf64_Phdr*)(file_buf + ehdr->e_phoff);
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            uint64_t vaddr = phdr[i].p_vaddr;
            uint64_t memsz = phdr[i].p_memsz;
            uint64_t filesz = phdr[i].p_filesz;
            uint64_t offset = phdr[i].p_offset;
            
            if (vaddr >= 0x0000800000000000ULL) {
                vga_print("Exec: p_vaddr invalido (Kernel Space)!\n");
                for(uint64_t k=0; k<file_pages; k++) pmm_free_page((void*)((uint64_t)phys_buf + k*4096));
                vmm_free_pagemap(process_pml4);
                return;
            }
            
            uint64_t vaddr_aligned = vaddr & ~0xFFFULL;
            uint64_t page_offset = vaddr & 0xFFFULL;
            uint64_t bytes_to_map = memsz + page_offset;
            uint64_t page_count = (bytes_to_map + 4095) / 4096;
            
            for (uint64_t j = 0; j < page_count; j++) {
                uint64_t paddr = (uint64_t)pmm_alloc_page();
                if (!paddr) {
                    vga_print("Exec: Falta de memoria fisica!\n");
                    for(uint64_t k=0; k<file_pages; k++) pmm_free_page((void*)((uint64_t)phys_buf + k*4096));
                    vmm_free_pagemap(process_pml4);
                    return;
                }
                vmm_map_page(process_pml4, vaddr_aligned + (j * 4096), paddr, PAGE_USER | PAGE_WRITE);
                
                uint8_t* dest = (uint8_t*)(paddr + hhdm_offset);
                memset(dest, 0, 4096);
                
                // Copy file data
                uint64_t current_vaddr = vaddr_aligned + (j * 4096);
                if (current_vaddr + 4096 > vaddr && current_vaddr < vaddr + filesz) {
                    uint64_t copy_start_in_page = (current_vaddr < vaddr) ? page_offset : 0;
                    uint64_t src_offset = offset + (current_vaddr + copy_start_in_page - vaddr);
                    uint64_t copy_size = 4096 - copy_start_in_page;
                    if (current_vaddr + copy_start_in_page + copy_size > vaddr + filesz) {
                        copy_size = (vaddr + filesz) - (current_vaddr + copy_start_in_page);
                    }
                    memcpy(dest + copy_start_in_page, file_buf + src_offset, copy_size);
                }
            }
        }
    }
    
    // Alocar User Stack em 0x700000000000 (1 pagina)
    uint64_t stack_phys = (uint64_t)pmm_alloc_page();
    if (!stack_phys) {
        vga_print("Exec: Falta de memoria para stack!\n");
        for(uint64_t k=0; k<file_pages; k++) pmm_free_page((void*)((uint64_t)phys_buf + k*4096));
        vmm_free_pagemap(process_pml4);
        return;
    }
    vmm_map_page(process_pml4, 0x700000000000, stack_phys, PAGE_USER | PAGE_WRITE);
    
    uint64_t user_stack_ptr = 0x700000001000;
    
    if (args) {
        int arg_len = 0;
        while(args[arg_len]) arg_len++;
        arg_len++; // Null terminator
        
        uint8_t* stack_kern_ptr = (uint8_t*)phystokv(stack_phys) + 4096 - arg_len;
        user_stack_ptr -= arg_len;
        
        for (int i = 0; i < arg_len; i++) {
            stack_kern_ptr[i] = args[i];
        }
    }
    
    for(uint64_t i=0; i<file_pages; i++) pmm_free_page((void*)((uint64_t)phys_buf + i*4096));
    
    vga_print("Exec: Entrando no Ring 3...\n");
    
    vmm_switch_pagemap(process_pml4);
    jump_usermode(ehdr->e_entry, user_stack_ptr, user_stack_ptr);
}
