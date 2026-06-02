#include "tcp.h"
#include "net.h"
#include "arp.h"
#include "../drivers/rtl8139.h"
#include "../drivers/timer.h"

#define TCB_COUNT      16
#define TCP_PROTO      6
#define TCP_MSS        536
#define SYN_RETRY_MAX  3

#define TCP_FIN  0x01
#define TCP_SYN  0x02
#define TCP_RST  0x04
#define TCP_PSH  0x08
#define TCP_ACK  0x10

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq;
    uint32_t ack;
    uint8_t  data_off;     // upper 4 bits: header length in 32-bit words
    uint8_t  flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;
} __attribute__((packed)) tcp_header_t;

static tcp_tcb_t tcbs[TCB_COUNT];
static uint16_t  next_eph_port = 32768;

void tcp_init(void) {
    for (int i = 0; i < TCB_COUNT; i++) {
        tcbs[i].state       = TCP_CLOSED;
        tcbs[i].active      = 0;
        tcbs[i].is_listener = 0;
        tcbs[i].accepted    = 0;
    }
}

static int alloc_tcb(void) {
    for (int i = 0; i < TCB_COUNT; i++) if (!tcbs[i].active) return i;
    return -1;
}

int tcp_listen(uint16_t local_port) {
    if (!local_port) return -1;
    // Reject duplicate listener on same port
    for (int i = 0; i < TCB_COUNT; i++) {
        if (tcbs[i].active && tcbs[i].is_listener && tcbs[i].local_port == local_port)
            return -1;
    }
    int id = alloc_tcb();
    if (id < 0) return -1;
    tcp_tcb_t* t = &tcbs[id];
    t->state       = TCP_LISTEN;
    t->is_listener = 1;
    t->accepted    = 0;
    for (int i = 0; i < 4; i++) { t->local_ip[i] = net_ip()[i]; t->remote_ip[i] = 0; }
    t->local_port  = local_port;
    t->remote_port = 0;
    t->snd_nxt = t->snd_una = t->rcv_nxt = 0;
    t->retries = 0;
    t->syn_sent_at_ms = 0;
    t->active = 1;
    return id;
}

int tcp_accept(uint16_t local_port) {
    for (int i = 0; i < TCB_COUNT; i++) {
        tcp_tcb_t* t = &tcbs[i];
        if (!t->active || t->is_listener) continue;
        if (t->local_port != local_port)  continue;
        if (t->state != TCP_ESTABLISHED && t->state != TCP_CLOSE_WAIT) continue;
        if (t->accepted) continue;
        t->accepted = 1;
        return i;
    }
    return -1;
}

static int find_listener(uint16_t local_port) {
    for (int i = 0; i < TCB_COUNT; i++) {
        if (tcbs[i].active && tcbs[i].is_listener &&
            tcbs[i].state == TCP_LISTEN &&
            tcbs[i].local_port == local_port) return i;
    }
    return -1;
}

int tcp_state(int id) {
    if (id < 0 || id >= TCB_COUNT || !tcbs[id].active) return TCP_CLOSED;
    return tcbs[id].state;
}

void tcp_for_each(tcp_iter_fn fn, void* user) {
    if (!fn) return;
    for (int i = 0; i < TCB_COUNT; i++) {
        if (tcbs[i].active) fn(i, &tcbs[i], user);
    }
}

uint16_t tcp_checksum(const uint8_t src_ip[4], const uint8_t dst_ip[4],
                      const uint8_t* tcp_hdr_and_data, uint16_t len) {
    uint32_t sum = 0;
    sum += ((uint32_t)src_ip[0] << 8) | src_ip[1];
    sum += ((uint32_t)src_ip[2] << 8) | src_ip[3];
    sum += ((uint32_t)dst_ip[0] << 8) | dst_ip[1];
    sum += ((uint32_t)dst_ip[2] << 8) | dst_ip[3];
    sum += TCP_PROTO;
    sum += len;

    const uint8_t* p = tcp_hdr_and_data;
    uint16_t n = len;
    while (n > 1) {
        sum += ((uint32_t)p[0] << 8) | p[1];
        p += 2; n -= 2;
    }
    if (n == 1) sum += (uint32_t)p[0] << 8;

    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

static int build_and_send(const tcp_tcb_t* t, uint8_t flags,
                          uint32_t seq, uint32_t ack) {
    uint8_t frame[14 + 20 + 20];
    for (uint32_t i = 0; i < sizeof(frame); i++) frame[i] = 0;

    uint8_t dst_mac[6];
    if (!arp_resolve(t->remote_ip, 500, dst_mac)) {
        for (int i = 0; i < 6; i++) dst_mac[i] = 0xFF;
    }

    eth_header_t* eh = (eth_header_t*) frame;
    for (int i = 0; i < 6; i++) { eh->dst[i] = dst_mac[i]; eh->src[i] = net_mac()[i]; }
    eh->ethertype = htons(ETHERTYPE_IPV4);

    ipv4_header_t* ip = (ipv4_header_t*)(frame + 14);
    ip->ver_ihl    = 0x45;
    ip->tos        = 0;
    ip->total_len  = htons(20 + 20);
    ip->id         = htons(1);
    ip->flags_frag = 0;
    ip->ttl        = 64;
    ip->protocol   = TCP_PROTO;
    for (int i = 0; i < 4; i++) ip->src[i] = t->local_ip[i];
    for (int i = 0; i < 4; i++) ip->dst[i] = t->remote_ip[i];
    ip->checksum   = 0;
    ip->checksum   = htons(net_checksum(ip, 20));

    tcp_header_t* th = (tcp_header_t*)(frame + 14 + 20);
    th->src_port = htons(t->local_port);
    th->dst_port = htons(t->remote_port);
    th->seq      = htonl(seq);
    th->ack      = htonl(ack);
    th->data_off = (5 << 4);
    th->flags    = flags;
    th->window   = htons(8192);
    th->checksum = 0;
    th->urgent   = 0;
    uint16_t cs = tcp_checksum(t->local_ip, t->remote_ip, (const uint8_t*)th, 20);
    th->checksum = htons(cs);

    return rtl8139_send(frame, sizeof(frame));
}

// Send a RST in response to a stray segment.
static void send_rst_reply(const uint8_t src_ip[4], const uint8_t dst_ip[4],
                           uint16_t src_port, uint16_t dst_port,
                           uint32_t seg_seq, uint32_t seg_ack, uint8_t seg_flags,
                           uint16_t seg_payload_len) {
    tcp_tcb_t tmp = (tcp_tcb_t){0};
    for (int i = 0; i < 4; i++) { tmp.local_ip[i] = dst_ip[i]; tmp.remote_ip[i] = src_ip[i]; }
    tmp.local_port  = dst_port;
    tmp.remote_port = src_port;
    if (seg_flags & TCP_ACK) {
        build_and_send(&tmp, TCP_RST, seg_ack, 0);
    } else {
        uint32_t ack = seg_seq + seg_payload_len + ((seg_flags & TCP_SYN) ? 1 : 0)
                                                 + ((seg_flags & TCP_FIN) ? 1 : 0);
        build_and_send(&tmp, TCP_RST | TCP_ACK, 0, ack);
    }
}

static uint32_t be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
}

int tcp_connect(const uint8_t dst_ip[4], uint16_t dst_port) {
    int slot = -1;
    for (int i = 0; i < TCB_COUNT; i++) {
        if (!tcbs[i].active) { slot = i; break; }
    }
    if (slot < 0) return -1;

    tcp_tcb_t* t = &tcbs[slot];
    t->state = TCP_SYN_SENT;
    for (int i = 0; i < 4; i++) { t->local_ip[i] = net_ip()[i]; t->remote_ip[i] = dst_ip[i]; }
    if (next_eph_port > 60000) next_eph_port = 32768;
    t->local_port  = ++next_eph_port;
    t->remote_port = dst_port;
    t->snd_nxt     = (uint32_t)timer_uptime_ms();
    t->snd_una     = t->snd_nxt;
    t->rcv_nxt     = 0;
    t->retries     = 0;
    t->syn_sent_at_ms = timer_uptime_ms();
    t->active      = 1;
    t->is_listener = 0;
    t->accepted    = 0;

    int sent = build_and_send(t, TCP_SYN, t->snd_nxt, 0);
    t->snd_nxt += 1;
    if (sent < 0) { t->active = 0; t->state = TCP_CLOSED; return -1; }
    return slot;
}

int tcp_close(int id) {
    if (id < 0 || id >= TCB_COUNT || !tcbs[id].active) return -1;
    tcp_tcb_t* t = &tcbs[id];
    if (t->state == TCP_ESTABLISHED) {
        build_and_send(t, TCP_FIN | TCP_ACK, t->snd_nxt, t->rcv_nxt);
        t->snd_nxt += 1;
        t->state = TCP_FIN_WAIT_1;
        return 0;
    }
    if (t->state == TCP_CLOSE_WAIT) {
        build_and_send(t, TCP_FIN | TCP_ACK, t->snd_nxt, t->rcv_nxt);
        t->snd_nxt += 1;
        t->state = TCP_LAST_ACK;
        return 0;
    }
    t->state  = TCP_CLOSED;
    t->active = 0;
    return 0;
}

static tcp_tcb_t* find_tcb(const uint8_t remote_ip[4], uint16_t remote_port,
                           uint16_t local_port) {
    for (int i = 0; i < TCB_COUNT; i++) {
        if (!tcbs[i].active) continue;
        if (tcbs[i].local_port  != local_port)  continue;
        if (tcbs[i].remote_port != remote_port) continue;
        int m = 1;
        for (int j = 0; j < 4; j++) if (tcbs[i].remote_ip[j] != remote_ip[j]) { m = 0; break; }
        if (m) return &tcbs[i];
    }
    return 0;
}

int tcp_input(const uint8_t* frame, uint16_t len) {
    if (len < 14 + 20 + 20) return 0;
    if (!(frame[12] == 0x08 && frame[13] == 0x00)) return 0;
    const ipv4_header_t* ip = (const ipv4_header_t*)(frame + 14);
    if ((ip->ver_ihl & 0xF0) != 0x40) return 0;
    uint8_t ihl = (ip->ver_ihl & 0x0F) * 4;
    if (ihl < 20) return 0;
    if (ip->protocol != TCP_PROTO) return 0;
    if (14 + ihl + 20 > len) return 0;

    const tcp_header_t* th = (const tcp_header_t*)(frame + 14 + ihl);
    uint16_t sport = ntohs(th->src_port);
    uint16_t dport = ntohs(th->dst_port);
    uint32_t seg_seq = be32((const uint8_t*)&th->seq);
    uint32_t seg_ack = be32((const uint8_t*)&th->ack);
    uint8_t  flags   = th->flags;
    uint16_t hdr_len = (th->data_off >> 4) * 4;
    if (hdr_len < 20) return 1;
    uint16_t total_ip = ntohs(ip->total_len);
    if (total_ip < ihl + hdr_len) return 1;
    uint16_t payload = total_ip - ihl - hdr_len;

    tcp_tcb_t* t = find_tcb(ip->src, sport, dport);
    if (!t) {
        // No full-tuple match — but is there a listener on dport?
        int lid = find_listener(dport);
        if (lid >= 0 && (flags & TCP_SYN) && !(flags & TCP_ACK) && !(flags & TCP_RST)) {
            int child = alloc_tcb();
            if (child < 0) {
                send_rst_reply(ip->src, ip->dst, sport, dport,
                               seg_seq, seg_ack, flags, payload);
                return 1;
            }
            tcp_tcb_t* c = &tcbs[child];
            c->is_listener = 0;
            c->accepted    = 0;
            for (int i = 0; i < 4; i++) { c->local_ip[i] = ip->dst[i]; c->remote_ip[i] = ip->src[i]; }
            c->local_port  = dport;
            c->remote_port = sport;
            c->snd_nxt     = (uint32_t)timer_uptime_ms();
            c->snd_una     = c->snd_nxt;
            c->rcv_nxt     = seg_seq + 1;
            c->retries     = 0;
            c->syn_sent_at_ms = timer_uptime_ms();
            c->active      = 1;
            c->state       = TCP_SYN_RECEIVED;
            build_and_send(c, TCP_SYN | TCP_ACK, c->snd_nxt, c->rcv_nxt);
            c->snd_nxt += 1;
            return 1;
        }
        if (!(flags & TCP_RST)) {
            send_rst_reply(ip->src, ip->dst, sport, dport,
                           seg_seq, seg_ack, flags, payload);
        }
        return 1;
    }

    if (flags & TCP_RST) {
        t->state  = TCP_CLOSED;
        t->active = 0;
        return 1;
    }

    switch (t->state) {
    case TCP_SYN_RECEIVED:
        if ((flags & TCP_ACK) && seg_ack == t->snd_nxt) {
            t->snd_una = seg_ack;
            t->state   = TCP_ESTABLISHED;
        } else if (flags & TCP_SYN) {
            // Retransmitted SYN — resend SYN-ACK
            build_and_send(t, TCP_SYN | TCP_ACK, t->snd_una, t->rcv_nxt);
        }
        break;

    case TCP_SYN_SENT:
        if ((flags & (TCP_SYN | TCP_ACK)) == (TCP_SYN | TCP_ACK)) {
            if (seg_ack != t->snd_nxt) {
                build_and_send(t, TCP_RST, seg_ack, 0);
                t->state = TCP_CLOSED; t->active = 0;
                return 1;
            }
            t->snd_una = seg_ack;
            t->rcv_nxt = seg_seq + 1;
            build_and_send(t, TCP_ACK, t->snd_nxt, t->rcv_nxt);
            t->state = TCP_ESTABLISHED;
        } else if (flags & TCP_SYN) {
            // Simultaneous open — not supported this phase; reset.
            build_and_send(t, TCP_RST, t->snd_nxt, 0);
            t->state = TCP_CLOSED; t->active = 0;
        }
        break;

    case TCP_ESTABLISHED:
        if (flags & TCP_ACK) t->snd_una = seg_ack;
        if (flags & TCP_FIN) {
            t->rcv_nxt = seg_seq + 1;
            build_and_send(t, TCP_ACK, t->snd_nxt, t->rcv_nxt);
            t->state = TCP_CLOSE_WAIT;
        }
        break;

    case TCP_FIN_WAIT_1:
        if (flags & TCP_ACK) t->snd_una = seg_ack;
        if (flags & TCP_FIN) {
            t->rcv_nxt = seg_seq + 1;
            build_and_send(t, TCP_ACK, t->snd_nxt, t->rcv_nxt);
            t->state = TCP_TIME_WAIT;
            t->syn_sent_at_ms = timer_uptime_ms();
        } else if ((flags & TCP_ACK) && seg_ack == t->snd_nxt) {
            t->state = TCP_FIN_WAIT_2;
        }
        break;

    case TCP_FIN_WAIT_2:
        if (flags & TCP_FIN) {
            t->rcv_nxt = seg_seq + 1;
            build_and_send(t, TCP_ACK, t->snd_nxt, t->rcv_nxt);
            t->state = TCP_TIME_WAIT;
            t->syn_sent_at_ms = timer_uptime_ms();
        }
        break;

    case TCP_LAST_ACK:
        if ((flags & TCP_ACK) && seg_ack == t->snd_nxt) {
            t->state = TCP_CLOSED;
            t->active = 0;
        }
        break;

    default: break;
    }
    return 1;
}

void tcp_tick(void) {
    uint64_t now = timer_uptime_ms();
    for (int i = 0; i < TCB_COUNT; i++) {
        if (!tcbs[i].active) continue;
        tcp_tcb_t* t = &tcbs[i];

        if (t->state == TCP_SYN_SENT) {
            uint64_t delta = now - t->syn_sent_at_ms;
            uint64_t retry_at = (uint64_t)1000 << t->retries;   // 1s, 2s, 4s
            if (delta >= retry_at) {
                if (t->retries >= SYN_RETRY_MAX) {
                    t->state = TCP_CLOSED;
                    t->active = 0;
                } else {
                    t->retries++;
                    build_and_send(t, TCP_SYN, t->snd_una, 0);
                    t->syn_sent_at_ms = now;
                }
            }
        } else if (t->state == TCP_TIME_WAIT) {
            if (now - t->syn_sent_at_ms >= 2000) {
                t->state  = TCP_CLOSED;
                t->active = 0;
            }
        }
    }
}

