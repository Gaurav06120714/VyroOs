#include "dhcp.h"

// Simulated DHCP lease — would come from a real DHCP exchange in v3.0.
// State and structures are correct; transport is stub (no NIC TX yet).
static uint8_t my_ip[4]      = { 10, 0, 2, 15 };
static uint8_t gateway_ip[4] = { 10, 0, 2,  2 };
static uint8_t dns_ip[4]     = { 10, 0, 2,  3 };
static uint8_t active        = 1;

void dhcp_init() {
    // Real version: build DHCPDISCOVER → UDP/68→67 broadcast → wait OFFER → REQUEST → ACK
    active = 1;
}

uint8_t dhcp_lease_active() { return active; }
const uint8_t* dhcp_offered_ip() { return my_ip; }
const uint8_t* dhcp_gateway()    { return gateway_ip; }
const uint8_t* dhcp_dns()        { return dns_ip; }
