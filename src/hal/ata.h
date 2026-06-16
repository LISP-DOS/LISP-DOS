#ifndef ATA_H
#define ATA_H

#include <stdint.h>

#define ATA_PRIMARY_DATA         0x1F0
#define ATA_PRIMARY_ERR          0x1F1
#define ATA_PRIMARY_SECCOUNT     0x1F2
#define ATA_PRIMARY_LBA_LO       0x1F3
#define ATA_PRIMARY_LBA_MID      0x1F4
#define ATA_PRIMARY_LBA_HI       0x1F5
#define ATA_PRIMARY_DRV_HEAD     0x1F6
#define ATA_PRIMARY_STATUS       0x1F7
#define ATA_PRIMARY_COMMAND      0x1F7

#define ATA_CMD_READ_PIO         0x20
#define ATA_CMD_WRITE_PIO        0x30
#define ATA_CMD_CACHE_FLUSH      0xE7

// Lê um setor (512 bytes) do disco usando LBA28
void ata_read_sector(uint32_t lba, uint8_t *buffer);

// Escreve um setor (512 bytes) no disco usando LBA28
void ata_write_sector(uint32_t lba, uint8_t *buffer);

#endif
