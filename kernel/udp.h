#ifndef UDP_H
#define UDP_H

#include "../include/types.h"

typedef void (*udp_rx_fn)(const uint8_t src_ip[4], uint16_t src_port,
                         const uint8_t* data, uint16_t len);

void udp_init(void);

int  udp_listen(uint16_t port, udp_rx_fn cb);

void udp_unlisten(uint16_t port);

int  udp_send_to(const uint8_t dst_ip[4], uint16_t src_port, uint16_t dst_port,
                 const uint8_t* data, uint16_t len);
int  udp_send_bcast(uint16_t src_port, uint16_t dst_port,
                    const uint8_t* data, uint16_t len);

int  udp_input(const uint8_t* frame, uint16_t len);

typedef void (*udp_listener_iter_fn)(uint16_t port, void* user);
void udp_for_each_listener(udp_listener_iter_fn fn, void* user);

uint16_t udp_checksum(const uint8_t src_ip[4], const uint8_t dst_ip[4],
                      const uint8_t* udp_hdr_and_data, uint16_t udp_len);

#endif
