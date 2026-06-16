#include "acpi.h"
#include <limine.h>
#include "io.h"
#include "vga.h"
#include "../kernel/pmm.h"
#include <stddef.h>

__attribute__((used, section(".requests")))
volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST,
    .revision = 0
};

struct RSDPDescriptor {
    char Signature[8];
    uint8_t Checksum;
    char OEMID[6];
    uint8_t Revision;
    uint32_t RsdtAddress;
} __attribute__ ((packed));

struct ACPISDTHeader {
    char Signature[4];
    uint32_t Length;
    uint8_t Revision;
    uint8_t Checksum;
    char OEMID[6];
    char OEMTableID[8];
    uint32_t OEMRevision;
    uint32_t CreatorID;
    uint32_t CreatorRevision;
} __attribute__ ((packed));

struct FADT {
    struct ACPISDTHeader h;
    uint32_t FirmwareCtrl;
    uint32_t Dsdt;
    uint8_t  Reserved;
    uint8_t  PreferredPowerManagementProfile;
    uint16_t SCI_Interrupt;
    uint32_t SMI_CommandPort;
    uint8_t  AcpiEnable;
    uint8_t  AcpiDisable;
    uint8_t  S4BIOS_REQ;
    uint8_t  PSTATE_Control;
    uint32_t PM1aEventBlock;
    uint32_t PM1bEventBlock;
    uint32_t PM1aControlBlock;
    uint32_t PM1bControlBlock;
    uint32_t PM2ControlBlock;
    uint32_t PMTimerBlock;
    uint32_t GPE0Block;
    uint32_t GPE1Block;
    uint8_t  PM1EventLength;
    uint8_t  PM1ControlLength;
    uint8_t  PM2ControlLength;
    uint8_t  PMTimerLength;
    uint8_t  GPE0Length;
    uint8_t  GPE1Length;
    uint8_t  GPE1Base;
    uint8_t  CStateControl;
    uint16_t WorstC2Latency;
    uint16_t WorstC3Latency;
    uint16_t FlushSize;
    uint16_t FlushStride;
    uint8_t  DutyOffset;
    uint8_t  DutyWidth;
    uint8_t  DayAlarm;
    uint8_t  MonthAlarm;
    uint8_t  Century;
    uint16_t BootArchitectureFlags;
    uint8_t  Reserved2;
    uint32_t Flags;
} __attribute__ ((packed));

static uint32_t pm1a_cnt_blk = 0;
static uint16_t slp_typa = 0;

static int strncmp(const char *s1, const char *s2, int n) {
    for (int i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return s1[i] - s2[i];
    }
    return 0;
}

void acpi_init(void) {
    if (rsdp_request.response == NULL) {
        vga_print("ACPI: RSDP nao encontrado pelo Limine.\n");
        return;
    }
    
    struct RSDPDescriptor *rsdp = (struct RSDPDescriptor *)rsdp_request.response->address;
    struct ACPISDTHeader *rsdt = (struct ACPISDTHeader *)phystokv(rsdp->RsdtAddress);
    
    int entries = (rsdt->Length - sizeof(struct ACPISDTHeader)) / 4;
    uint32_t *ptrs = (uint32_t *)((uintptr_t)rsdt + sizeof(struct ACPISDTHeader));
    
    struct FADT *fadt = NULL;
    
    for (int i = 0; i < entries; i++) {
        struct ACPISDTHeader *h = (struct ACPISDTHeader *)phystokv(ptrs[i]);
        if (strncmp(h->Signature, "FACP", 4) == 0) {
            fadt = (struct FADT *)h;
            break;
        }
    }
    
    if (!fadt) {
        vga_print("ACPI: FADT nao encontrado.\n");
        return;
    }
    
    // Ativa ACPI via SMI
    if (fadt->SMI_CommandPort != 0 && fadt->AcpiEnable != 0) {
        outb((uint16_t)fadt->SMI_CommandPort, fadt->AcpiEnable);
    }
    
    pm1a_cnt_blk = fadt->PM1aControlBlock;
    
    struct ACPISDTHeader *dsdt = (struct ACPISDTHeader *)phystokv(fadt->Dsdt);
    if (dsdt) {
        uint8_t *s5Addr = (uint8_t *)dsdt;
        int len = dsdt->Length;
        for (int i = 0; i < len - 4; i++) {
            if (s5Addr[i] == '_' && s5Addr[i+1] == 'S' && s5Addr[i+2] == '5' && s5Addr[i+3] == '_') {
                if ((i > 0 && s5Addr[i-1] == 0x08) || (i > 1 && s5Addr[i-2] == 0x08 && s5Addr[i-1] == '\\')) {
                    s5Addr += i + 4;
                    if (s5Addr[0] == 0x12) {
                        s5Addr++;
                        s5Addr++;
                        s5Addr++;
                        if (s5Addr[0] == 0x0A) s5Addr++;
                        slp_typa = *(s5Addr) << 10;
                        break;
                    }
                }
            }
        }
    }
    
    if (slp_typa == 0) slp_typa = (5 << 10);
}

void acpi_poweroff(void) {
    if (pm1a_cnt_blk == 0) {
        outw(0x604, 0x2000);
        outw(0xB004, 0x2000);
        return;
    }
    
    outw((uint16_t)pm1a_cnt_blk, slp_typa | 0x2000);
}
