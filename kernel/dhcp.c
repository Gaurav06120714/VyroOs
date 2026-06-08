#include "dhcp.h"

static uint8_t my_ip[4]      = { 10, 0, 2, 15 };
static uint8_t gateway_ip[4] = { 10, 0, 2,  2 };
static uint8_t dns_ip[4]     = { 10, 0, 2,  3 };
static uint8_t active        = 1;

void dhcp_init() {

    active = 1;
}

uint8_t dhcp_lease_active() { return active; }
const uint8_t* dhcp_offered_ip() { return my_ip; }
const uint8_t* dhcp_gateway()    { return gateway_ip; }
const uint8_t* dhcp_dns()        { return dns_ip; }
