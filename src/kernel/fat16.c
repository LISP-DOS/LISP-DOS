#include "fat16.h"
#include "../hal/ata.h"
#include "../hal/vga.h"

static struct fat_BS bpb;
static uint32_t root_dir_sector;
static uint32_t root_dir_size;
static uint32_t data_sector;

static void format_fat_name(const char* in, char* out) {
    for(int i=0; i<11; i++) out[i] = ' ';
    int i=0, j=0;
    while(in[i] && in[i] != '.' && j < 8) {
        char c = in[i++];
        if (c >= 'a' && c <= 'z') c -= 32;
        out[j++] = c;
    }
    while(in[i] && in[i] != '.') i++;
    if (in[i] == '.') {
        i++;
        j = 8;
        while(in[i] && j < 11) {
            char c = in[i++];
            if (c >= 'a' && c <= 'z') c -= 32;
            out[j++] = c;
        }
    }
}

static uint16_t fat16_get_fat_entry(uint16_t cluster) {
    uint8_t sector[512];
    uint32_t fat_offset = cluster * 2;
    uint32_t fat_sector = bpb.reserved_sector_count + (fat_offset / 512);
    uint32_t ent_offset = fat_offset % 512;
    ata_read_sector(fat_sector, sector);
    return *(uint16_t*)&sector[ent_offset];
}

static void fat16_set_fat_entry(uint16_t cluster, uint16_t value) {
    uint8_t sector[512];
    uint32_t fat_offset = cluster * 2;
    uint32_t fat_sector = bpb.reserved_sector_count + (fat_offset / 512);
    uint32_t ent_offset = fat_offset % 512;
    ata_read_sector(fat_sector, sector);
    *(uint16_t*)&sector[ent_offset] = value;
    ata_write_sector(fat_sector, sector);
    if (bpb.table_count > 1) {
        ata_write_sector(fat_sector + bpb.table_size_16, sector);
    }
}

static uint16_t fat16_alloc_cluster(void) {
    // Procura por um cluster livre (valor 0)
    uint32_t total_sectors = bpb.total_sectors_16;
    if (total_sectors == 0) total_sectors = bpb.total_sectors_32;
    
    uint32_t root_dir_sectors = ((bpb.root_entry_count * 32) + (bpb.bytes_per_sector - 1)) / bpb.bytes_per_sector;
    uint32_t data_sectors = total_sectors - (bpb.reserved_sector_count + (bpb.table_count * bpb.table_size_16) + root_dir_sectors);
    uint32_t total_clusters = data_sectors / bpb.sectors_per_cluster;
    
    for(uint16_t i=2; i<total_clusters && i<0xFFF0; i++) {
        if (fat16_get_fat_entry(i) == 0x0000) {
            fat16_set_fat_entry(i, 0xFFFF); // Marca como EOF temporário
            return i;
        }
    }
    return 0; // Disco cheio
}

void fat16_init(void) {
    uint8_t buffer[512];
    ata_read_sector(0, buffer);
    struct fat_BS *boot_sector = (struct fat_BS *)buffer;
    
    if (boot_sector->boot_sign != 0xAA55) {
        vga_print("FAT16: Assinatura 0xAA55 nao encontrada no setor 0.\n");
        return;
    }
    
    bpb = *boot_sector;
    root_dir_sector = bpb.reserved_sector_count + (bpb.table_count * bpb.table_size_16);
    root_dir_size = (bpb.root_entry_count * 32) / 512;
    data_sector = root_dir_sector + root_dir_size;
}

void fat16_dir(void) {
    vga_print("\n Pasta do HD (FAT16):\n\n");
    int count = 0;
    
    for (uint32_t sec = 0; sec < root_dir_size; sec++) {
        uint8_t root_sec[512];
        ata_read_sector(root_dir_sector + sec, root_sec);
        struct fat16_dir_entry* entries = (struct fat16_dir_entry*)root_sec;
        
        for (int i = 0; i < 16; i++) {
            if (entries[i].name[0] == 0x00) goto done; // Fim real do diretório
            if ((unsigned char)entries[i].name[0] == 0xE5) continue; // Arquivo deletado
            if (entries[i].attributes == 0x0F) continue; // LFN (Long File Name)
            
            // É um arquivo válido
            for(int j=0; j<8; j++) if(entries[i].name[j] != ' ') vga_putchar(entries[i].name[j]);
            if (entries[i].ext[0] != ' ') {
                vga_putchar('.');
                for(int j=0; j<3; j++) if(entries[i].ext[j] != ' ') vga_putchar(entries[i].ext[j]);
            }
            
            if (entries[i].attributes & 0x10) {
                vga_print("\t<DIR>\n");
            } else {
                vga_print("\t<ARQUIVO>\t");
                // Print size (simplified print_int inline)
                uint32_t sz = entries[i].file_size;
                if (sz == 0) vga_print("0");
                else {
                    char b[16]; int bi=0;
                    while(sz>0){ b[bi++]=(sz%10)+'0'; sz/=10; }
                    while(bi>0) vga_putchar(b[--bi]);
                }
                vga_print(" bytes\n");
            }
            count++;
        }
    }
done:
    if (count == 0) vga_print("Nenhum arquivo encontrado no HD.\n");
    vga_print("\n");
}

int fat16_read_file(const char* filename, uint8_t* buffer) {
    char target_name[11];
    format_fat_name(filename, target_name);
    
    for (uint32_t sec = 0; sec < root_dir_size; sec++) {
        uint8_t root_sec[512];
        ata_read_sector(root_dir_sector + sec, root_sec);
        struct fat16_dir_entry* entries = (struct fat16_dir_entry*)root_sec;
        for (int i = 0; i < 16; i++) {
            if (entries[i].name[0] == 0x00) return 0; // Não encontrou
            if ((unsigned char)entries[i].name[0] == 0xE5) continue;
            
            int match = 1;
            for(int j=0; j<11; j++) {
                char c = (j < 8) ? entries[i].name[j] : entries[i].ext[j-8];
                if(c != target_name[j]) match = 0;
            }
            if (match && !(entries[i].attributes & 0x10)) {
                uint16_t cluster = entries[i].first_cluster;
                uint32_t size = entries[i].file_size;
                uint32_t bytes_read = 0;
                
                while(cluster >= 2 && cluster <= 0xFFEF) {
                    uint32_t sector = data_sector + ((cluster - 2) * bpb.sectors_per_cluster);
                    for(int c=0; c<bpb.sectors_per_cluster; c++) {
                        uint8_t temp_sec[512];
                        ata_read_sector(sector + c, temp_sec);
                        
                        uint32_t to_copy = size - bytes_read;
                        if (to_copy > 512) to_copy = 512;
                        
                        for(uint32_t k=0; k<to_copy; k++) buffer[bytes_read + k] = temp_sec[k];
                        
                        bytes_read += to_copy;
                        if (bytes_read >= size) break;
                    }
                    if (bytes_read >= size) break;
                    cluster = fat16_get_fat_entry(cluster);
                }
                return size;
            }
        }
    }
    return 0; // File not found
}

uint32_t fat16_get_file_size(const char* filename) {
    char target_name[11];
    format_fat_name(filename, target_name);
    
    for (uint32_t sec = 0; sec < root_dir_size; sec++) {
        uint8_t root_sec[512];
        ata_read_sector(root_dir_sector + sec, root_sec);
        struct fat16_dir_entry* entries = (struct fat16_dir_entry*)root_sec;
        for (int i = 0; i < 16; i++) {
            if (entries[i].name[0] == 0x00) return 0;
            if ((unsigned char)entries[i].name[0] == 0xE5) continue;
            
            int match = 1;
            for(int j=0; j<11; j++) {
                char c = (j < 8) ? entries[i].name[j] : entries[i].ext[j-8];
                if(c != target_name[j]) match = 0;
            }
            if (match && !(entries[i].attributes & 0x10)) {
                return entries[i].file_size;
            }
        }
    }
    return 0;
}


int fat16_write_file(const char* filename, const uint8_t* buffer, uint32_t size) {
    char target_name[11];
    format_fat_name(filename, target_name);
    
    uint32_t target_sec = 0;
    int target_idx = -1;
    
    uint8_t root_sec[512];
    
    // Passo 1: Procurar se o arquivo já existe ou achar um espaço livre
    for (uint32_t sec = 0; sec < root_dir_size; sec++) {
        ata_read_sector(root_dir_sector + sec, root_sec);
        struct fat16_dir_entry* entries = (struct fat16_dir_entry*)root_sec;
        for (int i = 0; i < 16; i++) {
            if (entries[i].name[0] == 0x00 || (unsigned char)entries[i].name[0] == 0xE5) {
                if (target_idx == -1) { // Salva o primeiro slot livre
                    target_sec = sec;
                    target_idx = i;
                }
                if (entries[i].name[0] == 0x00) break; // Chegou no fim
            } else {
                int match = 1;
                for(int j=0; j<11; j++) {
                    char c = (j < 8) ? entries[i].name[j] : entries[i].ext[j-8];
                    if(c != target_name[j]) match = 0;
                }
                if (match) {
                    // Arquivo existe! Vamos sobrescrever
                    target_sec = sec;
                    target_idx = i;
                    // Liberar clusters antigos
                    uint16_t c = entries[i].first_cluster;
                    while(c >= 2 && c <= 0xFFEF) {
                        uint16_t next = fat16_get_fat_entry(c);
                        fat16_set_fat_entry(c, 0);
                        c = next;
                    }
                    goto slot_found;
                }
            }
        }
    }
    
slot_found:
    if (target_idx == -1) return 0; // Diretório raiz cheio
    
    // Passo 2: Alocar clusters e gravar dados
    uint16_t first_cluster = 0;
    uint16_t current_cluster = 0;
    uint32_t bytes_written = 0;
    
    while(bytes_written < size || (size == 0 && first_cluster == 0)) {
        uint16_t new_c = fat16_alloc_cluster();
        if (new_c == 0) {
            // Rollback de seguranca: Disco cheio no meio da escrita
            if (first_cluster != 0) {
                uint16_t cl = first_cluster;
                while(cl >= 2 && cl <= 0xFFEF) {
                    uint16_t next = fat16_get_fat_entry(cl);
                    fat16_set_fat_entry(cl, 0);
                    cl = next;
                }
            }
            return 0; // Aborta e retorna 0 sem corromper o FAT
        }
        
        if (first_cluster == 0) first_cluster = new_c;
        else fat16_set_fat_entry(current_cluster, new_c);
        
        current_cluster = new_c;
        
        // Gravar no disco
        uint32_t sector = data_sector + ((current_cluster - 2) * bpb.sectors_per_cluster);
        for(int c=0; c<bpb.sectors_per_cluster; c++) {
            uint8_t write_buf[512] = {0};
            for(int k=0; k<512 && bytes_written < size; k++) {
                write_buf[k] = buffer[bytes_written++];
            }
            ata_write_sector(sector + c, write_buf);
        }
        
        if (size == 0) break; // Arquivo vazio
    }
    
    // Passo 3: Atualizar entrada no diretório raiz
    ata_read_sector(root_dir_sector + target_sec, root_sec);
    struct fat16_dir_entry* entries = (struct fat16_dir_entry*)root_sec;
    for(int j=0; j<8; j++) entries[target_idx].name[j] = target_name[j];
    for(int j=0; j<3; j++) entries[target_idx].ext[j] = target_name[8+j];
    entries[target_idx].attributes = 0x20; // Archive
    entries[target_idx].first_cluster = first_cluster;
    entries[target_idx].file_size = size;
    ata_write_sector(root_dir_sector + target_sec, root_sec);
    
    return size;
}

int fat16_delete_file(const char* filename) {
    char target_name[11];
    format_fat_name(filename, target_name);
    
    for (uint32_t sec = 0; sec < root_dir_size; sec++) {
        uint8_t root_sec[512];
        ata_read_sector(root_dir_sector + sec, root_sec);
        struct fat16_dir_entry* entries = (struct fat16_dir_entry*)root_sec;
        for (int i = 0; i < 16; i++) {
            if (entries[i].name[0] == 0x00) return 0; // Fim real do diretório
            if ((unsigned char)entries[i].name[0] == 0xE5) continue;
            
            int match = 1;
            for(int j=0; j<11; j++) {
                char c = (j < 8) ? entries[i].name[j] : entries[i].ext[j-8];
                if(c != target_name[j]) match = 0;
            }
            if (match && !(entries[i].attributes & 0x10)) {
                // Liberar clusters
                uint16_t cluster = entries[i].first_cluster;
                while(cluster >= 2 && cluster <= 0xFFEF) {
                    uint16_t next = fat16_get_fat_entry(cluster);
                    fat16_set_fat_entry(cluster, 0); // Libera o cluster
                    cluster = next;
                }
                
                // Marca como deletado na entrada de diretorio (0xE5)
                entries[i].name[0] = 0xE5;
                ata_write_sector(root_dir_sector + sec, root_sec);
                return 1; // Sucesso
            }
        }
    }
    return 0; // Arquivo não encontrado
}
