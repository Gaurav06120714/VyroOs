#include "dns_real.h"
#include "net_io.h"
#include "../drivers/timer.h"

// Build a DNS A query for `hostname`. Returns length of query buffer.
static int build_dns_query(const char* hostname, uint16_t txid, uint8_t* out) {
    // 12-byte header
    out[0] = (txid >> 8) & 0xFF; out[1] = txid & 0xFF;
    out[2] = 0x01;   // flags: recursion desired
    out[3] = 0x00;
    out[4] = 0x00; out[5] = 0x01;     // qd_count = 1
    out[6] = 0x00; out[7] = 0x00;
    out[8] = 0x00; out[9] = 0x00;
    out[10] = 0x00; out[11] = 0x00;
    int p = 12;
    // QNAME: length-prefixed labels
    int label_start = p++;
    int label_len = 0;
    for (int i = 0; ; i++) {
        char c = hostname[i];
        if (c == '.' || c == '\0') {
            out[label_start] = (uint8_t)label_len;
            if (c == '\0') break;
            label_start = p++;
            label_len = 0;
        } else {
            out[p++] = (uint8_t)c;
            label_len++;
        }
    }
    out[p++] = 0x00;       // null terminator
    out[p++] = 0x00; out[p++] = 0x01;   // QTYPE = A
    out[p++] = 0x00; out[p++] = 0x01;   // QCLASS = IN
    return p;
}

// Parse an A response. Returns 1 if found, fills out_ip.
static int parse_dns_response(const netio_pkt_t* pkt, uint16_t txid, uint8_t out_ip[4]) {
    if (pkt->len < 14 + 20 + 8 + 12) return 0;
    if (!(pkt->data[12] == 0x08 && pkt->data[13] == 0x00)) return 0;
    if (pkt->data[14 + 9] != 17) return 0;       // not UDP
    uint16_t udp_src = ((uint16_t)pkt->data[14 + 20] << 8) | pkt->data[14 + 20 + 1];
    if (udp_src != 53) return 0;

    const uint8_t* dns = pkt->data + 14 + 20 + 8;
    uint16_t reply_txid = ((uint16_t)dns[0] << 8) | dns[1];
    if (reply_txid != txid) return 0;

    uint16_t qd = ((uint16_t)dns[4] << 8) | dns[5];
    uint16_t an = ((uint16_t)dns[6] << 8) | dns[7];
    if (an == 0) return 0;

    // Skip the question section
    int p = 12;
    int dns_len = pkt->len - (14 + 20 + 8);
    for (int q = 0; q < qd && p < dns_len; q++) {
        while (p < dns_len && dns[p] != 0) {
            if ((dns[p] & 0xC0) == 0xC0) { p += 2; goto skipped; }
            p += dns[p] + 1;
        }
        p++;        // null terminator
        skipped:
        p += 4;     // QTYPE + QCLASS
    }
    // Walk answer records
    for (int a = 0; a < an && p < dns_len; a++) {
        // NAME (compressed or label sequence)
        if ((dns[p] & 0xC0) == 0xC0) p += 2;
        else {
            while (p < dns_len && dns[p] != 0) p += dns[p] + 1;
            p++;
        }
        if (p + 10 > dns_len) return 0;
        uint16_t type   = ((uint16_t)dns[p] << 8) | dns[p+1]; p += 2;
        /*uint16_t cls    = ((uint16_t)dns[p] << 8) | dns[p+1];*/ p += 2;
        /*uint32_t ttl ...*/ p += 4;
        uint16_t rdlen  = ((uint16_t)dns[p] << 8) | dns[p+1]; p += 2;
        if (type == 1 && rdlen == 4) {
            for (int i = 0; i < 4; i++) out_ip[i] = dns[p + i];
            return 1;
        }
        p += rdlen;
    }
    return 0;
}

int dns_real_resolve(const char* hostname, const uint8_t dns_server_ip[4],
                     uint32_t timeout_ms, uint8_t out_ip[4]) {
    uint16_t txid = (uint16_t)(timer_ticks() & 0xFFFF);
    static uint8_t qbuf[256];
    int qlen = build_dns_query(hostname, txid, qbuf);

    if (net_io_send_udp(dns_server_ip, 51334, 53, qbuf, (uint16_t)qlen) <= 0)
        return 0;

    uint64_t deadline = timer_uptime_ms() + timeout_ms;
    while (timer_uptime_ms() < deadline) {
        netio_pkt_t in;
        while (net_io_poll(&in)) {
            if (parse_dns_response(&in, txid, out_ip)) return 1;
        }
        __asm__ volatile("hlt");
    }
    return 0;
}
