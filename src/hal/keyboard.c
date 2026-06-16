#include "keyboard.h"
#include "vga.h"
#include <stdint.h>

#include "io.h"

const char scancode_to_ascii[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

const char scancode_to_ascii_shift[] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' '
};

char keyboard_buffer[256];
volatile int kbd_head = 0;
volatile int kbd_tail = 0;
static int shift_pressed = 0;

static int e0_pressed = 0;

void keyboard_handler(void) {
    uint8_t status = inb(0x64);
    if (status & 1) {
        uint8_t scancode = inb(0x60);
        
        if (scancode == 0xE0) {
            e0_pressed = 1;
        } else if (scancode == 0x2A || scancode == 0x36) {
            shift_pressed = 1;
        } else if (scancode == 0xAA || scancode == 0xB6) {
            shift_pressed = 0;
        } else if (!(scancode & 0x80)) { 
            char c = 0;
            if (e0_pressed) {
                if (scancode == 0x48) c = 128; // Up
                else if (scancode == 0x50) c = 129; // Down
                else if (scancode == 0x4B) c = 130; // Left
                else if (scancode == 0x4D) c = 131; // Right
                e0_pressed = 0;
            } else {
                if (scancode < sizeof(scancode_to_ascii)) {
                    c = shift_pressed ? scancode_to_ascii_shift[scancode] : scancode_to_ascii[scancode];
                }
            }
            
            if (c) {
                keyboard_buffer[kbd_head] = c;
                kbd_head = (kbd_head + 1) % 256;
            }
        } else {
            // Key release
            if (e0_pressed) e0_pressed = 0;
        }
    }
    outb(0x20, 0x20); // EOI
}

char keyboard_getchar(void) {
    while (1) {
        __asm__ volatile("cli");
        if (kbd_head != kbd_tail) {
            __asm__ volatile("sti");
            break;
        }
        __asm__ volatile("sti\n\thlt");
    }
    char c = keyboard_buffer[kbd_tail];
    kbd_tail = (kbd_tail + 1) % 256;
    return c;
}
