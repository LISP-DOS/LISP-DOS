#include "ata.h"
#include "io.h"

// Espera até que o disco esteja pronto (BSY limpo e RDY setado)
static void ata_wait_ready(void) {
    while (1) {
        uint8_t status = inb(ATA_PRIMARY_STATUS);
        if ((status & 0x80) == 0 && (status & 0x40) != 0) {
            break;
        }
        if (status & 0x01) { // ERR (Error bit)
            extern void vga_print(const char*);
            vga_print("ATA: Erro de Hardware ou I/O invalido detectado.\n");
            break;
        }
    }
}

void ata_read_sector(uint32_t lba, uint8_t *buffer) {
    ata_wait_ready();
    
    outb(ATA_PRIMARY_DRV_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_SECCOUNT, 1);
    outb(ATA_PRIMARY_LBA_LO, (uint8_t)(lba));
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_LBA_HI, (uint8_t)(lba >> 16));
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_READ_PIO);
    
    ata_wait_ready();
    
    // Ler 256 palavras (512 bytes)
    insw(ATA_PRIMARY_DATA, buffer, 256);
}

void ata_write_sector(uint32_t lba, uint8_t *buffer) {
    ata_wait_ready();
    
    outb(ATA_PRIMARY_DRV_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_SECCOUNT, 1);
    outb(ATA_PRIMARY_LBA_LO, (uint8_t)(lba));
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_LBA_HI, (uint8_t)(lba >> 16));
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_WRITE_PIO);
    
    ata_wait_ready();
    
    // Escrever 256 palavras (512 bytes)
    outsw(ATA_PRIMARY_DATA, buffer, 256);
    
    // Limpar cache (Flush)
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_CACHE_FLUSH);
    ata_wait_ready();
}
