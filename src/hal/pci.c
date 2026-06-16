#include "pci.h"
#include "io.h"
#include "vga.h"
#include "usb.h"
#include <stdint.h>

uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc) | ((uint32_t)0x80000000));
    outl(0xCF8, address);
    return inl(0xCFC);
}

void pci_init(void) {
    int found_usb = 0;
    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            uint32_t vendor = pci_read_config(bus, slot, 0, 0);
            if ((vendor & 0xFFFF) == 0xFFFF) continue;
            
            for (int func = 0; func < 8; func++) {
                uint32_t class_info = pci_read_config(bus, slot, func, 0x08);
                uint8_t class_code = (class_info >> 24) & 0xFF;
                uint8_t subclass = (class_info >> 16) & 0xFF;
                uint8_t prog_if = (class_info >> 8) & 0xFF;
                
                if (class_code == 0x0C && subclass == 0x03) {
                    found_usb = 1;
                    if (prog_if == 0x00) vga_print("PCI: Encontrado Controlador USB UHCI\n");
                    else if (prog_if == 0x10) vga_print("PCI: Encontrado Controlador USB OHCI\n");
                    else if (prog_if == 0x20) {
                        vga_print("PCI: Encontrado Controlador USB EHCI\n");
                        usb_ehci_init(bus, slot, func);
                    }
                    else if (prog_if == 0x30) {
                        vga_print("PCI: Encontrado Controlador USB xHCI\n");
                        usb_xhci_init(bus, slot, func);
                    }
                    else vga_print("PCI: Encontrado Controlador USB Desconhecido\n");
                }
            }
        }
    }
    
    if (found_usb) {
        vga_print("USB: Rotinas xHCI/EHCI nativas ainda nao iniciadas. Usando BIOS SMI Legacy Emulation.\n");
    }
}
