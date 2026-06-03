#include "http.h"
#include "tcp.h"
#include "net_pump.h"
#include "../drivers/timer.h"

static uint32_t mini_strlen(const char* s) {
    uint32_t n = 0; while (s && s[n]) n++; return n;
}

static int ci_eq_prefix(const uint8_t* a, const char* b, uint32_t blen) {
    for (uint32_t i = 0; i < blen; i++) {
        char x = (char)a[i], y = b[i];
        if (x >= 'A' && x <= 'Z') x += 32;
        if (y >= 'A' && y <= 'Z') y += 32;
        if (x != y) return 0;
    }
    return 1;
}

int http_parse_response(const uint8_t* buf, uint32_t len,
                        int* status,
                        const uint8_t** body_out, uint32_t* body_len_out,
                        int32_t* content_length) {
    if (!buf || len < 12) return 0;
    if (!(buf[0] == 'H' && buf[1] == 'T' && buf[2] == 'T' && buf[3] == 'P' &&
          buf[4] == '/' && buf[5] == '1' && buf[6] == '.' &&
          (buf[7] == '0' || buf[7] == '1'))) return 0;
    // Find first space, then parse status code
    uint32_t p = 8;
    while (p < len && buf[p] != ' ') p++;
    if (p + 4 > len) return 0;
    p++;
    int code = 0;
    for (int i = 0; i < 3 && p + i < len; i++) {
        if (buf[p + i] < '0' || buf[p + i] > '9') return 0;
        code = code * 10 + (buf[p + i] - '0');
    }
    if (status) *status = code;
    p += 3;
    // Skip to end of status line (\r\n)
    while (p + 1 < len && !(buf[p] == '\r' && buf[p + 1] == '\n')) p++;
    if (p + 2 > len) return 0;
    p += 2;

    int32_t cl = -1;
    // Walk headers until blank \r\n
    while (p + 2 <= len) {
        if (buf[p] == '\r' && buf[p + 1] == '\n') { p += 2; break; }
        // Header line: name ':' value \r\n
        uint32_t line_start = p;
        while (p < len && buf[p] != '\r') p++;
        if (p + 2 > len) return 0;
        uint32_t line_end = p;
        // Parse Content-Length if present
        const char* k = "content-length:";
        if (line_end - line_start > 15 && ci_eq_prefix(buf + line_start, k, 15)) {
            uint32_t v = line_start + 15;
            while (v < line_end && buf[v] == ' ') v++;
            int32_t val = 0;
            while (v < line_end && buf[v] >= '0' && buf[v] <= '9') {
                val = val * 10 + (buf[v] - '0'); v++;
            }
            cl = val;
        }
        p += 2;     // skip \r\n
    }
    if (content_length) *content_length = cl;
    if (body_out)       *body_out       = buf + p;
    if (body_len_out)   *body_len_out   = len - p;
    return 1;
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
