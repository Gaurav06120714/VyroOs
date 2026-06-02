#include "net_io.h"
#include "net.h"
#include "../drivers/rtl8139.h"

// Simple RX queue (filled in IRQ context, drained from main thread)
static netio_pkt_t rxq[NETIO_RX_QUEUE];
static volatile uint32_t rxq_head = 0;
static volatile uint32_t rxq_tail = 0;
static volatile uint64_t rx_count = 0;
static volatile uint64_t tx_count = 0;

static void rx_handler(const uint8_t* frame, uint16_t len) {
    if (len > NETIO_RX_MAX_FRAME) return;
    uint32_t next = (rxq_tail + 1) % NETIO_RX_QUEUE;
    if (next == rxq_head) return;   // queue full, drop
    netio_pkt_t* p = &rxq[rxq_tail];
    p->len = len;
    for (int i = 0; i < len; i++) p->data[i] = frame[i];
    rxq_tail = next;
    rx_count++;
}

void net_io_init() {
    rxq_head = rxq_tail = 0;
    rtl8139_set_rx_handler(rx_handler);
}

int net_io_poll(netio_pkt_t* out) {
    if (rxq_head == rxq_tail) return 0;
    netio_pkt_t* _src = &rxq[rxq_head]; out->len = _src->len; for (uint16_t i = 0; i < _src->len; i++) out->data[i] = _src->data[i];
    rxq_head = (rxq_head + 1) % NETIO_RX_QUEUE;
    return 1;
}

uint64_t net_io_rx_count() { return rx_count; }
uint64_t net_io_tx_count() { return tx_count; }

// ─────────────────────────────────────────────────
// UDP/IP/Ethernet frame builder
// ─────────────────────────────────────────────────

// UDP pseudo + real header (8 bytes)
typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;       // header + data
    uint16_t checksum;     // optional in IPv4
} __attribute__((packed)) udp_header_t;

static int build_and_send(const uint8_t dst_mac[6], const uint8_t dst_ip[4],
                          uint16_t src_port, uint16_t dst_port,
                          const uint8_t* payload, uint16_t plen) {
    static uint8_t frame[1600];
    if (plen + 14 + 20 + 8 > sizeof(frame)) return -1;

    eth_header_t* eh = (eth_header_t*) frame;
    for (int i = 0; i < 6; i++) { eh->dst[i] = dst_mac[i]; eh->src[i] = net_mac()[i]; }
    eh->ethertype = htons(ETHERTYPE_IPV4);

    ipv4_header_t* ip = (ipv4_header_t*)(frame + 14);
    ip->ver_ihl   = 0x45;
    ip->tos       = 0;
    ip->total_len = htons(20 + 8 + plen);
    ip->id        = htons(1);
    ip->flags_frag = 0;
    ip->ttl       = 64;
    ip->protocol  = 17;       // UDP
    for (int i = 0; i < 4; i++) ip->src[i] = net_ip()[i];
    for (int i = 0; i < 4; i++) ip->dst[i] = dst_ip[i];
    ip->checksum  = 0;
    ip->checksum  = htons(net_checksum(ip, 20));

    udp_header_t* udp = (udp_header_t*)((uint8_t*)ip + 20);
    udp->src_port = htons(src_port);
    udp->dst_port = htons(dst_port);
    udp->length   = htons(8 + plen);
    udp->checksum = 0;        // optional for IPv4

    uint8_t* dst_payload = (uint8_t*)udp + 8;
    for (int i = 0; i < plen; i++) dst_payload[i] = payload[i];

    int total = 14 + 20 + 8 + plen;
    int sent = rtl8139_send(frame, (uint16_t)total);
    if (sent > 0) tx_count++;
    return sent;
}

int net_io_send_udp_bcast(uint16_t src_port, uint16_t dst_port,
                          const uint8_t* payload, uint16_t plen) {
    static uint8_t bcast_mac[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    static uint8_t bcast_ip[4]  = { 0xFF, 0xFF, 0xFF, 0xFF };

    // Temporarily use 0.0.0.0 as source for DHCP DISCOVER
    // (we don't have an IP yet)
    extern const uint8_t* net_ip();
    // Save and zero our IP for this transmission
    static uint8_t saved_ip[4];
    for (int i = 0; i < 4; i++) saved_ip[i] = net_ip()[i];

    // Hack: build frame manually with src=0.0.0.0
    static uint8_t frame[1600];
    if (plen + 14 + 20 + 8 > sizeof(frame)) return -1;
    eth_header_t* eh = (eth_header_t*) frame;
    for (int i = 0; i < 6; i++) { eh->dst[i] = 0xFF; eh->src[i] = net_mac()[i]; }
    eh->ethertype = htons(ETHERTYPE_IPV4);
    ipv4_header_t* ip = (ipv4_header_t*)(frame + 14);
    ip->ver_ihl   = 0x45;
    ip->tos       = 0;
    ip->total_len = htons(20 + 8 + plen);
    ip->id        = htons(1);
    ip->flags_frag = 0;
    ip->ttl       = 64;
    ip->protocol  = 17;
    for (int i = 0; i < 4; i++) ip->src[i] = 0;
    for (int i = 0; i < 4; i++) ip->dst[i] = 0xFF;
    ip->checksum  = 0;
    ip->checksum  = htons(net_checksum(ip, 20));
    udp_header_t* udp = (udp_header_t*)((uint8_t*)ip + 20);
    udp->src_port = htons(src_port);
    udp->dst_port = htons(dst_port);
    udp->length   = htons(8 + plen);
    udp->checksum = 0;
    uint8_t* dst_payload = (uint8_t*)udp + 8;
    for (int i = 0; i < plen; i++) dst_payload[i] = payload[i];
    int total = 14 + 20 + 8 + plen;
    int sent = rtl8139_send(frame, (uint16_t)total);
    if (sent > 0) tx_count++;
    (void)bcast_mac; (void)bcast_ip; (void)saved_ip;
    return sent;
}

int net_io_send_udp(const uint8_t dst_ip[4], uint16_t src_port, uint16_t dst_port,
                    const uint8_t* payload, uint16_t plen) {
    // Without an ARP cache yet, send to the broadcast MAC — QEMU's user-mode
    // stack accepts it for the local segment. Real ARP would resolve dst_ip → mac.
    static uint8_t bcast_mac[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    return build_and_send(bcast_mac, dst_ip, src_port, dst_port, payload, plen);
}
