#ifndef XHCI_H
#define XHCI_H

#include <stdint.h>

// Estrutura base para as capacidades do xHCI (Capability Registers)
typedef struct {
    uint8_t caplength;
    uint8_t reserved;
    uint16_t hciversion;
    uint32_t hcsparams1;
    uint32_t hcsparams2;
    uint32_t hcsparams3;
    uint32_t hccparams1;
    uint32_t dboff;
    uint32_t rtsoff;
    uint32_t hccparams2;
} __attribute__((packed)) xhci_cap_regs_t;

// Função de inicialização do xHCI (chamada pelo PCI scan)
void xhci_init_controller(uint32_t bus, uint32_t slot, uint32_t func);

#endif
