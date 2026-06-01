#include "ipc.h"

static pipe_t    pipes[MAX_PIPES];
static message_t mq[MAX_MSGS];
static int       signals_sent = 0;

int pipe_create() {
    for (int i = 0; i < MAX_PIPES; i++) {
        if (!pipes[i].in_use) {
            pipes[i].in_use = 1;
            pipes[i].read_pos = 0;
            pipes[i].write_pos = 0;
            pipes[i].count = 0;
            return i;
        }
    }
    return -1;
}

int pipe_write(int pid, const uint8_t* data, uint32_t n) {
    if (pid < 0 || pid >= MAX_PIPES || !pipes[pid].in_use) return -1;
    pipe_t* p = &pipes[pid];
    uint32_t written = 0;
    while (written < n && p->count < PIPE_BUF_SIZE) {
        p->buf[p->write_pos] = data[written++];
        p->write_pos = (p->write_pos + 1) % PIPE_BUF_SIZE;
        p->count++;
    }
    return (int)written;
}

int pipe_read(int pid, uint8_t* out, uint32_t n) {
    if (pid < 0 || pid >= MAX_PIPES || !pipes[pid].in_use) return -1;
    pipe_t* p = &pipes[pid];
    uint32_t got = 0;
    while (got < n && p->count > 0) {
        out[got++] = p->buf[p->read_pos];
        p->read_pos = (p->read_pos + 1) % PIPE_BUF_SIZE;
        p->count--;
    }
    return (int)got;
}

int pipe_count() {
    int n = 0;
    for (int i = 0; i < MAX_PIPES; i++) if (pipes[i].in_use) n++;
    return n;
}

int mq_send(int id, const char* payload) {
    for (int i = 0; i < MAX_MSGS; i++) {
        if (!mq[i].alive) {
            mq[i].id = id;
            int j = 0;
            while (payload[j] && j < 119) { mq[i].payload[j] = payload[j]; j++; }
            mq[i].payload[j] = 0;
            mq[i].alive = 1;
            return 0;
        }
    }
    return -1;
}

int mq_recv(char* out_payload) {
    for (int i = 0; i < MAX_MSGS; i++) {
        if (mq[i].alive) {
            int j = 0;
            while (mq[i].payload[j]) { out_payload[j] = mq[i].payload[j]; j++; }
            out_payload[j] = 0;
            mq[i].alive = 0;
            return 0;
        }
    }
    return -1;
}

int mq_count() {
    int n = 0;
    for (int i = 0; i < MAX_MSGS; i++) if (mq[i].alive) n++;
    return n;
}

void signal_send(int target_pid, int sig) {
    (void)target_pid; (void)sig;
    signals_sent++;
}

int signal_count() { return signals_sent; }

const char* signal_name(int sig) {
    switch (sig) {
        case SIGTERM: return "SIGTERM";
        case SIGKILL: return "SIGKILL";
        case SIGINT:  return "SIGINT";
        case SIGUSR1: return "SIGUSR1";
        default:      return "UNKNOWN";
    }
}
