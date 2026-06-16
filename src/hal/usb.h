#ifndef USB_H
#define USB_H

#include <stdint.h>

void usb_ehci_init(uint8_t bus, uint8_t slot, uint8_t func);
void usb_xhci_init(uint8_t bus, uint8_t slot, uint8_t func);

#endif
