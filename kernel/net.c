#include "net.h"
#include "../drivers/rtl8139.h"

// Vyro OS network identity. We try to inherit the NIC's real MAC at init time.
static uint8_t my_mac[ETH_ALEN] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
static uint8_t my_ip[4]         = { 10, 0, 2, 15 };   // QEMU default guest IP

void net_init() {
    const uint8_t* nic_mac = rtl8139_mac();
    if (nic_mac) {
        int nonzero = 0;
        for (int i = 0; i < 6; i++) if (nic_mac[i]) nonzero = 1;
        if (nonzero) for (int i = 0; i < 6; i++) my_mac[i] = nic_mac[i];
    }
}

const uint8_t* net_mac() { return my_mac; }
const uint8_t* net_ip()  { return my_ip; }

// ─────────────────────────────────────────────────
// net_checksum: the standard internet checksum (RFC 1071)
// Used by IPv4 and ICMP. Sum 16-bit words, fold carries,
// return one's complement.
// ─────────────────────────────────────────────────
uint16_t net_checksum(const void* data, uint32_t len) {
    const uint8_t* p = (const uint8_t*) data;
    uint32_t sum = 0;

    while (len > 1) {
        uint16_t word = (uint16_t)((p[0] << 8) | p[1]);
        sum += word;
        p   += 2;
        len -= 2;
    }
    if (len == 1) {
        sum += (uint16_t)(p[0] << 8);
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)(~sum);
}
