/*
 * Vyro Compositor — IPC server (vB.0.3)
 *
 * Listens on /run/vyro/compositor.sock (SOCK_SEQPACKET). Accepts client
 * connections, runs the HELLO handshake, services WINDOW_CREATE /
 * WINDOW_PRESENT / WINDOW_DESTROY, and blits each presented memfd onto
 * the DRM dumb buffer set up by main.c.
 *
 * Single-threaded, poll(2)-driven. Up to 32 client connections and
 * 64 windows total in vB.0.3 — multi-client multi-window is the point.
 */

#define _GNU_SOURCE
#include "../../libvyro-linux/include/vyro_proto.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#define MAX_CLIENTS 32
#define MAX_WINDOWS 64

/* The screen framebuffer is owned by main.c (DRM dumb buffer). The
 * server doesn't need to know its layout to blit — main.c exposes a
 * single function the server calls. */
extern void vyro_screen_info(uint32_t *w, uint32_t *h);
extern void vyro_screen_blit(int dst_x, int dst_y,
                             const uint8_t *src, uint32_t src_w,
                             uint32_t src_h, uint32_t src_stride);
extern void vyro_screen_flush(void);

typedef struct {
    int      fd;             /* socket fd, -1 if free */
    pid_t    pid;
    uint32_t session_id;
    int      hello_done;
} client_t;

typedef struct {
    int      in_use;
    int      owner;          /* index into clients[] */
    uint32_t id;
    uint32_t width;
    uint32_t height;
    char     title[128];
    /* layout: simple cascading offset for vB.0.3 */
    int      x, y;
} window_t;

static client_t g_clients[MAX_CLIENTS];
static window_t g_windows[MAX_WINDOWS];

extern int  vyro_chrome_titlebar_h(void);

/* --- vB.0.5: helpers used by input.c --- */

int vyro_server_window_hit(int cx, int cy, int *in_chrome, int *dx, int *dy) {
    int chrome_h = vyro_chrome_titlebar_h();
    for (int i = MAX_WINDOWS - 1; i >= 0; i--) {
        if (!g_windows[i].in_use) continue;
        int wx = g_windows[i].x;
        int wy = g_windows[i].y;
        int ww = (int)g_windows[i].width;
        int wh = (int)g_windows[i].height;
        /* Chrome box (title bar + content) */
        int box_x = wx;
        int box_y = wy - chrome_h;
        int box_w = ww;
        int box_h = wh + chrome_h;
        if (cx < box_x || cx >= box_x + box_w) continue;
        if (cy < box_y || cy >= box_y + box_h) continue;
        if (in_chrome) *in_chrome = (cy < wy);
        if (dx)        *dx = cx - wx;
        if (dy)        *dy = cy - wy;
        return i;
    }
    if (in_chrome) *in_chrome = 0;
    if (dx)        *dx = 0;
    if (dy)        *dy = 0;
    return -1;
}

void vyro_server_window_move(int idx, int dx, int dy) {
    if (idx < 0 || idx >= MAX_WINDOWS) return;
    if (!g_windows[idx].in_use) return;
    g_windows[idx].x += dx;
    g_windows[idx].y += dy;
}

void vyro_server_send_event_to(int idx, const vyro_event_t *ev) {
    if (idx < 0 || idx >= MAX_WINDOWS || !ev) return;
    if (!g_windows[idx].in_use) return;
    int owner = g_windows[idx].owner;
    if (owner < 0 || owner >= MAX_CLIENTS) return;
    if (g_clients[owner].fd < 0) return;
    send_msg(g_clients[owner].fd, VYRO_OP_EVENT, ev, sizeof(*ev), 0);
}
static uint32_t g_next_session = 1;
static uint32_t g_next_window  = 1;
static int      g_listen_fd    = -1;

/* ---- socket helpers (mirror libvyro-linux/src/proto.c) ---- */

static int send_msg(int fd, uint16_t op, const void *p, uint32_t plen, uint32_t serial) {
    vyro_hdr_t hdr = {
        .magic = VYRO_PROTO_MAGIC, .op = op, .flags = 0,
        .length = plen, .serial = serial,
    };
    struct iovec iov[2] = {
        {&hdr, sizeof(hdr)},
        {(void *)p, plen},
    };
    struct msghdr m = {0};
    m.msg_iov = iov; m.msg_iovlen = plen ? 2 : 1;
    ssize_t r = sendmsg(fd, &m, MSG_NOSIGNAL);
    return r < 0 ? -errno : 0;
}

static int recv_msg(int fd, vyro_hdr_t *hdr, void *buf, uint32_t buflen, int *out_fd) {
    struct iovec iov[2] = { {hdr, sizeof(*hdr)}, {buf, buflen} };
    struct msghdr m = {0};
    m.msg_iov = iov; m.msg_iovlen = 2;

    union { struct cmsghdr h; char buf[CMSG_SPACE(sizeof(int))]; } cmsg;
    memset(&cmsg, 0, sizeof(cmsg));
    m.msg_control    = cmsg.buf;
    m.msg_controllen = sizeof(cmsg.buf);

    ssize_t r = recvmsg(fd, &m, 0);
    if (r == 0) return -ECONNRESET;
    if (r < 0)  return -errno;
    if ((size_t)r < sizeof(*hdr) || hdr->magic != VYRO_PROTO_MAGIC) return -EBADMSG;

    int got_fd = -1;
    for (struct cmsghdr *c = CMSG_FIRSTHDR(&m); c; c = CMSG_NXTHDR(&m, c)) {
        if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_RIGHTS) {
            memcpy(&got_fd, CMSG_DATA(c), sizeof(int));
            break;
        }
    }
    if (out_fd) *out_fd = got_fd;
    else if (got_fd >= 0) close(got_fd);
    return (int)(r - sizeof(*hdr));
}

/* ---- protocol handlers ---- */

static void send_error(int fd, uint32_t serial, uint32_t code, const char *txt) {
    vyro_msg_error_t e = { .code = code };
    strncpy(e.text, txt ? txt : "", sizeof(e.text) - 1);
    send_msg(fd, VYRO_OP_ERROR, &e, sizeof(e), serial);
}

static void handle_hello(client_t *c, const vyro_hdr_t *hdr, const void *p, uint32_t plen) {
    if (plen < sizeof(vyro_msg_hello_t)) {
        send_error(c->fd, hdr->serial, EINVAL, "HELLO payload too short"); return;
    }
    const vyro_msg_hello_t *h = p;
    if (h->protocol_version != VYRO_PROTO_VERSION) {
        send_error(c->fd, hdr->serial, EPROTONOSUPPORT, "protocol mismatch"); return;
    }
    c->pid = h->client_pid;
    c->session_id = g_next_session++;
    c->hello_done = 1;

    uint32_t sw = 0, sh = 0;
    vyro_screen_info(&sw, &sh);

    vyro_msg_hello_ok_t reply = {
        .protocol_version = VYRO_PROTO_VERSION,
        .session_id       = c->session_id,
        .screen_w         = sw,
        .screen_h         = sh,
    };
    send_msg(c->fd, VYRO_OP_HELLO_OK, &reply, sizeof(reply), hdr->serial);
    fprintf(stderr, "compositor: client pid=%d session=%u (%ux%u)\n",
            (int)c->pid, c->session_id, sw, sh);
}

static int alloc_window(int owner) {
    for (int i = 0; i < MAX_WINDOWS; i++) if (!g_windows[i].in_use) {
        g_windows[i].in_use = 1;
        g_windows[i].owner  = owner;
        g_windows[i].id     = g_next_window++;
        return i;
    }
    return -1;
}

static int find_window(uint32_t id) {
    for (int i = 0; i < MAX_WINDOWS; i++)
        if (g_windows[i].in_use && g_windows[i].id == id) return i;
    return -1;
}

static void handle_window_create(client_t *c, int owner_idx,
                                 const vyro_hdr_t *hdr, const void *p, uint32_t plen) {
    if (!c->hello_done) { send_error(c->fd, hdr->serial, EPERM, "HELLO required"); return; }
    if (plen < sizeof(vyro_msg_window_create_t)) {
        send_error(c->fd, hdr->serial, EINVAL, "WINDOW_CREATE payload short"); return;
    }
    const vyro_msg_window_create_t *m = p;
    int idx = alloc_window(owner_idx);
    if (idx < 0) { send_error(c->fd, hdr->serial, ENOMEM, "too many windows"); return; }

    g_windows[idx].width  = m->width;
    g_windows[idx].height = m->height;
    memcpy(g_windows[idx].title, m->title, sizeof(g_windows[idx].title));
    g_windows[idx].title[sizeof(g_windows[idx].title) - 1] = '\0';

    /* simple cascading placement */
    g_windows[idx].x = 40 + (idx * 32);
    g_windows[idx].y = 60 + (idx * 32);

    vyro_msg_window_ok_t reply = { .window_id = g_windows[idx].id };
    send_msg(c->fd, VYRO_OP_WINDOW_OK, &reply, sizeof(reply), hdr->serial);
}

static void handle_window_destroy(client_t *c, const vyro_hdr_t *hdr,
                                  const void *p, uint32_t plen) {
    if (plen < sizeof(vyro_msg_window_id_t)) return;
    const vyro_msg_window_id_t *m = p;
    int idx = find_window(m->window_id);
    if (idx < 0) { send_error(c->fd, hdr->serial, ENOENT, "no such window"); return; }
    g_windows[idx].in_use = 0;
}

static void handle_window_present(client_t *c, const vyro_hdr_t *hdr,
                                  const void *p, uint32_t plen, int memfd) {
    if (plen < sizeof(vyro_msg_present_t) || memfd < 0) {
        if (memfd >= 0) close(memfd);
        send_error(c->fd, hdr->serial, EINVAL, "PRESENT needs payload + fd"); return;
    }
    const vyro_msg_present_t *m = p;
    int idx = find_window(m->window_id);
    if (idx < 0) { close(memfd);
        send_error(c->fd, hdr->serial, ENOENT, "no such window"); return; }

    size_t map_len = (size_t)m->stride * m->height;
    void *src = mmap(NULL, map_len, PROT_READ, MAP_SHARED, memfd, 0);
    if (src == MAP_FAILED) {
        close(memfd);
        send_error(c->fd, hdr->serial, errno, "mmap memfd failed"); return;
    }

    /* vB.0.4: decorate first (shadow underneath, title bar above),
     * then blit client content into the body region. */
    extern void vyro_chrome_decorate(int x, int y, int w, int h, const char *title);
    extern int  vyro_chrome_titlebar_h(void);
    vyro_chrome_decorate(g_windows[idx].x, g_windows[idx].y,
                         m->width, m->height, g_windows[idx].title);
    vyro_screen_blit(g_windows[idx].x, g_windows[idx].y,
                     src, m->width, m->height, m->stride);
    vyro_screen_flush();
    (void)vyro_chrome_titlebar_h;

    munmap(src, map_len);
    close(memfd);
}

/* ---- accept / dispatch loop ---- */

static int find_free_client(void) {
    for (int i = 0; i < MAX_CLIENTS; i++) if (g_clients[i].fd < 0) return i;
    return -1;
}

static void drop_client(int idx) {
    if (g_clients[idx].fd >= 0) close(g_clients[idx].fd);
    g_clients[idx].fd = -1;
    g_clients[idx].hello_done = 0;
    /* destroy this client's windows */
    for (int i = 0; i < MAX_WINDOWS; i++)
        if (g_windows[i].in_use && g_windows[i].owner == idx) g_windows[i].in_use = 0;
}

static void on_client_readable(int idx) {
    vyro_hdr_t hdr;
    uint8_t payload[4096];
    int fd = -1;
    int r = recv_msg(g_clients[idx].fd, &hdr, payload, sizeof(payload), &fd);
    if (r < 0) { drop_client(idx); return; }
    if (hdr.length > sizeof(payload)) {
        if (fd >= 0) close(fd);
        send_error(g_clients[idx].fd, hdr.serial, E2BIG, "payload too large");
        return;
    }
    switch (hdr.op) {
        case VYRO_OP_HELLO:          handle_hello(&g_clients[idx], &hdr, payload, hdr.length); break;
        case VYRO_OP_WINDOW_CREATE:  handle_window_create(&g_clients[idx], idx, &hdr, payload, hdr.length); break;
        case VYRO_OP_WINDOW_DESTROY: handle_window_destroy(&g_clients[idx], &hdr, payload, hdr.length); break;
        case VYRO_OP_WINDOW_PRESENT: handle_window_present(&g_clients[idx], &hdr, payload, hdr.length, fd); fd = -1; break;
        default:
            send_error(g_clients[idx].fd, hdr.serial, ENOSYS, "unknown op");
            break;
    }
    if (fd >= 0) close(fd);
}

int vyro_server_init(void) {
    for (int i = 0; i < MAX_CLIENTS; i++) g_clients[i].fd = -1;
    for (int i = 0; i < MAX_WINDOWS; i++) g_windows[i].in_use = 0;

    mkdir("/run/vyro", 0755);
    unlink(VYRO_SOCKET_PATH);

    int s = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (s < 0) { perror("compositor: socket"); return -1; }

    struct sockaddr_un sa = {0};
    sa.sun_family = AF_UNIX;
    strncpy(sa.sun_path, VYRO_SOCKET_PATH, sizeof(sa.sun_path) - 1);
    if (bind(s, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        perror("compositor: bind"); close(s); return -1;
    }
    chmod(VYRO_SOCKET_PATH, 0660);
    if (listen(s, 16) < 0) { perror("compositor: listen"); close(s); return -1; }

    g_listen_fd = s;
    fprintf(stderr, "compositor: listening on %s\n", VYRO_SOCKET_PATH);
    return 0;
}

/* Single iteration of the dispatch loop. Returns 0 on timeout, 1 on activity,
 * <0 on fatal error. Designed to be called from compositor-drm/main.c between
 * page flips so the server doesn't need its own thread. */
int vyro_server_tick(int timeout_ms) {
    struct pollfd pfds[1 + MAX_CLIENTS];
    int n = 0;

    pfds[n].fd = g_listen_fd; pfds[n].events = POLLIN; n++;

    int client_slot[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].fd >= 0) {
            pfds[n].fd = g_clients[i].fd; pfds[n].events = POLLIN;
            client_slot[n] = i;
            n++;
        }
    }

    int r = poll(pfds, n, timeout_ms);
    if (r <= 0) return r;

    if (pfds[0].revents & POLLIN) {
        int c = accept4(g_listen_fd, NULL, NULL, SOCK_CLOEXEC | SOCK_NONBLOCK);
        if (c >= 0) {
            int slot = find_free_client();
            if (slot < 0) { close(c); }
            else          { g_clients[slot].fd = c; }
        }
    }
    for (int i = 1; i < n; i++) {
        if (pfds[i].revents & (POLLIN | POLLHUP | POLLERR)) {
            on_client_readable(client_slot[i]);
        }
    }
    return 1;
}
