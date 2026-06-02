#ifndef NET_IO_H
#define NET_IO_H

#include "../include/types.h"

// ─────────────────────────────────────────────────
// Live packet I/O on top of the RTL8139 driver.
// Owns the RX queue and the UDP-broadcast TX path.
// ─────────────────────────────────────────────────

#define NETIO_RX_QUEUE     16
#define NETIO_RX_MAX_FRAME 1518

typedef struct {
    uint16_t len;
    uint8_t  data[NETIO_RX_MAX_FRAME];
} netio_pkt_t;

void net_io_init();

// Send a UDP broadcast packet (Ethernet broadcast, IP broadcast 255.255.255.255).
// Used by DHCPDISCOVER. Returns the number of bytes transmitted on the wire.
int  net_io_send_udp_bcast(uint16_t src_port, uint16_t dst_port,
                           const uint8_t* payload, uint16_t plen);

// Send a UDP packet to a specific IPv4 address (via gateway/broadcast MAC for simplicity).
int  net_io_send_udp(const uint8_t dst_ip[4], uint16_t src_port, uint16_t dst_port,
                     const uint8_t* payload, uint16_t plen);

// Main-loop poll: returns 1 if a packet was dequeued into *out.
int  net_io_poll(netio_pkt_t* out);

uint64_t net_io_rx_count();
uint64_t net_io_tx_count();

#endif
