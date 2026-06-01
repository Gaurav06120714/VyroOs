#ifndef SOCKETS_H
#define SOCKETS_H

#include "../include/types.h"

// ─────────────────────────────────────────────────
// Vyro OS Sockets API (Phase 46)
// Berkeley-style socket interface. Backend is a stub
// transport layer; the API and state machines are real.
// ─────────────────────────────────────────────────

#define SOCK_DGRAM   1     // UDP
#define SOCK_STREAM  2     // TCP

#define AF_INET      2

#define IPPROTO_UDP  17
#define IPPROTO_TCP  6

#define MAX_SOCKETS  32

typedef struct {
    uint16_t family;
    uint16_t port;
    uint8_t  addr[4];
} sock_addr_t;

// TCP state machine states (RFC 793)
typedef enum {
    TCP_CLOSED,
    TCP_LISTEN,
    TCP_SYN_SENT,
    TCP_SYN_RECEIVED,
    TCP_ESTABLISHED,
    TCP_FIN_WAIT_1,
    TCP_FIN_WAIT_2,
    TCP_CLOSE_WAIT,
    TCP_CLOSING,
    TCP_LAST_ACK,
    TCP_TIME_WAIT
} tcp_state_t;

typedef struct {
    int          fd;
    uint8_t      in_use;
    uint8_t      type;          // SOCK_DGRAM or SOCK_STREAM
    uint16_t     local_port;
    sock_addr_t  remote;
    tcp_state_t  tcp_state;     // only for SOCK_STREAM
    uint64_t     bytes_tx;
    uint64_t     bytes_rx;
} socket_t;

int  sock_socket(int family, int type, int proto);
int  sock_bind(int fd, const sock_addr_t* addr);
int  sock_connect(int fd, const sock_addr_t* addr);
int  sock_listen(int fd, int backlog);
int  sock_send(int fd, const void* buf, uint32_t len);
int  sock_recv(int fd, void* buf, uint32_t len);
int  sock_close(int fd);

// Inspection
int  sock_count();
socket_t* sock_get(int i);
const char* tcp_state_name(tcp_state_t s);

#endif
