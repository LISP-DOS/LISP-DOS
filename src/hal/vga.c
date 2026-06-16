#include "vga.h"
#include <limine.h>
#include "font8x8_basic.h"

__attribute__((used, section(".requests")))
volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

static struct limine_framebuffer *fb = NULL;
static size_t terminal_row = 0;
static size_t terminal_column = 0;
static uint32_t fg_color = 0x00FF00; // Verde
static uint32_t bg_color = 0x000000; // Preto

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

void vga_init(void) {
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        return; // Falha ao obter framebuffer
    }
    fb = framebuffer_request.response->framebuffers[0];
    vga_clear();
}

void vga_clear(void) {
    if (!fb) return;
    uint32_t *fb_ptr = fb->address;
    for (size_t i = 0; i < (fb->width * fb->height); i++) {
        fb_ptr[i] = bg_color;
    }
    terminal_row = 0;
    terminal_column = 0;
}

#define FONT_SCALE 2

void vga_set_cursor(size_t row, size_t col) {
    terminal_row = row;
    terminal_column = col;
}

size_t vga_get_max_rows(void) {
    if (!fb) return 25;
    return fb->height / (8 * FONT_SCALE);
}

size_t vga_get_max_cols(void) {
    if (!fb) return 80;
    return fb->width / (8 * FONT_SCALE);
}

void vga_set_color(uint8_t fg, uint8_t bg) {
    // Mapeamento simples de cores VGA para RGB
    uint32_t colors[16] = {
        0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 
        0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
        0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
        0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF
    };
    fg_color = colors[fg & 0x0F];
    bg_color = colors[bg & 0x0F];
}

void vga_putchar(char c) {
    outb(0x3F8, c); // Mantém a saída serial para log
    
    if (!fb) return;

    if (c == '\b') {
        if (terminal_column > 0) {
            terminal_column--;
        } else if (terminal_row > 0) {
            terminal_row--;
            terminal_column = (fb->width / (8 * FONT_SCALE)) - 1;
        }
        // Limpar o caractere
        uint32_t *fb_ptr = fb->address;
        size_t start_x = terminal_column * (8 * FONT_SCALE);
        size_t start_y = terminal_row * (8 * FONT_SCALE);
        for (int y = 0; y < 8 * FONT_SCALE; y++) {
            for (int x = 0; x < 8 * FONT_SCALE; x++) {
                fb_ptr[(start_y + y) * (fb->pitch / 4) + (start_x + x)] = bg_color;
            }
        }
        return;
    }

    if (c == '\n') {
        terminal_column = 0;
        if (++terminal_row >= fb->height / (8 * FONT_SCALE)) {
            terminal_row = 0;
        }
        return;
    }

    uint8_t *glyph = (uint8_t *)font8x8_basic[(uint8_t)c];
    uint32_t *fb_ptr = fb->address;
    size_t start_x = terminal_column * (8 * FONT_SCALE);
    size_t start_y = terminal_row * (8 * FONT_SCALE);

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            int set = glyph[y] & (1 << x);
            uint32_t color = set ? fg_color : bg_color;
            // Desenha o pixel em escala
            for (int sy = 0; sy < FONT_SCALE; sy++) {
                for (int sx = 0; sx < FONT_SCALE; sx++) {
                    size_t fb_y = start_y + (y * FONT_SCALE) + sy;
                    size_t fb_x = start_x + (x * FONT_SCALE) + sx;
                    fb_ptr[fb_y * (fb->pitch / 4) + fb_x] = color;
                }
            }
        }
    }

    if (++terminal_column >= fb->width / (8 * FONT_SCALE)) {
        terminal_column = 0;
        if (++terminal_row >= fb->height / (8 * FONT_SCALE)) {
            terminal_row = 0;
        }
    }
}

void vga_print(const char* data) {
    for (size_t i = 0; data[i] != '\0'; i++) {
        vga_putchar(data[i]);
    }
}

void vga_print_hex(uint64_t n) {
    char buffer[20];
    int i = 0;
    if (n == 0) {
        buffer[i++] = '0';
    } else {
        while (n > 0) {
            int rem = n % 16;
            if (rem < 10) buffer[i++] = '0' + rem;
            else buffer[i++] = 'A' + (rem - 10);
            n /= 16;
        }
    }
    vga_print("0x");
    while (i > 0) {
        vga_putchar(buffer[--i]);
    }
}
