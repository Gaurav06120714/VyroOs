#include "dns.h"

typedef struct { const char* host; uint8_t ip[4]; } host_entry_t;
static host_entry_t hosts[] = {
    { "localhost",      { 127,0,0,1 } },
    { "vyro.local",     {  10,0,2,15 } },
    { "router",         {  10,0,2,2  } },
    { "gateway",        {  10,0,2,2  } },
    { "dns",            {  10,0,2,3  } },
    { "github.com",     {140,82,114,4} },
    { "google.com",     {142,250,80,46} },
    { 0, {0,0,0,0} }
};

static int s_eq(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; } return *a == *b;
}

int dns_resolve(const char* hostname, uint8_t ip_out[4]) {
    for (int i = 0; hosts[i].host; i++) {
        if (s_eq(hosts[i].host, hostname)) {
            for (int j = 0; j < 4; j++) ip_out[j] = hosts[i].ip[j];
            return 0;
        }
    }
    return -1;
}
