#include "ipv6.h"
#include "net.h"
#include "../drivers/rtl8139.h"

static uint8_t local_addr[16];

void ipv6_init(void) {
    for (int i = 0; i < 16; i++) local_addr[i] = 0;
    local_addr[0] = 0xFE; local_addr[1] = 0x80;
    const uint8_t* mac = net_mac();

    local_addr[ 8] = mac[0] ^ 0x02;
    local_addr[ 9] = mac[1];
    local_addr[10] = mac[2];
    local_addr[11] = 0xFF;
    local_addr[12] = 0xFE;
    local_addr[13] = mac[3];
    local_addr[14] = mac[4];
    local_addr[15] = mac[5];
}

const uint8_t* ipv6_local_addr(void) { return local_addr; }

static uint16_t icmpv6_checksum(const uint8_t src[16], const uint8_t dst[16],
                                const uint8_t* msg, uint32_t len) {
    uint32_t sum = 0;
    for (int i = 0; i < 16; i += 2) sum += ((uint32_t)src[i] << 8) | src[i+1];
    for (int i = 0; i < 16; i += 2) sum += ((uint32_t)dst[i] << 8) | dst[i+1];
    sum += (len >> 16) & 0xFFFF;
    sum += len & 0xFFFF;
    sum += 58;
    for (uint32_t i = 0; i + 1 < len; i += 2) {
        sum += ((uint32_t)msg[i] << 8) | msg[i+1];
    }
    if (len & 1) sum += (uint32_t)msg[len - 1] << 8;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

static int addr_eq(const uint8_t* a, const uint8_t* b) {
    for (int i = 0; i < 16; i++) if (a[i] != b[i]) return 0;
    return 1;
}

int ipv6_input(const uint8_t* frame, uint16_t len) {
    if (len < 14 + 40) return 0;
    if (!(frame[12] == 0x86 && frame[13] == 0xDD)) return 0;
    const ipv6_header_t* ip = (const ipv6_header_t*)(frame + 14);
    if ((ip->ver_tc_fl[0] >> 4) != 6) return 0;
    uint16_t payload_len = ((uint16_t)ip->payload_len) >> 0;
    payload_len = ((payload_len & 0xFF) << 8) | ((payload_len >> 8) & 0xFF);
    if (14 + 40 + payload_len > len) return 1;


    if (ip->next_header != 58) return 1;
    if (!addr_eq(ip->dst, local_addr)) return 1;

    const icmpv6_header_t* ic = (const icmpv6_header_t*)(frame + 14 + 40);



    if (ic->type == 135 && payload_len >= 24) {
        const uint8_t* target = (const uint8_t*)ic + 8;



        static uint8_t reply[14 + 40 + 32];
        for (int i = 0; i < 6; i++) reply[i]     = frame[6 + i];
        for (int i = 0; i < 6; i++) reply[6 + i] = net_mac()[i];
        reply[12] = 0x86; reply[13] = 0xDD;
        ipv6_header_t* ip2 = (ipv6_header_t*)(reply + 14);
        for (int i = 0; i < 4; i++) ip2->ver_tc_fl[i] = 0;
        ip2->ver_tc_fl[0] = 0x60;
        uint16_t na_payload = 32;
        ip2->payload_len = (uint16_t)((na_payload << 8) | (na_payload >> 8));
        ip2->next_header = 58;
        ip2->hop_limit   = 255;
        for (int i = 0; i < 16; i++) ip2->src[i] = local_addr[i];
        for (int i = 0; i < 16; i++) ip2->dst[i] = ip->src[i];
        uint8_t* na = reply + 14 + 40;
        na[0] = 136; na[1] = 0; na[2] = 0; na[3] = 0;
        na[4] = 0x60; na[5] = na[6] = na[7] = 0;
        for (int i = 0; i < 16; i++) na[8 + i] = target[i];
        na[24] = 2; na[25] = 1;
        for (int i = 0; i < 6; i++) na[26 + i] = net_mac()[i];

        ((icmpv6_header_t*)na)->checksum = 0;
        uint16_t cs = icmpv6_checksum(ip2->src, ip2->dst, na, na_payload);
        ((icmpv6_header_t*)na)->checksum = htons(cs);
        rtl8139_send(reply, sizeof(reply));
        return 1;
    }

    if (ic->type != ICMPV6_ECHO_REQUEST) return 1;


    static uint8_t reply[1600];
    if (14 + 40 + payload_len > sizeof(reply)) return 1;
    for (uint32_t i = 0; i < 14 + 40 + payload_len; i++) reply[i] = frame[i];

    for (int i = 0; i < 6; i++) reply[i]     = frame[6 + i];
    for (int i = 0; i < 6; i++) reply[6 + i] = frame[i];

    ipv6_header_t* ip2 = (ipv6_header_t*)(reply + 14);
    for (int i = 0; i < 16; i++) ip2->src[i] = ip->dst[i];
    for (int i = 0; i < 16; i++) ip2->dst[i] = ip->src[i];
    icmpv6_header_t* ic2 = (icmpv6_header_t*)(reply + 14 + 40);
    ic2->type = ICMPV6_ECHO_REPLY;
    ic2->checksum = 0;
    uint16_t cs = icmpv6_checksum(ip2->src, ip2->dst, (uint8_t*)ic2, payload_len);
    ic2->checksum = htons(cs);
    rtl8139_send(reply, (uint16_t)(14 + 40 + payload_len));
    return 1;
}
