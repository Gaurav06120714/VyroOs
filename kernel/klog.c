#include "klog.h"

static char    buffer[KLOG_MAX][80];
static int     count = 0;

void klog(const char* msg) {
    if (count >= KLOG_MAX) {
        // shift up (drop oldest)
        for (int i = 1; i < KLOG_MAX; i++)
            for (int j = 0; j < 80; j++) buffer[i-1][j] = buffer[i][j];
        count = KLOG_MAX - 1;
    }
    int i = 0;
    while (msg[i] && i < 79) { buffer[count][i] = msg[i]; i++; }
    buffer[count][i] = '\0';
    count++;
}

int klog_count() { return count; }

const char* klog_get(int i) {
    if (i < 0 || i >= count) return "";
    return buffer[i];
}
