#ifndef NET_PUMP_H
#define NET_PUMP_H

#include "../include/types.h"

// Drain the RX queue and dispatch frames through ARP and UDP for `ms` milliseconds.
// Designed to be called from the main thread / shell commands that wait for replies.
// Uses `hlt` between polls so idle CPU is near zero.
void net_pump_run(uint32_t ms);

#endif
