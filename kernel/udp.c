#include "udp.h"
#include "net.h"
#include "arp.h"
#include "../drivers/rtl8139.h"

#define UDP_PORT_TABLE_SIZE 16
#define UDP_PROTO           17

typedef struct {
    uint16_t  port;
    udp_rx_fn cb;
    uint8_t   active;
} udp_listener_t;

static udp_listener_t listeners[UDP_PORT_TABLE_SIZE];

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed)) udp_header_t;

void udp_init(void) {
    for (int i = 0; i < UDP_PORT_TABLE_SIZE; i++) {
        listeners[i].port = 0;
        listeners[i].cb   = 0;
        listeners[i].active = 0;
    }
}

int udp_listen(uint16_t port, udp_rx_fn cb) {
    if (!port || !cb) return 0;
    int slot = -1;
    for (int i = 0; i < UDP_PORT_TABLE_SIZE; i++) {
        if (listeners[i].active && listeners[i].port == port) return 0;
        if (!listeners[i].active && slot < 0) slot = i;
    }
    if (slot < 0) return 0;
    listeners[slot].port   = port;
    listeners[slot].cb     = cb;
    listeners[slot].active = 1;
    return 1;
}

void udp_unlisten(uint16_t port) {
    for (int i = 0; i < UDP_PORT_TABLE_SIZE; i++) {
        if (listeners[i].active && listeners[i].port == port) {
            listeners[i].active = 0;
            listeners[i].cb     = 0;
            listeners[i].port   = 0;
            return;
        }
    }
}

void udp_for_each_listener(udp_listener_iter_fn fn, void* user) {
    if (!fn) return;
    for (int i = 0; i < UDP_PORT_TABLE_SIZE; i++) {
        if (listeners[i].active) fn(listeners[i].port, user);
    }
}

// RFC 768 / RFC 1071 — internet checksum over a synthetic pseudo header
// (src IP, dst IP, zero, proto, udp_len) concatenated with the UDP header
// + data. We sum in network byte order.
uint16_t udp_checksum(const uint8_t src_ip[4], const uint8_t dst_ip[4],
                      const uint8_t* udp_hdr_and_data, uint16_t udp_len) {
    uint32_t sum = 0;

    // Pseudo header
    sum += ((uint32_t)src_ip[0] << 8) | src_ip[1];
    sum += ((uint32_t)src_ip[2] << 8) | src_ip[3];
    sum += ((uint32_t)dst_ip[0] << 8) | dst_ip[1];
    sum += ((uint32_t)dst_ip[2] << 8) | dst_ip[3];
    sum += UDP_PROTO;
    sum += udp_len;

    // UDP header + data
    const uint8_t* p = udp_hdr_and_data;
    uint16_t len = udp_len;
    while (len > 1) {
        sum += ((uint32_t)p[0] << 8) | p[1];
        p += 2; len -= 2;
    }
    if (len == 1) sum += (uint32_t)p[0] << 8;

    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    uint16_t cs = (uint16_t)(~sum);
    return cs ? cs : 0xFFFF;   // 0 means "not computed"; substitute 0xFFFF
}

// Build Eth+IPv4+UDP into `frame`. Returns total length on wire.
static int build_udp_frame(uint8_t* frame, uint16_t frame_max,
                           const uint8_t dst_mac[6], const uint8_t src_ip[4],
                           const uint8_t dst_ip[4],
                           uint16_t src_port, uint16_t dst_port,
                           const uint8_t* data, uint16_t len) {
    int total = 14 + 20 + 8 + (int)len;
    if (total > (int)frame_max) return -1;

    eth_header_t* eh = (eth_header_t*) frame;
    for (int i = 0; i < 6; i++) { eh->dst[i] = dst_mac[i]; eh->src[i] = net_mac()[i]; }
    eh->ethertype = htons(ETHERTYPE_IPV4);

    ipv4_header_t* ip = (ipv4_header_t*)(frame + 14);
    ip->ver_ihl    = 0x45;
    ip->tos        = 0;
    ip->total_len  = htons(20 + 8 + len);
    ip->id         = htons(1);
    ip->flags_frag = 0;
    ip->ttl        = 64;
    ip->protocol   = UDP_PROTO;
    for (int i = 0; i < 4; i++) ip->src[i] = src_ip[i];
    for (int i = 0; i < 4; i++) ip->dst[i] = dst_ip[i];
    ip->checksum   = 0;
    ip->checksum   = htons(net_checksum(ip, 20));

    udp_header_t* udp = (udp_header_t*)(frame + 14 + 20);
    udp->src_port = htons(src_port);
    udp->dst_port = htons(dst_port);
    udp->length   = htons(8 + len);
    udp->checksum = 0;

    uint8_t* payload = frame + 14 + 20 + 8;
    for (uint16_t i = 0; i < len; i++) payload[i] = data[i];

    // Compute checksum (optional for IPv4 UDP, but we set it for correctness)
    uint16_t cs = udp_checksum(src_ip, dst_ip, (const uint8_t*)udp, 8 + len);
    udp->checksum = htons(cs);

    return total;
}

int udp_send_to(const uint8_t dst_ip[4], uint16_t src_port, uint16_t dst_port,
                const uint8_t* data, uint16_t len) {
    uint8_t dst_mac[6];
    if (!arp_resolve(dst_ip, 500, dst_mac)) {
        for (int i = 0; i < 6; i++) dst_mac[i] = 0xFF;   // fall back to broadcast
    }
    static uint8_t frame[1600];
    int total = build_udp_frame(frame, sizeof(frame), dst_mac, net_ip(), dst_ip,
                                src_port, dst_port, data, len);
    if (total < 0) return -1;
    return rtl8139_send(frame, (uint16_t)total);
}

int udp_send_bcast(uint16_t src_port, uint16_t dst_port,
                   const uint8_t* data, uint16_t len) {
    static const uint8_t bcast_mac[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    static const uint8_t bcast_ip[4]  = { 0xFF, 0xFF, 0xFF, 0xFF };
    static const uint8_t zero_ip[4]   = { 0, 0, 0, 0 };
    static uint8_t frame[1600];
    int total = build_udp_frame(frame, sizeof(frame), bcast_mac, zero_ip, bcast_ip,
                                src_port, dst_port, data, len);
    if (total < 0) return -1;
    return rtl8139_send(frame, (uint16_t)total);
}

int udp_input(const uint8_t* frame, uint16_t len) {
    if (len < 14 + 20 + 8) return 0;
    // Ethertype IPv4
    if (!(frame[12] == 0x08 && frame[13] == 0x00)) return 0;
    const ipv4_header_t* ip = (const ipv4_header_t*)(frame + 14);
    if ((ip->ver_ihl & 0xF0) != 0x40) return 0;
    uint8_t ihl = (ip->ver_ihl & 0x0F) * 4;
    if (ihl < 20) return 0;
    if (ip->protocol != UDP_PROTO) return 0;
    if (14 + ihl + 8 > len) return 0;

    const udp_header_t* udp = (const udp_header_t*)(frame + 14 + ihl);
    uint16_t udp_len  = ntohs(udp->length);
    uint16_t dst_port = ntohs(udp->dst_port);
    uint16_t src_port = ntohs(udp->src_port);
    if (udp_len < 8) return 1;
    if (14 + ihl + udp_len > len) return 1;

    // Note: UDP checksum on RX is intentionally not validated. RFC 768 allows
    // checksum=0 (not computed). Validating against real-world senders adds risk
    // without a corresponding correctness win for our use cases.

    for (int i = 0; i < UDP_PORT_TABLE_SIZE; i++) {
        if (!listeners[i].active) continue;
        if (listeners[i].port != dst_port) continue;
        const uint8_t* payload = (const uint8_t*)udp + 8;
        uint16_t plen = udp_len - 8;
        listeners[i].cb(ip->src, src_port, payload, plen);
        break;
    }
    return 1;
}
