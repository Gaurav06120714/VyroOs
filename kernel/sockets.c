#include "sockets.h"

static socket_t sockets[MAX_SOCKETS];
static int      next_fd = 100;

const char* tcp_state_name(tcp_state_t s) {
    switch (s) {
        case TCP_CLOSED:        return "CLOSED";
        case TCP_LISTEN:        return "LISTEN";
        case TCP_SYN_SENT:      return "SYN_SENT";
        case TCP_SYN_RECEIVED:  return "SYN_RECEIVED";
        case TCP_ESTABLISHED:   return "ESTABLISHED";
        case TCP_FIN_WAIT_1:    return "FIN_WAIT_1";
        case TCP_FIN_WAIT_2:    return "FIN_WAIT_2";
        case TCP_CLOSE_WAIT:    return "CLOSE_WAIT";
        case TCP_CLOSING:       return "CLOSING";
        case TCP_LAST_ACK:      return "LAST_ACK";
        case TCP_TIME_WAIT:     return "TIME_WAIT";
        default:                return "UNKNOWN";
    }
}

static socket_t* find_by_fd(int fd) {
    for (int i = 0; i < MAX_SOCKETS; i++)
        if (sockets[i].in_use && sockets[i].fd == fd) return &sockets[i];
    return 0;
}

int sock_socket(int family, int type, int proto) {
    (void)family; (void)proto;
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (!sockets[i].in_use) {
            sockets[i].in_use = 1;
            sockets[i].type   = type;
            sockets[i].fd     = next_fd++;
            sockets[i].local_port = 0;
            sockets[i].tcp_state  = (type == SOCK_STREAM) ? TCP_CLOSED : TCP_CLOSED;
            sockets[i].bytes_tx = sockets[i].bytes_rx = 0;
            return sockets[i].fd;
        }
    }
    return -1;
}

int sock_bind(int fd, const sock_addr_t* addr) {
    socket_t* s = find_by_fd(fd);
    if (!s) return -1;
    s->local_port = addr->port;
    return 0;
}

int sock_connect(int fd, const sock_addr_t* addr) {
    socket_t* s = find_by_fd(fd);
    if (!s) return -1;
    s->remote = *addr;
    if (s->type == SOCK_STREAM) {
        // Simulate the SYN -> SYN-ACK -> ACK handshake state walk
        s->tcp_state = TCP_SYN_SENT;
        s->tcp_state = TCP_ESTABLISHED;   // stub: instant connect
    }
    return 0;
}

int sock_listen(int fd, int backlog) {
    (void)backlog;
    socket_t* s = find_by_fd(fd);
    if (!s) return -1;
    if (s->type == SOCK_STREAM) s->tcp_state = TCP_LISTEN;
    return 0;
}

int sock_send(int fd, const void* buf, uint32_t len) {
    (void)buf;
    socket_t* s = find_by_fd(fd);
    if (!s) return -1;
    s->bytes_tx += len;
    return (int)len;
}

int sock_recv(int fd, void* buf, uint32_t len) {
    (void)buf; (void)len;
    socket_t* s = find_by_fd(fd);
    if (!s) return -1;
    return 0;   // no data available (stub)
}

int sock_close(int fd) {
    socket_t* s = find_by_fd(fd);
    if (!s) return -1;
    if (s->type == SOCK_STREAM && s->tcp_state == TCP_ESTABLISHED) {
        s->tcp_state = TCP_FIN_WAIT_1;
        s->tcp_state = TCP_FIN_WAIT_2;
        s->tcp_state = TCP_TIME_WAIT;
        s->tcp_state = TCP_CLOSED;
    }
    s->in_use = 0;
    return 0;
}

int sock_count() {
    int n = 0;
    for (int i = 0; i < MAX_SOCKETS; i++) if (sockets[i].in_use) n++;
    return n;
}

socket_t* sock_get(int i) {
    int j = 0;
    for (int k = 0; k < MAX_SOCKETS; k++) {
        if (sockets[k].in_use) { if (j == i) return &sockets[k]; j++; }
    }
    return 0;
}
