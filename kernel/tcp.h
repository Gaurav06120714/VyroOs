#ifndef TCP_H
#define TCP_H

#include "../include/types.h"

// TCP (RFC 793) — active-open client only this phase. Up to 16 simultaneous TCBs.
// No data transfer yet — handshake, RST handling, graceful close (FIN).

enum tcp_state {
    TCP_CLOSED = 0,
    TCP_LISTEN,
    TCP_SYN_SENT,
    TCP_SYN_RECEIVED,
    TCP_ESTABLISHED,
    TCP_FIN_WAIT_1,
    TCP_FIN_WAIT_2,
    TCP_CLOSE_WAIT,
    TCP_LAST_ACK,
    TCP_TIME_WAIT
};

typedef struct {
    uint8_t  state;
    uint8_t  local_ip[4],  remote_ip[4];
    uint16_t local_port,   remote_port;
    uint32_t snd_nxt;
    uint32_t snd_una;
    uint32_t rcv_nxt;
    uint64_t syn_sent_at_ms;
    uint8_t  retries;
    uint8_t  active;
    uint8_t  is_listener;     // 1 = passive (listen) socket
    uint8_t  accepted;        // 1 = already returned by tcp_accept

    // RTT estimation (RFC 6298)
    uint32_t srtt_ms;         // smoothed RTT, 0 = uninitialized
    uint32_t rttvar_ms;
    uint32_t rto_ms;          // current retransmit timeout
    uint32_t rtt_probe_seq;   // sequence number we're timing (snd_nxt at send)
    uint64_t rtt_probe_sent_ms;
    uint8_t  rtt_in_flight;   // 1 if currently timing a segment

    // Fast retransmit
    uint32_t last_ack_recv;
    uint8_t  dup_acks;

    // Congestion control (bytes)
    uint32_t cwnd;
    uint32_t ssthresh;
} tcp_tcb_t;

void tcp_init(void);

// Active open. Returns conn id (>=0) on success, -1 on failure.
int  tcp_connect(const uint8_t dst_ip[4], uint16_t dst_port);

// Passive open. Returns listener id (>=0) on success, -1 on failure.
int  tcp_listen(uint16_t local_port);

// Pull a completed inbound connection on `local_port`. Returns conn id (>=0)
// of a freshly ESTABLISHED child not yet returned, or -1 if none ready.
int  tcp_accept(uint16_t local_port);

// Begin a graceful close. Returns 0 on success.
int  tcp_close(int id);

// Send bytes on an ESTABLISHED conn. Returns bytes accepted into the send buffer
// (may be less than `len` if the send window is congested).
int  tcp_send(int id, const uint8_t* data, uint16_t len);

// Drain up to `max` bytes from the conn's receive buffer. Returns bytes copied.
// Returns 0 if no data is ready, -1 on bad id.
int  tcp_recv(int id, uint8_t* out, uint16_t max);

// State accessor for the given conn id. Returns TCP_CLOSED for invalid id.
int  tcp_state(int id);

// RX hook — call from the main-loop pump after udp_input.
int  tcp_input(const uint8_t* frame, uint16_t len);

typedef void (*tcp_iter_fn)(int id, const tcp_tcb_t* tcb, void* user);
void tcp_for_each(tcp_iter_fn fn, void* user);

// Per-tick maintenance: retransmits SYN, expires TIME_WAIT, etc.
void tcp_tick(void);

uint16_t tcp_checksum(const uint8_t src_ip[4], const uint8_t dst_ip[4],
                      const uint8_t* tcp_hdr_and_data, uint16_t len);

#endif
