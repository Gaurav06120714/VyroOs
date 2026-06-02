#include "dns_real.h"
#include "udp.h"
#include "net_pump.h"
#include "net_io.h"
#include "../drivers/timer.h"

// Compile-time switch: when defined, falls back to the pre-v3.1 direct
// net_io_poll path.
// #define VYRO_UDP_LEGACY 1

static volatile uint16_t pending_txid = 0;
static volatile uint8_t  got_result   = 0;
static uint8_t           result_ip[4] = {0,0,0,0};
static uint16_t          eph_port     = 49152;

static int build_dns_query(const char* hostname, uint16_t txid, uint8_t* out) {
    out[0] = (txid >> 8) & 0xFF; out[1] = txid & 0xFF;
    out[2] = 0x01; out[3] = 0x00;
    out[4] = 0x00; out[5] = 0x01;
    out[6] = 0x00; out[7] = 0x00;
    out[8] = 0x00; out[9] = 0x00;
    out[10] = 0x00; out[11] = 0x00;
    int p = 12;
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
    out[p++] = 0x00;
    out[p++] = 0x00; out[p++] = 0x01;
    out[p++] = 0x00; out[p++] = 0x01;
    return p;
}

static int parse_dns_payload(const uint8_t* dns, uint16_t dns_len, uint16_t txid,
                             uint8_t out_ip[4]) {
    if (dns_len < 12) return 0;
    uint16_t reply_txid = ((uint16_t)dns[0] << 8) | dns[1];
    if (reply_txid != txid) return 0;
    uint16_t qd = ((uint16_t)dns[4] << 8) | dns[5];
    uint16_t an = ((uint16_t)dns[6] << 8) | dns[7];
    if (an == 0) return 0;

    int p = 12;
    for (int q = 0; q < qd && p < dns_len; q++) {
        while (p < dns_len && dns[p] != 0) {
            if ((dns[p] & 0xC0) == 0xC0) { p += 2; goto skipped; }
            p += dns[p] + 1;
        }
        p++;
        skipped:
        p += 4;
    }
    for (int a = 0; a < an && p < dns_len; a++) {
        if ((dns[p] & 0xC0) == 0xC0) p += 2;
        else {
            while (p < dns_len && dns[p] != 0) p += dns[p] + 1;
            p++;
        }
        if (p + 10 > dns_len) return 0;
        uint16_t type = ((uint16_t)dns[p] << 8) | dns[p+1]; p += 2;
        p += 2;
        p += 4;
        uint16_t rdlen = ((uint16_t)dns[p] << 8) | dns[p+1]; p += 2;
        if (type == 1 && rdlen == 4 && p + 4 <= dns_len) {
            for (int i = 0; i < 4; i++) out_ip[i] = dns[p + i];
            return 1;
        }
        p += rdlen;
    }
    return 0;
}

#ifndef VYRO_UDP_LEGACY
static void on_dns_reply(const uint8_t src_ip[4], uint16_t src_port,
                         const uint8_t* data, uint16_t len) {
    (void)src_ip;
    if (got_result) return;
    if (src_port != 53) return;
    if (parse_dns_payload(data, len, pending_txid, result_ip)) {
        got_result = 1;
    }
}
#endif

int dns_real_resolve(const char* hostname, const uint8_t dns_server_ip[4],
                     uint32_t timeout_ms, uint8_t out_ip[4]) {
    uint16_t txid = (uint16_t)(timer_ticks() & 0xFFFF);
    pending_txid  = txid;
    got_result    = 0;

    static uint8_t qbuf[256];
    int qlen = build_dns_query(hostname, txid, qbuf);
    uint16_t my_port = ++eph_port;
    if (eph_port > 60000) eph_port = 49152;

#ifndef VYRO_UDP_LEGACY
    if (!udp_listen(my_port, on_dns_reply)) return 0;
    int sent = udp_send_to(dns_server_ip, my_port, 53, qbuf, (uint16_t)qlen);
    if (sent <= 0) { udp_unlisten(my_port); return 0; }

    uint64_t deadline = timer_uptime_ms() + timeout_ms;
    while (timer_uptime_ms() < deadline && !got_result) {
        net_pump_run(50);
    }
    udp_unlisten(my_port);
    if (got_result) {
        for (int i = 0; i < 4; i++) out_ip[i] = result_ip[i];
        return 1;
    }
    return 0;
#else
    if (net_io_send_udp(dns_server_ip, my_port, 53, qbuf, (uint16_t)qlen) <= 0)
        return 0;
    uint64_t deadline = timer_uptime_ms() + timeout_ms;
    while (timer_uptime_ms() < deadline) {
        netio_pkt_t in;
        while (net_io_poll(&in)) {
            if (in.len < 14 + 20 + 8 + 12) continue;
            if (in.data[14 + 9] != 17) continue;
            uint16_t udp_src = ((uint16_t)in.data[14 + 20] << 8) | in.data[14 + 20 + 1];
            if (udp_src != 53) continue;
            const uint8_t* dns = in.data + 14 + 20 + 8;
            uint16_t dlen = in.len - (14 + 20 + 8);
            if (parse_dns_payload(dns, dlen, txid, out_ip)) return 1;
        }
        __asm__ volatile("hlt");
    }
    return 0;
#endif
}
