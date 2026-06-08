#ifndef NET_IO_H
#define NET_IO_H

#include "../include/types.h"

#define NETIO_RX_QUEUE     16
#define NETIO_RX_MAX_FRAME 1518

typedef struct {
    uint16_t len;
    uint8_t  data[NETIO_RX_MAX_FRAME];
} netio_pkt_t;

void net_io_init();

int  net_io_send_udp_bcast(uint16_t src_port, uint16_t dst_port,
                           const uint8_t* payload, uint16_t plen);

int  net_io_send_udp(const uint8_t dst_ip[4], uint16_t src_port, uint16_t dst_port,
                     const uint8_t* payload, uint16_t plen);

int  net_io_poll(netio_pkt_t* out);

uint64_t net_io_rx_count();
uint64_t net_io_tx_count();

#endif
