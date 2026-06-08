#ifndef SECURITY_H
#define SECURITY_H

#include "../include/types.h"

#define MAX_USERS    8
#define USER_NAME_MAX 32

typedef struct {
    char     name[USER_NAME_MAX];
    char     salt[16];
    uint8_t  pw_hash[32];
    uint32_t uid;
    uint32_t gid;
    uint8_t  is_admin;
    uint8_t  active;
} user_t;

void        security_init();
int         user_add(const char* name, const char* password, uint8_t admin);
int         auth_login(const char* name, const char* password);
void        auth_logout();
const char* current_user();
int         current_is_admin();
int         user_list(user_t** out);

#endif
