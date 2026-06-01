#include "app.h"

static const app_def_t* apps[MAX_APPS];
static int              count = 0;

static int sa(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; } return *a - *b;
}

void app_register(const app_def_t* def) {
    if (count >= MAX_APPS) return;
    apps[count++] = def;
}

int app_count() { return count; }
const app_def_t* app_get(int i) {
    if (i < 0 || i >= count) return 0;
    return apps[i];
}
const app_def_t* app_find(const char* name) {
    for (int i = 0; i < count; i++)
        if (sa(apps[i]->name, name) == 0) return apps[i];
    return 0;
}
