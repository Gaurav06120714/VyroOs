#include "ahci.h"
#include "pci.h"

static ahci_info_t info;

#define HBA_CAP     0x00
#define HBA_GHC     0x04
#define HBA_IS      0x08
#define HBA_PI      0x0C
#define HBA_VS      0x10
#define HBA_PORT0   0x100
#define HBA_PORTSIZE 0x80

#define PORT_CMD    0x18
#define PORT_TFD    0x20
#define PORT_SIG    0x24
#define PORT_SSTS   0x28

#define GHC_AE      (1u << 31)   // AHCI enable

static inline uint32_t mmio_r32(uint64_t addr) { return *(volatile uint32_t*)addr; }
static inline void     mmio_w32(uint64_t addr, uint32_t v) { *(volatile uint32_t*)addr = v; }

int ahci_init(void) {
    for (uint32_t i = 0; i < sizeof(info); i++) ((uint8_t*)&info)[i] = 0;

    // Find an AHCI controller via PCI: class 0x01, subclass 0x06, prog-if 0x01
    pci_device_t* dev = pci_find_class(0x01, 0x06);
    if (!dev) return 0;
    // pci_find_class doesn't filter prog_if — verify
    if (dev->prog_if != 0x01) return 0;

    // ABAR is BAR5. The pci_device_t struct only stores bar0 in our minimal
    // PCI; read BAR5 directly via config space.
    extern uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off);
    uint32_t bar5 = pci_config_read(dev->bus, dev->slot, dev->func, 0x24);
    info.mmio_base = bar5 & ~0xFULL;
    if (!info.mmio_base) return 0;

    // Enable AHCI in GHC
    uint32_t ghc = mmio_r32(info.mmio_base + HBA_GHC);
    mmio_w32(info.mmio_base + HBA_GHC, ghc | GHC_AE);

    info.capabilities          = mmio_r32(info.mmio_base + HBA_CAP);
    info.port_implemented_mask = mmio_r32(info.mmio_base + HBA_PI);
    info.version               = mmio_r32(info.mmio_base + HBA_VS);
    info.num_ports             = ((info.capabilities >> 0)  & 0x1F) + 1;
    info.num_command_slots     = ((info.capabilities >> 8)  & 0x1F) + 1;
    info.supports_64bit        =  (info.capabilities >> 31) & 1;
    info.present               = 1;
    return 1;
}

const ahci_info_t* ahci_info(void) { return &info; }

uint32_t ahci_active_ports(void) {
    if (!info.present) return 0;
    uint32_t active = 0;
    for (uint32_t i = 0; i < info.num_ports; i++) {
        if (!(info.port_implemented_mask & (1u << i))) continue;
        uint64_t port_base = info.mmio_base + HBA_PORT0 + i * HBA_PORTSIZE;
        uint32_t ssts = mmio_r32(port_base + PORT_SSTS);
        uint8_t det = ssts & 0xF;
        uint8_t ipm = (ssts >> 8) & 0xF;
        // det=3 (device present, PHY established) and ipm=1 (active)
        if (det == 3 && ipm == 1) active |= (1u << i);
    }
    return active;
}
