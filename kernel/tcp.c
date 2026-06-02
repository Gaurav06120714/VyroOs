#include "tcp.h"
#include "net.h"
#include "arp.h"
#include "../drivers/rtl8139.h"
#include "../drivers/timer.h"

#define TCB_COUNT      16
#define TCP_PROTO      6
#define TCP_MSS        536
#define SYN_RETRY_MAX  3
#define TCP_BUF_SIZE        1024
#define TCP_DATA_RTO_MS     1000     // initial RTO before any RTT sample
#define TCP_RTO_MIN_MS       200
#define TCP_RTO_MAX_MS      5000
#define TCP_DUP_ACK_THRESH     3

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

// Per-TCB linear buffers. snd_buf holds outstanding (unacked) data starting at
// sequence snd_una; rcv_buf holds bytes received in order but not yet drained.
static uint8_t   snd_buf[TCB_COUNT][TCP_BUF_SIZE];
static uint16_t  snd_len[TCB_COUNT];
static uint64_t  snd_oldest_ms[TCB_COUNT];
static uint8_t   rcv_buf[TCB_COUNT][TCP_BUF_SIZE];
static uint16_t  rcv_len[TCB_COUNT];

// Single-slot out-of-order reassembly buffer per TCB.
static uint8_t   ooo_buf[TCB_COUNT][TCP_MSS];
static uint16_t  ooo_len[TCB_COUNT];
static uint32_t  ooo_seq[TCB_COUNT];

void tcp_init(void) {
    for (int i = 0; i < TCB_COUNT; i++) {
        tcbs[i].state       = TCP_CLOSED;
        tcbs[i].active      = 0;
        tcbs[i].is_listener = 0;
        tcbs[i].accepted    = 0;
        snd_len[i] = 0;
        rcv_len[i] = 0;
        ooo_len[i] = 0;
        snd_oldest_ms[i] = 0;
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
    snd_len[id] = 0;
    rcv_len[id] = 0;
    ooo_len[id] = 0;
    snd_oldest_ms[id] = 0;
    t->srtt_ms = 0; t->rttvar_ms = 0; t->rto_ms = TCP_DATA_RTO_MS;
    t->rtt_in_flight = 0; t->last_ack_recv = 0; t->dup_acks = 0;
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

static int build_and_send_data(const tcp_tcb_t* t, uint8_t flags,
                               uint32_t seq, uint32_t ack,
                               const uint8_t* data, uint16_t dlen);
static void arm_rtt_probe(tcp_tcb_t* t, uint32_t seq_being_sent);

int tcp_send(int id, const uint8_t* data, uint16_t len) {
    if (id < 0 || id >= TCB_COUNT) return -1;
    tcp_tcb_t* t = &tcbs[id];
    if (!t->active || t->state != TCP_ESTABLISHED) return -1;
    uint16_t free_space = TCP_BUF_SIZE - snd_len[id];
    if (len > free_space) len = free_space;
    if (len == 0) return 0;

    // Append to snd_buf.
    for (uint16_t i = 0; i < len; i++) snd_buf[id][snd_len[id] + i] = data[i];
    uint16_t off_in_buf = snd_len[id];      // where the new bytes start
    snd_len[id] += len;
    if (snd_oldest_ms[id] == 0) snd_oldest_ms[id] = timer_uptime_ms();

    // Transmit the new bytes immediately, MSS at a time.
    uint16_t emitted = 0;
    while (emitted < len) {
        uint16_t chunk = len - emitted;
        if (chunk > TCP_MSS) chunk = TCP_MSS;
        arm_rtt_probe(t, t->snd_nxt);
        build_and_send_data(t, TCP_ACK | TCP_PSH,
                            t->snd_nxt, t->rcv_nxt,
                            &snd_buf[id][off_in_buf + emitted], chunk);
        t->snd_nxt += chunk;
        emitted    += chunk;
    }
    return len;
}

int tcp_recv(int id, uint8_t* out, uint16_t max) {
    if (id < 0 || id >= TCB_COUNT) return -1;
    if (!tcbs[id].active) return -1;
    uint16_t n = rcv_len[id];
    if (n > max) n = max;
    if (n == 0) return 0;
    for (uint16_t i = 0; i < n; i++) out[i] = rcv_buf[id][i];
    // Shift remainder forward.
    for (uint16_t i = 0; i < rcv_len[id] - n; i++) rcv_buf[id][i] = rcv_buf[id][n + i];
    rcv_len[id] -= n;
    return n;
}

// Called from tcp_input when a peer ACKs new bytes.
static void slide_snd_window(int id, uint32_t acked) {
    if (acked == 0 || acked > snd_len[id]) {
        // Nothing acknowledged, or peer over-ACKed (should not happen with strict RFC)
        if (acked > snd_len[id]) acked = snd_len[id];
        else return;
    }
    for (uint16_t i = 0; i < snd_len[id] - acked; i++) snd_buf[id][i] = snd_buf[id][acked + i];
    snd_len[id] -= acked;
    if (snd_len[id] == 0) snd_oldest_ms[id] = 0;
    else                   snd_oldest_ms[id] = timer_uptime_ms();
}

static int tcb_index(const tcp_tcb_t* t) {
    return (int)(t - tcbs);
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

static int build_and_send_data(const tcp_tcb_t* t, uint8_t flags,
                               uint32_t seq, uint32_t ack,
                               const uint8_t* data, uint16_t dlen) {
    uint8_t frame[14 + 20 + 20 + TCP_MSS];
    if (dlen > TCP_MSS) dlen = TCP_MSS;
    for (uint32_t i = 0; i < 14 + 20 + 20; i++) frame[i] = 0;

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
    ip->total_len  = htons(20 + 20 + dlen);
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
    th->window   = htons(TCP_BUF_SIZE);
    th->checksum = 0;
    th->urgent   = 0;

    uint8_t* payload = frame + 14 + 20 + 20;
    for (uint16_t i = 0; i < dlen; i++) payload[i] = data[i];

    uint16_t cs = tcp_checksum(t->local_ip, t->remote_ip, (const uint8_t*)th, 20 + dlen);
    th->checksum = htons(cs);

    return rtl8139_send(frame, 14 + 20 + 20 + dlen);
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
    snd_len[slot] = 0;
    rcv_len[slot] = 0;
    ooo_len[slot] = 0;
    snd_oldest_ms[slot] = 0;
    t->srtt_ms = 0; t->rttvar_ms = 0; t->rto_ms = TCP_DATA_RTO_MS;
    t->rtt_in_flight = 0; t->last_ack_recv = 0; t->dup_acks = 0;

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

// RFC 6298 RTT estimator. Updates srtt/rttvar/rto on a fresh sample.
static void rtt_sample(tcp_tcb_t* t, uint32_t sample_ms) {
    if (sample_ms == 0) sample_ms = 1;
    if (t->srtt_ms == 0) {
        // First measurement
        t->srtt_ms   = sample_ms;
        t->rttvar_ms = sample_ms / 2;
    } else {
        // RFC 6298: RTTVAR <- (1-beta)*RTTVAR + beta*|SRTT - sample|, beta=1/4
        //          SRTT   <- (1-alpha)*SRTT + alpha*sample, alpha=1/8
        uint32_t diff = (t->srtt_ms > sample_ms) ? t->srtt_ms - sample_ms
                                                 : sample_ms - t->srtt_ms;
        t->rttvar_ms = (3 * t->rttvar_ms + diff) / 4;
        t->srtt_ms   = (7 * t->srtt_ms + sample_ms) / 8;
    }
    uint32_t rto = t->srtt_ms + 4 * t->rttvar_ms;
    if (rto < TCP_RTO_MIN_MS) rto = TCP_RTO_MIN_MS;
    if (rto > TCP_RTO_MAX_MS) rto = TCP_RTO_MAX_MS;
    t->rto_ms = rto;
}

static void arm_rtt_probe(tcp_tcb_t* t, uint32_t seq_being_sent) {
    if (t->rtt_in_flight) return;   // already timing one
    t->rtt_in_flight     = 1;
    t->rtt_probe_seq     = seq_being_sent;
    t->rtt_probe_sent_ms = timer_uptime_ms();
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
            snd_len[child] = 0;
            rcv_len[child] = 0;
            ooo_len[child] = 0;
            snd_oldest_ms[child] = 0;
            c->srtt_ms = 0; c->rttvar_ms = 0; c->rto_ms = TCP_DATA_RTO_MS;
            c->rtt_in_flight = 0; c->last_ack_recv = 0; c->dup_acks = 0;
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

    case TCP_ESTABLISHED: {
        int id = tcb_index(t);
        if (flags & TCP_ACK) {
            uint32_t newly_acked = seg_ack - t->snd_una;
            if (newly_acked > 0 && newly_acked <= snd_len[id]) {
                // RTT sample: if the probed segment is now fully ACKed
                if (t->rtt_in_flight &&
                    (int32_t)(seg_ack - t->rtt_probe_seq) > 0) {
                    uint32_t sample = (uint32_t)(timer_uptime_ms() - t->rtt_probe_sent_ms);
                    rtt_sample(t, sample);
                    t->rtt_in_flight = 0;
                }
                slide_snd_window(id, newly_acked);
                t->snd_una     = seg_ack;
                t->dup_acks    = 0;
                t->last_ack_recv = seg_ack;
            } else if (newly_acked == 0 && snd_len[id] > 0 && payload == 0 &&
                       !(flags & (TCP_SYN | TCP_FIN))) {
                // Duplicate ACK
                if (seg_ack == t->last_ack_recv) {
                    t->dup_acks++;
                } else {
                    t->dup_acks      = 1;
                    t->last_ack_recv = seg_ack;
                }
                if (t->dup_acks == TCP_DUP_ACK_THRESH) {
                    // Fast retransmit: re-emit the segment at snd_una
                    uint16_t chunk = snd_len[id] < TCP_MSS ? snd_len[id] : TCP_MSS;
                    build_and_send_data(t, TCP_ACK | TCP_PSH,
                                        t->snd_una, t->rcv_nxt, &snd_buf[id][0], chunk);
                    // Halve RTO floor as a crude congestion signal
                    if (t->rto_ms > TCP_RTO_MIN_MS) {
                        t->rto_ms = t->rto_ms / 2;
                        if (t->rto_ms < TCP_RTO_MIN_MS) t->rto_ms = TCP_RTO_MIN_MS;
                    }
                }
            }
        }
        if (payload > 0) {
            const uint8_t* pdata = (const uint8_t*)th + hdr_len;
            if (seg_seq == t->rcv_nxt) {
                // In-order: deliver to recv buf, drain OoO slot if it now fits
                uint16_t free_space = TCP_BUF_SIZE - rcv_len[id];
                uint16_t to_copy = payload;
                if (to_copy > free_space) to_copy = free_space;
                for (uint16_t i = 0; i < to_copy; i++) rcv_buf[id][rcv_len[id] + i] = pdata[i];
                rcv_len[id] += to_copy;
                t->rcv_nxt  += to_copy;

                // If the stored OoO segment is now contiguous, drain it.
                if (ooo_len[id] && ooo_seq[id] == t->rcv_nxt) {
                    uint16_t fs = TCP_BUF_SIZE - rcv_len[id];
                    uint16_t cp = ooo_len[id];
                    if (cp > fs) cp = fs;
                    for (uint16_t i = 0; i < cp; i++) rcv_buf[id][rcv_len[id] + i] = ooo_buf[id][i];
                    rcv_len[id] += cp;
                    t->rcv_nxt  += cp;
                    ooo_len[id]  = 0;
                }
                build_and_send(t, TCP_ACK, t->snd_nxt, t->rcv_nxt);
            } else if ((int32_t)(seg_seq - t->rcv_nxt) > 0) {
                // Future segment: store in single OoO slot (overwrite if newer or none)
                if (payload <= TCP_MSS &&
                    (ooo_len[id] == 0 || seg_seq < ooo_seq[id])) {
                    for (uint16_t i = 0; i < payload; i++) ooo_buf[id][i] = pdata[i];
                    ooo_len[id] = payload;
                    ooo_seq[id] = seg_seq;
                }
                // Duplicate ACK to signal the gap
                build_and_send(t, TCP_ACK, t->snd_nxt, t->rcv_nxt);
            } else {
                // Old / already-acked data — re-ACK
                build_and_send(t, TCP_ACK, t->snd_nxt, t->rcv_nxt);
            }
        }
        if (flags & TCP_FIN) {
            t->rcv_nxt = seg_seq + payload + 1;
            build_and_send(t, TCP_ACK, t->snd_nxt, t->rcv_nxt);
            t->state = TCP_CLOSE_WAIT;
        }
        break;
    }

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
        } else if (t->state == TCP_ESTABLISHED && snd_len[i] > 0) {
            uint32_t rto = t->rto_ms ? t->rto_ms : TCP_DATA_RTO_MS;
            if (snd_oldest_ms[i] && now - snd_oldest_ms[i] >= rto) {
                // Retransmit everything in the unacked window, MSS at a time.
                uint32_t seq = t->snd_una;
                uint16_t off = 0;
                while (off < snd_len[i]) {
                    uint16_t chunk = snd_len[i] - off;
                    if (chunk > TCP_MSS) chunk = TCP_MSS;
                    build_and_send_data(t, TCP_ACK | TCP_PSH, seq, t->rcv_nxt,
                                        &snd_buf[i][off], chunk);
                    seq += chunk;
                    off += chunk;
                }
                snd_oldest_ms[i] = now;
                // Karn: a retransmitted segment cannot be used for RTT sampling.
                t->rtt_in_flight = 0;
                // Exponential back-off, capped.
                uint32_t back = t->rto_ms * 2;
                if (back > TCP_RTO_MAX_MS) back = TCP_RTO_MAX_MS;
                t->rto_ms = back;
            }
        }
    }
}

