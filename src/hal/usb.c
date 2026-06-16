#include "usb.h"
#include "vga.h"

void usb_ehci_init(uint8_t bus, uint8_t slot, uint8_t func) {
    (void)bus; (void)slot; (void)func;
    vga_print("USB: Inicializando driver EHCI puro (stub)...\n");
    // TODO: Mapear MMIO registers do controlador EHCI via BAR0
    // TODO: Desativar Legacy Support via BIOS (OS Ownership handoff)
    // TODO: Inicializar fila assincrona (Async Schedule) e estruturas de dados
}

void usb_xhci_init(uint8_t bus, uint8_t slot, uint8_t func) {
    (void)bus; (void)slot; (void)func;
    vga_print("USB: Inicializando driver xHCI puro (stub)...\n");
    // TODO: Ler Capability Registers, Operational Registers e Runtime Registers
    // TODO: Requisitar OS Ownership via USBLEGSUP
    // TODO: Criar Device Context Base Address Array (DCBAA) e Command Ring
}
