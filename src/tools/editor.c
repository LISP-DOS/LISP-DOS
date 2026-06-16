#include "syscalls.h"

#define MEMORY_POOL_SIZE (1024 * 1024)
static char memory_pool[MEMORY_POOL_SIZE];
static int memory_pool_index = 0;

static void* editor_malloc(int size) {
    if (memory_pool_index + size > MEMORY_POOL_SIZE) return NULL;
    void* ptr = &memory_pool[memory_pool_index];
    memory_pool_index += size;
    return ptr;
}

static char* edit_buffer;
static int edit_buffer_max = 0;
static int buffer_len = 0;
static int cursor_pos = 0;
static int is_insert_mode = 0; // 0 = Normal, 1 = Insert
static char current_filename[32];

static char last_cmd_key = 0;

static void draw_editor_ui(void) {
    vga_clear();
    
    size_t max_rows = vga_get_max_rows();
    
    // Linha de status
    vga_set_cursor(max_rows - 2, 0);
    if (is_insert_mode) {
        vga_print("-- INSERT -- [ESC: Normal, Setas: Navegar]");
    } else {
        vga_print("-- NORMAL -- [i: Insert, w: Salvar, q: Sair, Setas: Navegar]");
    }
    
    // Tecla pressionada
    vga_set_cursor(max_rows - 1, 0);
    if (!is_insert_mode && last_cmd_key != 0) {
        vga_print("Tecla Pressionada: ");
        if (last_cmd_key >= 32 && last_cmd_key <= 126) vga_putchar(last_cmd_key);
    }
    
    // Volta o cursor pro início
    vga_set_cursor(0, 0);
    
    // Imprime o buffer de texto atual com o cursor
    for (int i = 0; i <= buffer_len; i++) {
        if (i == cursor_pos) {
            vga_set_color(0, 2); // Cores invertidas para o cursor
            if (i == buffer_len || edit_buffer[i] == '\n') {
                vga_putchar(' '); // Espaco vazio representando o cursor
                vga_set_color(2, 0); // Restaura as cores
                if (i < buffer_len) vga_putchar('\n');
                continue;
            } else {
                vga_putchar(edit_buffer[i]);
                vga_set_color(2, 0); // Restaura as cores
                continue;
            }
        }
        if (i < buffer_len) vga_putchar(edit_buffer[i]);
    }
}

static void kstrncpy(char* dest, const char* src, int max_len) {
    int i = 0;
    while(src[i] && i < max_len - 1) { 
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

void _start(char* args) {
    
    char* filename = "buffer.txt";
    if (args && args[0] != '\0') {
        filename = args;
    }
    
    buffer_len = 0;
    cursor_pos = 0;
    is_insert_mode = 0;
    kstrncpy(current_filename, filename, sizeof(current_filename));
    
    // Tenta carregar do FAT16
    int fsize = fat16_get_file_size(filename);
    
    edit_buffer_max = fsize + 8192; // Espaco extra de 8KB
    edit_buffer = (char*)editor_malloc(edit_buffer_max);
    
    if (!edit_buffer) {
        vga_clear();
        vga_print("Erro: Memoria insuficiente no pool do Editor (1 MB).\n");
        sys_exit();
    }
    
    if (fsize > 0) {
        int size = fat16_read_file(filename, (uint8_t*)edit_buffer);
        if (size > 0) {
            buffer_len = size;
        }
    }
    
    draw_editor_ui();
    
    while (1) {
        unsigned char c = keyboard_getchar();
        
        if (c == 27) { // ESCAPE
            is_insert_mode = 0;
            last_cmd_key = 0;
            draw_editor_ui();
            continue;
        }
        
        // Teclas direcionais
        if (c >= 128 && c <= 131) {
            if (c == 130) { // Esquerda
                if (cursor_pos > 0) cursor_pos--;
            } else if (c == 131) { // Direita
                if (cursor_pos < buffer_len) cursor_pos++;
            } else if (c == 128) { // Cima
                int col = 0;
                int p = cursor_pos - 1;
                while(p >= 0 && edit_buffer[p] != '\n') { col++; p--; }
                if (p >= 0) {
                    int prev_start = p - 1;
                    while(prev_start >= 0 && edit_buffer[prev_start] != '\n') { prev_start--; }
                    prev_start++;
                    int new_pos = prev_start;
                    for(int i=0; i<col; i++) {
                        if (new_pos == p) break;
                        new_pos++;
                    }
                    cursor_pos = new_pos;
                }
            } else if (c == 129) { // Baixo
                int col = 0;
                int p = cursor_pos - 1;
                while(p >= 0 && edit_buffer[p] != '\n') { col++; p--; }
                int next_start = cursor_pos;
                while(next_start < buffer_len && edit_buffer[next_start] != '\n') { next_start++; }
                if (next_start < buffer_len) {
                    next_start++;
                    int new_pos = next_start;
                    for(int i=0; i<col; i++) {
                        if (new_pos >= buffer_len || edit_buffer[new_pos] == '\n') break;
                        new_pos++;
                    }
                    cursor_pos = new_pos;
                }
            }
            draw_editor_ui();
            continue;
        }
        
        if (is_insert_mode) {
            if (c == '\b') {
                if (cursor_pos > 0) {
                    // Shift left
                    for(int i = cursor_pos; i < buffer_len; i++) {
                        edit_buffer[i-1] = edit_buffer[i];
                    }
                    cursor_pos--;
                    buffer_len--;
                }
            } else if (buffer_len < edit_buffer_max - 1 && ((c >= 32 && c <= 126) || c == '\n')) {
                // Shift right
                for(int i = buffer_len; i > cursor_pos; i--) {
                    edit_buffer[i] = edit_buffer[i-1];
                }
                edit_buffer[cursor_pos++] = c;
                buffer_len++;
            }
            draw_editor_ui();
        } else {
            // Modo Normal (Vim)
            last_cmd_key = c;
            if (c == 'i') {
                is_insert_mode = 1;
                last_cmd_key = 0;
                draw_editor_ui();
            } else if (c == 'w') { // Salvar e Sair
                edit_buffer[buffer_len] = '\0';
                int written = fat16_write_file(current_filename, (const uint8_t*)edit_buffer, buffer_len);
                vga_clear();
                if (written > 0) {
                    vga_print("Arquivo salvo no HD (FAT16).\n");
                } else {
                    vga_print("Erro ao salvar o arquivo.\n");
                }
                sys_exit();
            } else if (c == 'q') { // Sair sem salvar
                vga_clear();
                sys_exit();
            } else {
                draw_editor_ui();
            }
        }
    }
}
