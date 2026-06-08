#ifndef IPC_H
#define IPC_H

#include "../include/types.h"

#define PIPE_BUF_SIZE   1024
#define MAX_PIPES       16
#define MAX_MSGS        32

typedef struct {
    uint8_t  in_use;
    uint8_t  buf[PIPE_BUF_SIZE];
    uint32_t read_pos;
    uint32_t write_pos;
    uint32_t count;
} pipe_t;

typedef struct {
    int      id;
    char     payload[120];
    uint8_t  alive;
} message_t;

#define SIGTERM   15
#define SIGKILL    9
#define SIGINT     2
#define SIGUSR1   10

int  pipe_create();
int  pipe_write(int pid, const uint8_t* data, uint32_t n);
int  pipe_read(int pid, uint8_t* out, uint32_t n);
int  pipe_count();

int  mq_send(int id, const char* payload);
int  mq_recv(char* out_payload);
int  mq_count();

void signal_send(int target_pid, int sig);
int  signal_count();
const char* signal_name(int sig);

#endif
