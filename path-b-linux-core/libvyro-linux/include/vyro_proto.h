/*
 * Vyro compositor IPC — wire protocol (v0)
 *
 * Apps talk to vyro-compositor over a UNIX seqpacket socket at
 * /run/vyro/compositor.sock. Each message is a fixed-size header followed
 * by an optional payload, with optional SCM_RIGHTS fd-passing for shared
 * framebuffer handles (memfd).
 *
 * Wire format is little-endian; sizes are explicit. Bumping any field
 * size or layout means bumping VYRO_PROTO_VERSION.
 */

#ifndef VYRO_PROTO_H
#define VYRO_PROTO_H

#include <stdint.h>

#define VYRO_SOCKET_PATH    "/run/vyro/compositor.sock"
#define VYRO_PROTO_MAGIC    0x56594F50u   /* 'VYOP' */
#define VYRO_PROTO_VERSION  1u

/* --- Opcodes --- */
typedef enum {
    /* client -> server */
    VYRO_OP_HELLO          = 0x0001,   /* handshake, no payload */
    VYRO_OP_WINDOW_CREATE  = 0x0010,   /* payload: vyro_msg_window_create_t */
    VYRO_OP_WINDOW_DESTROY = 0x0011,   /* payload: vyro_msg_window_id_t */
    VYRO_OP_WINDOW_PRESENT = 0x0012,   /* payload: vyro_msg_present_t */
    VYRO_OP_WINDOW_TITLE   = 0x0013,   /* payload: vyro_msg_window_id_t + utf8 */

    /* server -> client */
    VYRO_OP_HELLO_OK       = 0x8001,   /* payload: vyro_msg_hello_ok_t */
    VYRO_OP_WINDOW_OK      = 0x8010,   /* payload: vyro_msg_window_ok_t */
    VYRO_OP_EVENT          = 0x8100,   /* payload: vyro_event_t */
    VYRO_OP_ERROR          = 0x8FFF,   /* payload: vyro_msg_error_t */
} vyro_op_t;

/* --- Header (16 bytes, naturally aligned) --- */
typedef struct {
    uint32_t magic;        /* VYRO_PROTO_MAGIC */
    uint16_t op;           /* vyro_op_t */
    uint16_t flags;        /* bit 0: has fd attached via SCM_RIGHTS */
    uint32_t length;       /* payload length in bytes (not incl header) */
    uint32_t serial;       /* request serial; reply echoes it */
} vyro_hdr_t;

/* --- Payloads --- */
typedef struct {
    uint32_t protocol_version;
    uint32_t client_pid;
} vyro_msg_hello_t;

typedef struct {
    uint32_t protocol_version;
    uint32_t session_id;
    uint32_t screen_w;
    uint32_t screen_h;
} vyro_msg_hello_ok_t;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t flags;        /* bit 0: borderless, bit 1: floating */
    char     title[128];
} vyro_msg_window_create_t;

typedef struct {
    uint32_t window_id;
} vyro_msg_window_id_t;

typedef struct {
    uint32_t window_id;
    uint32_t width;
    uint32_t height;
    uint32_t stride;       /* bytes per row of the attached memfd */
    /* The attached memfd is passed via SCM_RIGHTS in the ancillary data. */
} vyro_msg_present_t;

typedef struct {
    uint32_t window_id;
} vyro_msg_window_ok_t;

typedef struct {
    uint32_t code;
    char     text[120];
} vyro_msg_error_t;

#endif /* VYRO_PROTO_H */
