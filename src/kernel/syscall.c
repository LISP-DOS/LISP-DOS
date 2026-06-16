#include "syscall.h"
#include "../hal/vga.h"
#include "vmm.h"

// MSRs para Syscall
#define MSR_EFER  0xC0000080
#define MSR_STAR  0xC0000081
#define MSR_LSTAR 0xC0000082
#define MSR_FMASK 0xC0000084

static inline void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t low = val & 0xFFFFFFFF;
    uint32_t high = val >> 32;
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

extern void syscall_entry(void);

void syscall_init(void) {
    uint64_t efer = rdmsr(MSR_EFER);
    efer |= 1; // Habilita SCE (System Call Extensions)
    wrmsr(MSR_EFER, efer);

    // STAR: Sysret CS (bits 63:48) e Syscall CS (bits 47:32)
    // Kernel CS = 0x08, User CS32 = 0x18, User CS64 = 0x20
    // O sysret carrega SS = STAR[63:48] + 8 = 0x10 + 8 = 0x18 (User DS)
    // O sysret carrega CS = STAR[63:48] + 16 = 0x10 + 16 = 0x20 (User CS)
    // O GDT tem:
    // 0: Null, 0x08: Kern CS, 0x10: Kern DS, 0x18: User DS, 0x20: User CS
    // O MSR_STAR precisa: ((0x10) << 48) | (KernCS << 32)
    uint64_t star = ((uint64_t)0x10 << 48) | ((uint64_t)0x08 << 32);
    wrmsr(MSR_STAR, star);

    wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);

    // FMASK: Desabilita interrupcoes ao entrar na syscall (RFLAGS)
    wrmsr(MSR_FMASK, 0x200);
}

// Handler em C
uint64_t syscall_handler_c(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    if (num == 1) { // sys_write
        if (!vmm_check_user_string(arg2)) return (uint64_t)-1;
        vga_print((const char *)arg2);
        return 0;
    } else if (num == 2) { // sys_clear
        vga_clear();
        return 0;
    } else if (num == 3) { // sys_set_cursor
        vga_set_cursor(arg1, arg2);
        return 0;
    } else if (num == 4) { // sys_putchar
        vga_putchar((char)arg1);
        return 0;
    } else if (num == 5) { // sys_set_color
        vga_set_color(arg1, arg2);
        return 0;
    } else if (num == 6) { // sys_get_max_rows
        return vga_get_max_rows();
    } else if (num == 7) { // sys_keyboard_getchar
        extern char keyboard_getchar(void);
        return keyboard_getchar();
    } else if (num == 8) { // sys_fat16_read_file
        if (!vmm_check_user_string(arg1)) return (uint64_t)-1;
        extern uint32_t fat16_get_file_size(const char*);
        uint32_t size = fat16_get_file_size((const char*)arg1);
        if (size == 0) return 0;
        if (!vmm_check_user_ptr(arg2, size, 1)) {
            extern void vga_print_hex(uint64_t);
            vga_print("Syscall Read: Check User Ptr Failed! arg2=0x");
            vga_print_hex(arg2);
            vga_print(" size=");
            vga_print_hex(size);
            vga_print("\n");
            return (uint64_t)-1;
        }
        extern int fat16_read_file(const char* filename, uint8_t* buffer);
        return fat16_read_file((const char*)arg1, (uint8_t*)arg2);
    } else if (num == 9) { // sys_fat16_write_file
        if (!vmm_check_user_string(arg1)) return (uint64_t)-1;
        uint32_t size = (uint32_t)arg3;
        if (!vmm_check_user_ptr(arg2, size, 0)) return (uint64_t)-1;
        extern void fat16_write_file(const char* filename, const uint8_t* data, uint32_t size);
        fat16_write_file((const char*)arg1, (const uint8_t*)arg2, size);
        return 0;
    } else if (num == 10) { // sys_fat16_get_file_size
        if (!vmm_check_user_string(arg1)) return (uint64_t)-1;
        extern uint32_t fat16_get_file_size(const char*);
        return (uint64_t)fat16_get_file_size((const char*)arg1);
    } else if (num == 60) { // sys_exit
        extern void vmm_restore_kernel_pagemap(void);
        extern void asm_return_to_kernel(void);
        
        uint64_t current_cr3;
        __asm__ volatile("mov %%cr3, %0" : "=r"(current_cr3));
        
        vmm_restore_kernel_pagemap();
        vmm_free_pagemap((void*)current_cr3);
        
        asm_return_to_kernel();
        return 0;
    }
    return 0;
}
