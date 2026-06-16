#include <stdint.h>
#include <stddef.h>
#include <limine.h>
#include "../hal/vga.h"
#include "pmm.h"
#include "vmm.h"
#include "heap.h"
#include "tarfs.h"
#include "../hal/ata.h"
#include "../tools/editor.h"
#include "../tools/lisp.h"
#include "fat16.h"
#include "../hal/io.h"
#include "../hal/acpi.h"
#include "../hal/pci.h"
#include "syscall.h"
extern void gdt_init(void);
extern void idt_init(void);
extern void pic_remap(void);
extern char keyboard_getchar(void);

__attribute__((used, section(".requests")))
LIMINE_BASE_REVISION(1)

extern volatile struct limine_rsdp_request rsdp_request;
extern volatile struct limine_framebuffer_request framebuffer_request;
extern volatile struct limine_module_request module_request;
extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_hhdm_request hhdm_request;

#include "../hal/serial.h"

static void fpu_init(void) {
    uint64_t cr0, cr4;
    
    // Ler CR0
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1 << 2); // Limpa EM (Emulation)
    cr0 |= (1 << 1);  // Seta MP (Monitor Coprocessor)
    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0));
    
    // Ler CR4
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 9);  // OSFXSR: Suporte a salvar/restaurar FXSAVE/FXRSTOR
    cr4 |= (1 << 10); // OSXMMEXCPT: Suporte a exceções SIMD
    __asm__ volatile("mov %0, %%cr4" :: "r"(cr4));
}

void _start(void) {
    serial_init();
    serial_print("[BOOT] serial_init done\n");

    vga_init();
    vga_print("Inicializando Hardware e Memoria...\n");
    serial_print("[BOOT] vga_init done\n");

    gdt_init();
    serial_print("[BOOT] gdt_init done\n");
    idt_init();
    serial_print("[BOOT] idt_init done\n");
    fpu_init();
    serial_print("[BOOT] fpu_init done\n");
    syscall_init();
    serial_print("[BOOT] syscall_init done\n");
    pic_remap();
    serial_print("[BOOT] pic_remap done\n");
    
    pmm_init();
    serial_print("[BOOT] pmm_init done\n");
    vmm_init();
    serial_print("[BOOT] vmm_init done\n");
    heap_init();
    serial_print("[BOOT] heap_init done\n");
    tarfs_init();
    serial_print("[BOOT] tarfs_init done\n");
    fat16_init(); // Inicializa o disco rígido com FAT16
    serial_print("[BOOT] fat16_init done\n");
    pci_init();   // Inicializa barramento PCI
    serial_print("[BOOT] pci_init done\n");
    acpi_init();  // Inicializa gerenciamento de energia ACPI
    serial_print("[BOOT] acpi_init done\n");
    
    vga_print("LISP-DOS Iniciado com sucesso!\n\n");
    
    // Loop principal (Shell simplificado)
    char command[256];
    int cmd_len = 0;
    while (1) {
        vga_print("C:\\> ");
        cmd_len = 0;

        while (1) {
            char c = keyboard_getchar();
            if (c == '\n') {
                vga_putchar('\n');
                command[cmd_len] = '\0';
                break;
            } else if (c == '\b') {
                if (cmd_len > 0) {
                    vga_putchar('\b');
                    cmd_len--;
                }
            } else {
                vga_putchar(c);
                if (cmd_len < 255) {
                    command[cmd_len++] = c;
                }
            }
        }

        if (cmd_len == 0) continue;

        // Procurar o primeiro espaço para separar o comando dos argumentos
        int space_idx = -1;
        for (int i = 0; i < cmd_len; i++) {
            if (command[i] == ' ') {
                space_idx = i;
                break;
            }
        }

        char cmd_name[32];
        char args[224];
        cmd_name[0] = '\0';
        args[0] = '\0';

        if (space_idx != -1) {
            int len = space_idx < 31 ? space_idx : 31;
            for (int i = 0; i < len; i++) cmd_name[i] = command[i];
            cmd_name[len] = '\0';

            int arg_idx = 0;
            for (int i = space_idx + 1; i < cmd_len; i++) {
                args[arg_idx++] = command[i];
            }
            args[arg_idx] = '\0';
        } else {
            int len = cmd_len < 31 ? cmd_len : 31;
            for (int i = 0; i < len; i++) cmd_name[i] = command[i];
            cmd_name[len] = '\0';
        }

        int strcmp(const char* s1, const char* s2) {
            while(*s1 && *s1 == *s2) { s1++; s2++; }
            return *(const unsigned char*)s1 - *(const unsigned char*)s2;
        }

        if (strcmp(cmd_name, "echo") == 0) {
            vga_print(args);
            vga_print("\n");
        } else if (strcmp(cmd_name, "cls") == 0 || strcmp(cmd_name, "clear") == 0) {
            vga_clear();
        } else if (strcmp(cmd_name, "dir") == 0) {
            tarfs_dir();
            fat16_dir();
        } else if (strcmp(cmd_name, "edit") == 0) {
            extern void process_exec(const char*, const char*);
            process_exec("bin/editor.elf", args);
        } else if (strcmp(cmd_name, "lisp") == 0) {
            extern void process_exec(const char*, const char*);
            process_exec("bin/lisp.elf", args);
        } else if (strcmp(cmd_name, "type") == 0) {
            tarfs_type(args);
        } else if (strcmp(cmd_name, "exec") == 0) {
            extern void process_exec(const char*, const char*);
            // exec needs filename and args, we can just pass args as both for now
            // ou separar args em filename e sub-args
            process_exec(args, NULL);
        } else if (strcmp(cmd_name, "rm") == 0) {
            if (args[0] == '\0') {
                vga_print("Sintaxe: rm <arquivo>\n");
            } else {
                int status = fat16_delete_file(args);
                if (status == 1) {
                    vga_print("Arquivo removido.\n");
                } else {
                    vga_print("Arquivo nao encontrado ou erro ao remover.\n");
                }
            }
        } else if (strcmp(cmd_name, "poweroff") == 0 || strcmp(cmd_name, "exit") == 0) {
            vga_print("Sincronizando discos e buffers na RAM...\n");
            vga_print("Sincronizacao completa.\n");
            vga_print("Desligando o sistema...\n");
            acpi_poweroff();
            __asm__ volatile("cli; hlt");
        } else if (strcmp(cmd_name, "reboot") == 0) {
            vga_print("Reiniciando o sistema...\n");
            uint8_t good = 0x02;
            while (good & 0x02) {
                good = inb(0x64);
            }
            outb(0x64, 0xFE);
            __asm__ volatile("cli; hlt");
        } else {
            vga_print("Comando invalido ou nome de arquivo incorreto\n");
        }
    }

    __asm__ volatile("cli; hlt");
}
