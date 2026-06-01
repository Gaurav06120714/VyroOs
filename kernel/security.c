#include "security.h"
#include "sha256.h"

static user_t users[MAX_USERS];
static int    user_count = 0;
static int    logged_in  = -1;   // index of current user, -1 = guest

// ─────────────────────────────────────────────────
// string helpers
// ─────────────────────────────────────────────────
static void scpy(char* d, const char* s, int max) {
    int i = 0;
    while (s[i] && i < max - 1) { d[i] = s[i]; i++; }
    d[i] = '\0';
}
static int scmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a - *b;
}
static int slen(const char* s) { int i = 0; while (s[i]) i++; return i; }

// ─────────────────────────────────────────────────
// hash_password: SHA-256(salt || password)
// ─────────────────────────────────────────────────
static void hash_password(const char* salt, const char* password, uint8_t out[32]) {
    uint8_t buf[128];
    int n = 0;
    for (int i = 0; salt[i] && n < 120; i++)     buf[n++] = (uint8_t)salt[i];
    for (int i = 0; password[i] && n < 120; i++) buf[n++] = (uint8_t)password[i];
    sha256(buf, n, out);
}

// ─────────────────────────────────────────────────
// user_add: create a new account
// ─────────────────────────────────────────────────
int user_add(const char* name, const char* password, uint8_t admin) {
    if (user_count >= MAX_USERS) return -1;
    // No duplicates
    for (int i = 0; i < user_count; i++)
        if (scmp(users[i].name, name) == 0) return -1;

    user_t* u = &users[user_count];
    scpy(u->name, name, USER_NAME_MAX);

    // Derive a simple per-user salt from the name + index
    char salt[16];
    int s = 0;
    salt[s++] = 'v'; salt[s++] = 'y';
    for (int i = 0; name[i] && s < 14; i++) salt[s++] = name[i] ^ 0x5A;
    salt[s] = '\0';
    scpy(u->salt, salt, 16);

    hash_password(u->salt, password, u->pw_hash);
    u->uid      = 1000 + user_count;
    u->gid      = admin ? 0 : 100;
    u->is_admin = admin;
    u->active   = 1;
    user_count++;
    return 0;
}

// ─────────────────────────────────────────────────
// security_init: seed default accounts
// ─────────────────────────────────────────────────
void security_init() {
    user_count = 0;
    logged_in  = -1;
    user_add("root",  "toor",  1);   // admin
    user_add("guest", "guest", 0);   // standard user
    // Default session: guest
    for (int i = 0; i < user_count; i++)
        if (scmp(users[i].name, "guest") == 0) logged_in = i;
}

// ─────────────────────────────────────────────────
// auth_login: verify credentials, set current user
// ─────────────────────────────────────────────────
int auth_login(const char* name, const char* password) {
    for (int i = 0; i < user_count; i++) {
        if (scmp(users[i].name, name) == 0) {
            uint8_t h[32];
            hash_password(users[i].salt, password, h);
            int match = 1;
            for (int j = 0; j < 32; j++) if (h[j] != users[i].pw_hash[j]) match = 0;
            if (match) { logged_in = i; return 0; }
            return -1;   // wrong password
        }
    }
    return -2;   // no such user
}

void auth_logout() {
    // Drop back to guest
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
