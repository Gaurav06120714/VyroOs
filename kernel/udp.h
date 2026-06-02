#ifndef UDP_H
#define UDP_H

#include "../include/types.h"

// UDP transport layer (RFC 768) over IPv4.
// Provides a port-dispatch API: callers register a callback per local port,
// udp_input() parses incoming frames and dispatches to the matching listener.

typedef void (*udp_rx_fn)(const uint8_t src_ip[4], uint16_t src_port,
                         const uint8_t* data, uint16_t len);

void udp_init(void);

// Register cb for inbound UDP datagrams destined to `port`. Returns 1 on success,
// 0 if the table is full or the port is already taken.
int  udp_listen(uint16_t port, udp_rx_fn cb);

// Remove any listener on `port` (no-op if none).
void udp_unlisten(uint16_t port);

// Send a datagram. Returns bytes on wire on success, <=0 on failure.
// udp_send_to ARPs dst_ip first (or falls back to broadcast MAC).
// udp_send_bcast uses 255.255.255.255 / Ethernet broadcast and source 0.0.0.0.
int  udp_send_to(const uint8_t dst_ip[4], uint16_t src_port, uint16_t dst_port,
                 const uint8_t* data, uint16_t len);
int  udp_send_bcast(uint16_t src_port, uint16_t dst_port,
                    const uint8_t* data, uint16_t len);

// Hand a received Ethernet frame to UDP. Returns 1 if the frame was a UDP/IPv4
// datagram and dispatched (or dropped due to no listener); 0 if it wasn't UDP.
int  udp_input(const uint8_t* frame, uint16_t len);

typedef void (*udp_listener_iter_fn)(uint16_t port, void* user);
void udp_for_each_listener(udp_listener_iter_fn fn, void* user);

// Exposed for callers that want to compute checksum manually.
uint16_t udp_checksum(const uint8_t src_ip[4], const uint8_t dst_ip[4],
                      const uint8_t* udp_hdr_and_data, uint16_t udp_len);

#endif
