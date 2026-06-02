#include "arp.h"
#include "net.h"
#include "net_io.h"
#include "../drivers/rtl8139.h"
#include "../drivers/timer.h"

#define ARP_CACHE_SIZE 8

typedef struct {
    uint8_t  ip[4];
    uint8_t  mac[6];
    uint8_t  valid;
} arp_entry_t;

static arp_entry_t cache[ARP_CACHE_SIZE];

static int cache_lookup(const uint8_t ip[4], uint8_t out_mac[6]) {
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!cache[i].valid) continue;
        int m = 1;
        for (int j = 0; j < 4; j++) if (cache[i].ip[j] != ip[j]) { m = 0; break; }
        if (m) { for (int j = 0; j < 6; j++) out_mac[j] = cache[i].mac[j]; return 1; }
    }
    return 0;
}

static void cache_insert(const uint8_t ip[4], const uint8_t mac[6]) {
    int slot = -1;
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (cache[i].valid) {
            int m = 1;
            for (int j = 0; j < 4; j++) if (cache[i].ip[j] != ip[j]) { m = 0; break; }
            if (m) { slot = i; break; }
        } else if (slot < 0) slot = i;
    }
    if (slot < 0) slot = 0;   // evict first slot
    for (int j = 0; j < 4; j++) cache[slot].ip[j]  = ip[j];
    for (int j = 0; j < 6; j++) cache[slot].mac[j] = mac[j];
    cache[slot].valid = 1;
}

static void send_arp_request(const uint8_t target_ip[4]) {
    uint8_t frame[42];
    for (int i = 0; i < 42; i++) frame[i] = 0;

    eth_header_t* eh = (eth_header_t*) frame;
    for (int i = 0; i < 6; i++) { eh->dst[i] = 0xFF; eh->src[i] = net_mac()[i]; }
    eh->ethertype = htons(ETHERTYPE_ARP);

    arp_packet_t* a = (arp_packet_t*)(frame + 14);
    a->htype = htons(1);
    a->ptype = htons(ETHERTYPE_IPV4);
    a->hlen  = 6;
    a->plen  = 4;
    a->oper  = htons(1);                                         // request
    for (int i = 0; i < 6; i++) a->sha[i] = net_mac()[i];
    for (int i = 0; i < 4; i++) a->spa[i] = net_ip()[i];
    for (int i = 0; i < 6; i++) a->tha[i] = 0;
    for (int i = 0; i < 4; i++) a->tpa[i] = target_ip[i];

    rtl8139_send(frame, 42);
}

static void send_arp_reply(const uint8_t their_mac[6], const uint8_t their_ip[4]) {
    uint8_t frame[42];
    for (int i = 0; i < 42; i++) frame[i] = 0;

    eth_header_t* eh = (eth_header_t*) frame;
    for (int i = 0; i < 6; i++) { eh->dst[i] = their_mac[i]; eh->src[i] = net_mac()[i]; }
    eh->ethertype = htons(ETHERTYPE_ARP);

    arp_packet_t* a = (arp_packet_t*)(frame + 14);
    a->htype = htons(1);
    a->ptype = htons(ETHERTYPE_IPV4);
    a->hlen  = 6;
    a->plen  = 4;
    a->oper  = htons(2);                                         // reply
    for (int i = 0; i < 6; i++) a->sha[i] = net_mac()[i];
    for (int i = 0; i < 4; i++) a->spa[i] = net_ip()[i];
    for (int i = 0; i < 6; i++) a->tha[i] = their_mac[i];
    for (int i = 0; i < 4; i++) a->tpa[i] = their_ip[i];

    rtl8139_send(frame, 42);
}

int arp_input(const uint8_t* frame, uint16_t len) {
    if (len < 14 + 28) return 0;
    const eth_header_t* eh = (const eth_header_t*) frame;
    if (ntohs(eh->ethertype) != ETHERTYPE_ARP) return 0;

    const arp_packet_t* a = (const arp_packet_t*)(frame + 14);
    if (ntohs(a->ptype) != ETHERTYPE_IPV4 || a->hlen != 6 || a->plen != 4) return 1;

    // Learn the sender's IP→MAC mapping regardless of op
    cache_insert(a->spa, a->sha);

    // If it's a request for our IP, reply
    if (ntohs(a->oper) == 1) {
        int for_us = 1;
        for (int i = 0; i < 4; i++) if (a->tpa[i] != net_ip()[i]) { for_us = 0; break; }
        if (for_us) send_arp_reply(a->sha, a->spa);
    }
    return 1;
}

int arp_resolve(const uint8_t dst_ip[4], uint32_t timeout_ms, uint8_t out_mac[6]) {
    if (cache_lookup(dst_ip, out_mac)) return 1;

    send_arp_request(dst_ip);

    uint64_t deadline = timer_uptime_ms() + timeout_ms;
    uint64_t next_retry = timer_uptime_ms() + 250;
    while (timer_uptime_ms() < deadline) {
        netio_pkt_t in;
        while (net_io_poll(&in)) {
            arp_input(in.data, in.len);
            if (cache_lookup(dst_ip, out_mac)) return 1;
        }
        if (timer_uptime_ms() >= next_retry) {
            send_arp_request(dst_ip);
            next_retry = timer_uptime_ms() + 250;
        }
        __asm__ volatile("hlt");
    }
    return 0;
}
