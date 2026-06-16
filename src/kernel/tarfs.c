#include "tarfs.h"
#include <limine.h>
#include "../hal/vga.h"
#include <stdint.h>
#include <stddef.h>

__attribute__((used, section(".requests")))
volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST,
    .revision = 0
};

struct tar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
};

static uint8_t* ramdisk_base = NULL;
static uint64_t ramdisk_size = 0;

static uint64_t octal_to_int(const char* str, int size) {
    uint64_t n = 0;
    while (size-- > 0 && *str) {
        if (*str >= '0' && *str <= '7') {
            n = (n * 8) + (*str - '0');
        }
        str++;
    }
    return n;
}

static int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

void tarfs_init(void) {
    struct limine_module_response *modules = module_request.response;
    if (modules == NULL || modules->module_count == 0) {
        vga_print("Aviso: Nenhum modulo (RAM disk) detectado pelo Limine.\n");
        return;
    }
    struct limine_file *module = modules->modules[0];
    ramdisk_base = (uint8_t*)module->address;
    ramdisk_size = module->size;
    vga_print("TarFS: Unidade C: (Initramfs) montada com sucesso.\n");
}

void tarfs_dir(void) {
    if (!ramdisk_base) {
        vga_print("Volume sem formato ou sem disco.\n");
        return;
    }
    uint64_t offset = 0;
    vga_print("\n Pasta de C:\\\n\n");
    int count = 0;
    
    while (offset < ramdisk_size) {
        struct tar_header *header = (struct tar_header *)(ramdisk_base + offset);
        if (header->name[0] == '\0') break; 
        
        uint64_t size = octal_to_int(header->size, 11);
        
        if (header->typeflag != '5') {
            vga_print(header->name);
            vga_print("    <ARQUIVO>\n");
            count++;
        }
        
        uint64_t file_blocks = (size + 511) / 512;
        offset += 512 + (file_blocks * 512);
    }
    
    if (count == 0) {
        vga_print("Arquivo nao encontrado\n");
    } else {
        vga_print("\n");
    }
}

void tarfs_type(const char* filename) {
    if (!ramdisk_base) {
        vga_print("Volume sem formato ou sem disco.\n");
        return;
    }
    
    // Ignorar espacos iniciais se existirem no argumento do comando type
    while(*filename == ' ') filename++;
    
    uint64_t offset = 0;
    int found = 0;
    while (offset < ramdisk_size) {
        struct tar_header *header = (struct tar_header *)(ramdisk_base + offset);
        if (header->name[0] == '\0') break;
        
        uint64_t size = octal_to_int(header->size, 11);
        
        if (strcmp(header->name, filename) == 0) {
            found = 1;
            char* file_data = (char*)(ramdisk_base + offset + 512);
            for (uint64_t i = 0; i < size; i++) {
                if (file_data[i] != '\r') vga_putchar(file_data[i]);
            }
            vga_print("\n");
            break;
        }
        
        uint64_t file_blocks = (size + 511) / 512;
        offset += 512 + (file_blocks * 512);
    }
    
    if (!found) {
        vga_print("O sistema nao pode encontrar o arquivo especificado.\n");
    }
}

uint64_t tarfs_get_file_size(const char* filename) {
    if (!ramdisk_base) return 0;
    uint64_t offset = 0;
    while (offset < ramdisk_size) {
        struct tar_header *header = (struct tar_header *)(ramdisk_base + offset);
        if (header->name[0] == '\0') break;
        uint64_t size = octal_to_int(header->size, 11);
        if (strcmp(header->name, filename) == 0) return size;
        uint64_t file_blocks = (size + 511) / 512;
        offset += 512 + (file_blocks * 512);
    }
    return 0;
}

int tarfs_read_file(const char* filename, uint8_t* buffer) {
    if (!ramdisk_base) return 0;
    uint64_t offset = 0;
    while (offset < ramdisk_size) {
        struct tar_header *header = (struct tar_header *)(ramdisk_base + offset);
        if (header->name[0] == '\0') break;
        uint64_t size = octal_to_int(header->size, 11);
        if (strcmp(header->name, filename) == 0) {
            char* file_data = (char*)(ramdisk_base + offset + 512);
            for(uint64_t i = 0; i < size; i++) buffer[i] = file_data[i];
            return size;
        }
        uint64_t file_blocks = (size + 511) / 512;
        offset += 512 + (file_blocks * 512);
    }
    return 0;
}
