#include "rtl8139.h"
#include "pic.h"
#include "../kernel/pci.h"
#include "../kernel/heap.h"
#include "../kernel/isr.h"

// I/O port helpers (16/32-bit variants in addition to byte-level outb/inb)
static inline void outw_p(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint16_t inw_p(uint16_t port) {
    uint16_t r; __asm__ volatile("inw %1, %0" : "=a"(r) : "Nd"(port)); return r;
}
static inline void outl_p(uint16_t port, uint32_t val) {
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint32_t inl_p(uint16_t port) {
    uint32_t r; __asm__ volatile("inl %1, %0" : "=a"(r) : "Nd"(port)); return r;
}

// Register offsets from BAR0 I/O base
#define R_MAC0     0x00
#define R_TSD0     0x10
#define R_TSAD0    0x20
#define R_RBSTART  0x30
#define R_CMD      0x37
#define R_CAPR     0x38
#define R_IMR      0x3C
#define R_ISR      0x3E
#define R_TCR      0x40
#define R_RCR      0x44
#define R_CONFIG1  0x52

#define RX_BUF_SIZE 8192

static uint16_t       io_base   = 0;
static uint8_t        my_mac[6] = { 0 };
static uint8_t*       rx_buf    = 0;
static uint32_t       rx_off    = 0;
static uint8_t*       tx_buf[4] = { 0 };
static int            tx_slot   = 0;
static rtl_rx_fn      rx_cb     = 0;
static volatile uint64_t tx_total = 0, rx_total = 0;

static void rtl_irq(registers_t* regs) {
    (void)regs;
    uint16_t isr = inw_p(io_base + R_ISR);
    outw_p(io_base + R_ISR, isr);   // ack all bits

    if (isr & 0x01) {
        // RX OK — drain ring
        while (!(inb(io_base + R_CMD) & 0x01)) {   // bit 0 = BUFE (empty)
            uint8_t* p   = rx_buf + rx_off;
            uint16_t status = ((uint16_t)p[1] << 8) | p[0];
            uint16_t length = ((uint16_t)p[3] << 8) | p[2];
            if (!(status & 1)) break;             // not OK
            if (length < 14 || length > 1518) break;
            if (rx_cb) rx_cb(p + 4, length - 4);  // strip 4-byte FCS
            rx_total++;
            // advance: header (4) + length, 4-byte aligned
            rx_off = (rx_off + length + 4 + 3) & ~3;
            if (rx_off >= RX_BUF_SIZE) rx_off -= RX_BUF_SIZE;
            outw_p(io_base + R_CAPR, (uint16_t)(rx_off - 0x10));
        }
    }
}

int rtl8139_init() {
    pci_device_t* nic = 0;
    int n = (int)pci_device_count();
    for (int i = 0; i < n; i++) {
        pci_device_t* d = pci_get_device(i);
        if (d->vendor_id == RTL_VENDOR && d->device_id == RTL_DEVICE) { nic = d; break; }
    }
    if (!nic) return 0;
    io_base = (uint16_t)(nic->bar0 & ~0x3);
    if (!io_base) return 0;

    // Enable bus mastering in PCI command register (offset 0x04, bit 2)
    uint32_t cmd = pci_config_read(nic->bus, nic->slot, nic->func, 0x04);
    cmd |= 0x04;
    // PCI write is via the same mechanism; reuse the address-set + outl.
    uint32_t addr = (uint32_t)((nic->bus << 16) | (nic->slot << 11) | (nic->func << 8) |
                               (0x04 & 0xFC) | 0x80000000);
    outl_p(0xCF8, addr);
    outl_p(0xCFC, cmd);

    // Power on
    outb(io_base + R_CONFIG1, 0x00);

    // Software reset
    outb(io_base + R_CMD, 0x10);
    int spin = 0;
    while ((inb(io_base + R_CMD) & 0x10) && spin++ < 1000000);

    // Read MAC address from registers 0x00..0x05
    for (int i = 0; i < 6; i++) my_mac[i] = inb(io_base + R_MAC0 + i);

    // Allocate RX ring (RBSTART buffer + 16 bytes + 1500 MTU slop)
    rx_buf = (uint8_t*) kmalloc(RX_BUF_SIZE + 16 + 1500);
    if (!rx_buf) return 0;
    rx_off = 0;
    outl_p(io_base + R_RBSTART, (uint32_t)(uint64_t)rx_buf);

    // Allocate 4 TX buffers (one per descriptor)
    for (int i = 0; i < 4; i++) {
        tx_buf[i] = (uint8_t*) kmalloc(2048);
        if (!tx_buf[i]) return 0;
    }
    tx_slot = 0;

    // Enable RX OK + TX OK interrupts (Transmit OK = 0x0004, Receive OK = 0x0001)
    outw_p(io_base + R_IMR, 0x0005);

    // Receive config:
    //   AAP=1 (accept all), APM=1 (my MAC), AM=1 (multicast), AB=1 (broadcast)
    //   WRAP=1, MaxDMA=1024B (6 << 8), no early RX, RBLEN=8K (0 << 11)
    outl_p(io_base + R_RCR, 0x0000000F | (1 << 7) | (6 << 8));

    // Transmit config: standard
    outl_p(io_base + R_TCR, (6 << 8));

    // Enable RX and TX in CMD
    outb(io_base + R_CMD, 0x0C);

    // Hook IRQ
    uint8_t irq_line = nic->irq_line;
    if (irq_line < 16) {
        irq_register(irq_line, rtl_irq);
        pic_unmask_irq(irq_line);
        // The NIC sits behind the slave PIC on most setups
        if (irq_line >= 8) pic_unmask_irq(2);
    }
    return 1;
}

int rtl8139_send(const uint8_t* frame, uint16_t len) {
    if (!io_base || !tx_buf[tx_slot]) return -1;
    if (len < 60) len = 60;             // pad to minimum Ethernet size
    if (len > 1792) return -2;

    for (int i = 0; i < len; i++) tx_buf[tx_slot][i] = frame[i];
    for (int i = len; i < 60; i++)   tx_buf[tx_slot][i] = 0;

    outl_p(io_base + R_TSAD0 + tx_slot*4, (uint32_t)(uint64_t)tx_buf[tx_slot]);
    // TSD: writing length (low 13 bits) starts transmission; clear OWN bit (bit 13=0)
    outl_p(io_base + R_TSD0  + tx_slot*4, len & 0x1FFF);

    tx_slot = (tx_slot + 1) % 4;
    tx_total++;
    return len;
}

const uint8_t* rtl8139_mac()    { return my_mac; }
uint64_t       rtl8139_tx_count() { return tx_total; }
uint64_t       rtl8139_rx_count() { return rx_total; }
void rtl8139_set_rx_handler(rtl_rx_fn fn) { rx_cb = fn; }
