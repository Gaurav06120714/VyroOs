#include "http.h"
#include "tcp.h"
#include "net_pump.h"
#include "../drivers/timer.h"

static uint32_t mini_strlen(const char* s) {
    uint32_t n = 0; while (s && s[n]) n++; return n;
}

static void itoa10(char* dst, uint32_t v) {
    char tmp[12]; int n = 0;
    if (v == 0) { dst[0] = '0'; dst[1] = 0; return; }
    while (v > 0) { tmp[n++] = '0' + (v % 10); v /= 10; }
    int p = 0;
    while (n > 0) dst[p++] = tmp[--n];
    dst[p] = 0;
}

int http_get(const uint8_t ip[4], uint16_t port,
             const char* host, const char* path,
             uint8_t* out, uint32_t out_max,
             uint32_t timeout_ms) {
    int tid = tcp_connect(ip, port);
    if (tid < 0) return -1;
    uint64_t deadline = timer_uptime_ms() + timeout_ms;
    while (timer_uptime_ms() < deadline && tcp_state(tid) == TCP_SYN_SENT) {
        net_pump_run(100);
    }
    if (tcp_state(tid) != TCP_ESTABLISHED) { tcp_close(tid); return -2; }

    // Build request — GET path HTTP/1.1\r\nHost: host\r\nConnection: close\r\n\r\n
    static uint8_t req[1024];
    uint32_t p = 0;
    const char* prefix = "GET ";
    for (uint32_t i = 0; prefix[i]; i++) req[p++] = (uint8_t)prefix[i];
    for (uint32_t i = 0; path[i] && p < sizeof(req); i++) req[p++] = (uint8_t)path[i];
    const char* tail = " HTTP/1.1\r\nHost: ";
    for (uint32_t i = 0; tail[i]; i++) req[p++] = (uint8_t)tail[i];
    for (uint32_t i = 0; host[i] && p < sizeof(req); i++) req[p++] = (uint8_t)host[i];
    const char* end = "\r\nConnection: close\r\nUser-Agent: VyroOS/3.21\r\n\r\n";
    for (uint32_t i = 0; end[i]; i++) req[p++] = (uint8_t)end[i];

    int sent = tcp_send(tid, req, (uint16_t)p);
    if (sent != (int)p) { tcp_close(tid); return -3; }

    // Drain response until close or timeout.
    uint32_t read_total = 0;
    while (timer_uptime_ms() < deadline && read_total < out_max) {
        net_pump_run(100);
        int n = tcp_recv(tid, out + read_total, (uint16_t)(out_max - read_total));
        if (n > 0) read_total += (uint32_t)n;
        if (tcp_state(tid) == TCP_CLOSE_WAIT || tcp_state(tid) == TCP_CLOSED) break;
    }
    tcp_close(tid);
    return (int)read_total;
}
