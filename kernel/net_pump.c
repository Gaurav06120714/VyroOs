#include "net_pump.h"
#include "net_io.h"
#include "arp.h"
#include "udp.h"
#include "tcp.h"
#include "../drivers/timer.h"

void net_pump_run(uint32_t ms) {
    uint64_t deadline = timer_uptime_ms() + ms;
    uint64_t next_tcp_tick = timer_uptime_ms();
    while (timer_uptime_ms() < deadline) {
        netio_pkt_t in;
        while (net_io_poll(&in)) {
            if (arp_input(in.data, in.len)) continue;
            if (udp_input(in.data, in.len)) continue;
            if (tcp_input(in.data, in.len)) continue;
        }
        if (timer_uptime_ms() >= next_tcp_tick) {
            tcp_tick();
            next_tcp_tick = timer_uptime_ms() + 100;
        }
        __asm__ volatile("hlt");
    }
}
