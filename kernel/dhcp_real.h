#ifndef DHCP_REAL_H
#define DHCP_REAL_H

#include "../include/types.h"

int dhcp_real_run(uint32_t timeout_ms);

const uint8_t* dhcp_real_ip();
const uint8_t* dhcp_real_gw();
const uint8_t* dhcp_real_dns();

#endif
