#include "security.h"

static user_t users[MAX_USERS];
static int    user_count = 0;
static int    logged_in  = -1;

static void scpy(char* d, const char* s, int max) {
    int i = 0;
    while (s[i] && i < max - 1) { d[i] = s[i]; i++; }
    d[i] = '\0';
}
static int scmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a - *b;
}

/* Store password as simple XOR-obfuscated bytes — no SHA256, no deep stack */
static void store_pw(const char* password, uint8_t out[32]) {
    int i = 0;
    while (password[i] && i < 32) {
        out[i] = (uint8_t)(password[i] ^ 0xA5);
        i++;
    }
    for (; i < 32; i++) out[i] = 0;
}

static int check_pw(const char* password, const uint8_t stored[32]) {
    int i = 0;
    while (password[i] && i < 32) {
        if ((uint8_t)(password[i] ^ 0xA5) != stored[i]) return 0;
        i++;
    }
    return stored[i] == 0;
}

int user_add(const char* name, const char* password, uint8_t admin) {
    if (user_count >= MAX_USERS) return -1;
    for (int i = 0; i < user_count; i++)
        if (scmp(users[i].name, name) == 0) return -1;
    user_t* u = &users[user_count];
    scpy(u->name, name, USER_NAME_MAX);
    store_pw(password, u->pw_hash);
    u->uid      = 1000 + user_count;
    u->gid      = admin ? 0 : 100;
    u->is_admin = admin;
    u->active   = 1;
    user_count++;
    return 0;
}

void security_init() {
    user_count = 0;
    logged_in  = -1;
    user_add("root",  "toor",  1);
    user_add("guest", "guest", 0);
    for (int i = 0; i < user_count; i++)
        if (scmp(users[i].name, "guest") == 0) logged_in = i;
}

int auth_login(const char* name, const char* password) {
    for (int i = 0; i < user_count; i++) {
        if (scmp(users[i].name, name) == 0) {
            if (check_pw(password, users[i].pw_hash)) { logged_in = i; return 0; }
            return -1;
        }
    }
    return -2;
}

void auth_logout() {
    for (int i = 0; i < user_count; i++)
        if (scmp(users[i].name, "guest") == 0) { logged_in = i; return; }
    logged_in = -1;
}

const char* current_user() {
    if (logged_in < 0) return "guest";
    return users[logged_in].name;
}

int current_is_admin() {
    if (logged_in < 0) return 0;
    return users[logged_in].is_admin;
}

int user_list(user_t** out) {
    *out = users;
    return user_count;
}
