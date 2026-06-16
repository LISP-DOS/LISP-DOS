#include "pic.h"
#include <stdint.h>

#include "io.h"

void pic_remap(void) {
    // Save masks if we wanted to restore, but we force them at the end.
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    
    outb(0x21, 0x20); // Master PIC vector offset 32
    outb(0xA1, 0x28); // Slave PIC vector offset 40
    
    outb(0x21, 4);
    outb(0xA1, 2);
    
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    
    // Unmask IRQ1 (Teclado) e IRQ2 (Cascade)
    outb(0x21, 0xFD); // 1111 1101 (Desmascara apenas o IRQ1)
    outb(0xA1, 0xFF); // Mantém o PIC escravo mascarado
}
