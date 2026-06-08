#ifndef TCP_H
#define TCP_H

#include "../include/types.h"

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
    uint8_t  is_listener;
    uint8_t  accepted;


    uint32_t srtt_ms;
    uint32_t rttvar_ms;
    uint32_t rto_ms;
    uint32_t rtt_probe_seq;
    uint64_t rtt_probe_sent_ms;
    uint8_t  rtt_in_flight;


    uint32_t last_ack_recv;
    uint8_t  dup_acks;


    uint32_t cwnd;
    uint32_t ssthresh;
} tcp_tcb_t;

void tcp_init(void);

int  tcp_connect(const uint8_t dst_ip[4], uint16_t dst_port);

int  tcp_listen(uint16_t local_port);

int  tcp_accept(uint16_t local_port);

int  tcp_close(int id);

int  tcp_send(int id, const uint8_t* data, uint16_t len);

int  tcp_recv(int id, uint8_t* out, uint16_t max);

int  tcp_state(int id);

int  tcp_input(const uint8_t* frame, uint16_t len);

typedef void (*tcp_iter_fn)(int id, const tcp_tcb_t* tcb, void* user);
void tcp_for_each(tcp_iter_fn fn, void* user);

void tcp_tick(void);

uint16_t tcp_checksum(const uint8_t src_ip[4], const uint8_t dst_ip[4],
                      const uint8_t* tcp_hdr_and_data, uint16_t len);

#endif
