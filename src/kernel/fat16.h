#ifndef FAT16_H
#define FAT16_H

#include <stdint.h>
#include <stddef.h>

struct fat_extBS_16 {
    uint8_t     bios_drive_num;
    uint8_t     reserved1;
    uint8_t     boot_signature;
    uint32_t    volume_id;
    char        volume_label[11];
    char        fat_type_label[8];
} __attribute__((packed));

struct fat_BS {
    uint8_t     bootjmp[3];
    char        oem_name[8];
    uint16_t    bytes_per_sector;
    uint8_t     sectors_per_cluster;
    uint16_t    reserved_sector_count;
    uint8_t     table_count;
    uint16_t    root_entry_count;
    uint16_t    total_sectors_16;
    uint8_t     media_type;
    uint16_t    table_size_16;
    uint16_t    sectors_per_track;
    uint16_t    head_side_count;
    uint32_t    hidden_sector_count;
    uint32_t    total_sectors_32;
    
    struct fat_extBS_16 ext;
    
    uint8_t     boot_code[448];
    uint16_t    boot_sign;
} __attribute__((packed));

struct fat16_dir_entry {
    char name[8];
    char ext[3];
    uint8_t attributes;
    uint8_t reserved;
    uint8_t creation_time_tenths;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t high_cluster; // Sempre 0
    uint16_t last_mod_time;
    uint16_t last_mod_date;
    uint16_t first_cluster;
    uint32_t file_size;
} __attribute__((packed));

void fat16_init(void);
void fat16_dir(void);
int fat16_read_file(const char* filename, uint8_t* buffer);
int fat16_write_file(const char* filename, const uint8_t* buffer, uint32_t size);
int fat16_delete_file(const char* filename);
uint32_t fat16_get_file_size(const char* filename);

#endif
