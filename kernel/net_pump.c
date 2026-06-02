#include "net_pump.h"
#include "net_io.h"
#include "arp.h"
#include "udp.h"
#include "../drivers/timer.h"

void net_pump_run(uint32_t ms) {
    uint64_t deadline = timer_uptime_ms() + ms;
    while (timer_uptime_ms() < deadline) {
        netio_pkt_t in;
        while (net_io_poll(&in)) {
            if (arp_input(in.data, in.len)) continue;
            if (udp_input(in.data, in.len)) continue;
            // ICMP and TCP will hook in here in later phases.
        }
        __asm__ volatile("hlt");
    }
}
