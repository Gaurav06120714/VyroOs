#include "notify.h"
#include "../drivers/timer.h"

static notification_t notes[MAX_NOTIFICATIONS];
static notification_t history[MAX_NOTIFICATIONS * 4];
static int            hist_count = 0;
#define LIFETIME_TICKS 400

static void scpy(char* d, const char* s, int max) {
    int i = 0; while (s[i] && i < max - 1) { d[i] = s[i]; i++; } d[i] = '\0';
}

void notify_post(const char* title, const char* body) {
    for (int i = 0; i < MAX_NOTIFICATIONS; i++) {
        if (!notes[i].alive) {
            scpy(notes[i].title, title, sizeof(notes[i].title));
            scpy(notes[i].body,  body,  sizeof(notes[i].body));
            notes[i].shown_at_tick = timer_ticks();
            notes[i].alive = 1;

            int hi = hist_count % (MAX_NOTIFICATIONS * 4);
            history[hi] = notes[i];
            hist_count++;
            return;
        }
    }
}

void notify_tick() {
    uint64_t now = timer_ticks();
    for (int i = 0; i < MAX_NOTIFICATIONS; i++) {
        if (notes[i].alive && now - notes[i].shown_at_tick > LIFETIME_TICKS)
            notes[i].alive = 0;
    }
}

int notify_active(notification_t** out) {
    *out = notes;
    int n = 0;
    for (int i = 0; i < MAX_NOTIFICATIONS; i++) if (notes[i].alive) n++;
    return n;
}

void notify_history(notification_t** out, int* count) {
    *out   = history;
    *count = hist_count > MAX_NOTIFICATIONS*4 ? MAX_NOTIFICATIONS*4 : hist_count;
}
