#include "idt.h"

struct idt_entry {
    uint16_t isr_low;
    uint16_t kernel_cs;
    uint8_t ist;
    uint8_t attributes;
    uint16_t isr_mid;
    uint32_t isr_high;
    uint32_t reserved;
} __attribute__((packed));

struct idtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct idt_entry idt[256];
struct idtr idtp;

void idt_set_descriptor(uint8_t vector, void *isr, uint8_t flags) {
    uint64_t addr = (uint64_t)isr;
    idt[vector].isr_low = addr & 0xFFFF;
    idt[vector].kernel_cs = 0x08;
    idt[vector].ist = 0;
    idt[vector].attributes = flags;
    idt[vector].isr_mid = (addr >> 16) & 0xFFFF;
    idt[vector].isr_high = (addr >> 32) & 0xFFFFFFFF;
    idt[vector].reserved = 0;
}

struct registers {
    uint64_t ds;
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp, r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
};

extern void isr0(void); extern void isr1(void); extern void isr2(void); extern void isr3(void);
extern void isr4(void); extern void isr5(void); extern void isr6(void); extern void isr7(void);
extern void isr8(void); extern void isr9(void); extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void); extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void); extern void isr30(void); extern void isr31(void);
extern void isr_default(void);

extern void vga_print(const char*);
extern void vga_print_hex(uint64_t);
extern void asm_return_to_kernel(void);

void exception_handler(struct registers *regs) {
    if (regs->int_no == 33) return; // shouldn't happen here
    
    if ((regs->cs & 3) == 3) { // Ring 3
        uint64_t cr2;
        __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
        vga_print("\n[!] Falha de Seguranca: Processo Ring 3 abortado (Excecao ");
        vga_print_hex(regs->int_no);
        vga_print(") no RIP: ");
        vga_print_hex(regs->rip);
        vga_print(" - CR2: 0x");
        vga_print_hex(cr2);
        vga_print("\n");
        asm_return_to_kernel();
    } else { // Ring 0
        vga_print("\nKERNEL PANIC: Excecao nao tratada no Ring 0 (Vetor ");
        vga_print_hex(regs->int_no);
        vga_print(") RIP=");
        vga_print_hex(regs->rip);
        vga_print("\nSistema travado.\n");
        __asm__ volatile("cli; hlt");
    }
}

static void idt_set_exception_gates(void) {
    void* isrs[32] = {
        isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
        isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
        isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
        isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
    };
    for (int i = 0; i < 32; i++) {
        idt_set_descriptor(i, isrs[i], 0x8E);
    }
}

void idt_init(void) {
    idtp.base = (uint64_t)&idt[0];
    idtp.limit = (uint16_t)sizeof(struct idt_entry) * 256 - 1;

    for (int vector = 0; vector < 256; vector++) {
        idt_set_descriptor(vector, isr0, 0x8E); // Fallback temporário
    }
    
    idt_set_exception_gates();
    
    extern void isr_keyboard(void);
    idt_set_descriptor(33, isr_keyboard, 0x8E);

    __asm__ volatile ("lidt %0" : : "m"(idtp));
    __asm__ volatile ("sti");
}
