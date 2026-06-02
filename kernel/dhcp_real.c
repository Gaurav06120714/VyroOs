#include "dhcp_real.h"
#include "net_io.h"
#include "net.h"
#include "../drivers/rtl8139.h"
#include "../drivers/timer.h"

#define DHCP_MAGIC 0x63825363

static uint8_t got_ip[4]  = {0,0,0,0};
static uint8_t got_gw[4]  = {0,0,0,0};
static uint8_t got_dns[4] = {0,0,0,0};

static uint32_t make_xid() {
    return 0xA0B0C0D0 ^ (uint32_t)timer_ticks();
}

static int parse_dhcp_reply(const netio_pkt_t* pkt, uint32_t xid) {
    if (pkt->len < 14 + 20 + 8 + 240) return 0;
    // Verify Eth=IPv4, IPv4 proto=UDP, UDP dst=68
    if (!(pkt->data[12] == 0x08 && pkt->data[13] == 0x00)) return 0;
    if (pkt->data[14 + 9] != 17) return 0;
    uint16_t udp_dst = ((uint16_t)pkt->data[14 + 20 + 2] << 8) | pkt->data[14 + 20 + 3];
    if (udp_dst != 68) return 0;

    const uint8_t* dhcp = pkt->data + 14 + 20 + 8;
    // BOOTP op == 2 (reply)
    if (dhcp[0] != 2) return 0;
    // xid match (bytes 4..7)
    uint32_t reply_xid = ((uint32_t)dhcp[4]<<24)|((uint32_t)dhcp[5]<<16)|((uint32_t)dhcp[6]<<8)|dhcp[7];
    if (reply_xid != xid) return 0;
    // yiaddr at offset 16
    for (int i = 0; i < 4; i++) got_ip[i] = dhcp[16 + i];
    // siaddr at offset 20 (server)
    for (int i = 0; i < 4; i++) got_gw[i] = dhcp[20 + i];

    // Walk options (start at 240 after BOOTP+magic)
    int o = 240;
    int end = pkt->len - (14 + 20 + 8);
    while (o < end) {
        uint8_t code = dhcp[o++];
        if (code == 0) continue;
        if (code == 255) break;
        if (o >= end) break;
        uint8_t len = dhcp[o++];
        if (code == 3 && len >= 4) {            // Router
            for (int i = 0; i < 4; i++) got_gw[i] = dhcp[o + i];
        } else if (code == 6 && len >= 4) {     // DNS server
            for (int i = 0; i < 4; i++) got_dns[i] = dhcp[o + i];
        }
        o += len;
    }
    return 1;
}

int dhcp_real_run(uint32_t timeout_ms) {
    uint32_t xid = make_xid();
    static uint8_t pkt[300];
    for (int i = 0; i < (int)sizeof(pkt); i++) pkt[i] = 0;

    pkt[0] = 1;            // op = BOOTREQUEST
    pkt[1] = 1;            // htype = Ethernet
    pkt[2] = 6;            // hlen
    pkt[4] = (xid >> 24) & 0xFF;
    pkt[5] = (xid >> 16) & 0xFF;
    pkt[6] = (xid >>  8) & 0xFF;
    pkt[7] =  xid        & 0xFF;
    // chaddr at offset 28
    const uint8_t* mac = net_mac();
    for (int i = 0; i < 6; i++) pkt[28 + i] = mac[i];
    // Magic cookie at 236
    pkt[236] = 0x63; pkt[237] = 0x82; pkt[238] = 0x53; pkt[239] = 0x63;
    // Options: DHCP message type = 1 (DISCOVER)
    pkt[240] = 53; pkt[241] = 1; pkt[242] = 1;
    // Parameter request list: router, DNS
    pkt[243] = 55; pkt[244] = 2; pkt[245] = 3; pkt[246] = 6;
    // End
    pkt[247] = 255;

    if (net_io_send_udp_bcast(68, 67, pkt, 248) <= 0) return 0;

    // Wait up to timeout_ms for OFFER
    uint64_t deadline = timer_uptime_ms() + timeout_ms;
    while (timer_uptime_ms() < deadline) {
        netio_pkt_t in;
        while (net_io_poll(&in)) {
            if (parse_dhcp_reply(&in, xid)) return 1;
        }
        __asm__ volatile("hlt");
    }
    return 0;
}

const uint8_t* dhcp_real_ip()  { return got_ip; }
const uint8_t* dhcp_real_gw()  { return got_gw; }
const uint8_t* dhcp_real_dns() { return got_dns; }
