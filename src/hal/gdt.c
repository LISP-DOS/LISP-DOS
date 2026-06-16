#include "gdt.h"
#include "../kernel/string.h"

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

struct tss_entry {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct gdt_entry gdt[7];
struct gdt_ptr gp;
struct tss_entry tss;

void gdt_set_gate(int num, uint64_t base, uint64_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;
    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = ((limit >> 16) & 0x0F);
    gdt[num].granularity |= (gran & 0xF0);
    gdt[num].access = access;
}

extern void gdt_flush(uint64_t);
extern void tss_flush(void);

void gdt_init(void) {
    gp.limit = (sizeof(struct gdt_entry) * 7) - 1;
    gp.base = (uint64_t)&gdt;
    
    gdt_set_gate(0, 0, 0, 0, 0); // Null
    gdt_set_gate(1, 0, 0xFFFFF, 0x9A, 0xAF); // Kernel CS
    gdt_set_gate(2, 0, 0xFFFFF, 0x92, 0xAF); // Kernel DS
    gdt_set_gate(3, 0, 0xFFFFF, 0xF2, 0xAF); // User DS (Ring 3) -> 0x18
    gdt_set_gate(4, 0, 0xFFFFF, 0xFA, 0xAF); // User CS (Ring 3) -> 0x20
    
    // TSS (104 bytes = 0x68 limit)
    uint64_t tss_base = (uint64_t)&tss;
    gdt_set_gate(5, tss_base, sizeof(struct tss_entry) - 1, 0x89, 0x00);
    gdt[6].limit_low = (tss_base >> 32) & 0xFFFF;
    gdt[6].base_low = (tss_base >> 48) & 0xFFFF;
    gdt[6].base_middle = 0;
    gdt[6].access = 0;
    gdt[6].granularity = 0;
    gdt[6].base_high = 0;
    
    // Configurar TSS básica
    memset(&tss, 0, sizeof(struct tss_entry));
    tss.iopb_offset = sizeof(struct tss_entry);
    
    gdt_flush((uint64_t)&gp);
    tss_flush();
}

void tss_set_kernel_stack(uint64_t stack) {
    tss.rsp0 = stack;
}
