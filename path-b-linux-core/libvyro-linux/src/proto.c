/*
 * libvyro-linux — wire-protocol client (vB.0.2)
 *
 * Sends/receives vyro_hdr_t + payload over the seqpacket socket; supports
 * SCM_RIGHTS fd passing for memfd-backed framebuffers.
 */

#define _GNU_SOURCE
#include "../include/vyro.h"
#include "../include/vyro_proto.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

static int g_fd = -1;
static uint32_t g_serial = 0;

int vyro_proto_connect(void) {
    if (g_fd >= 0) return 0;

    int s = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    if (s < 0) return -errno;

    struct sockaddr_un sa = {0};
    sa.sun_family = AF_UNIX;
    strncpy(sa.sun_path, VYRO_SOCKET_PATH, sizeof(sa.sun_path) - 1);

    if (connect(s, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        int e = -errno;
        close(s);
        return e;
    }
    g_fd = s;
    return 0;
}

void vyro_proto_disconnect(void) {
    if (g_fd >= 0) { close(g_fd); g_fd = -1; }
}

/* Send hdr + payload; optionally attach `fd` via SCM_RIGHTS. */
int vyro_proto_send(uint16_t op, const void *payload, uint32_t plen, int fd) {
    if (g_fd < 0) return -ENOTCONN;
    vyro_hdr_t hdr = {
        .magic   = VYRO_PROTO_MAGIC,
        .op      = op,
        .flags   = (fd >= 0) ? 1u : 0u,
        .length  = plen,
        .serial  = ++g_serial,
    };

    struct iovec iov[2];
    int niov = 1;
    iov[0].iov_base = &hdr;
    iov[0].iov_len  = sizeof(hdr);
    if (plen && payload) {
        iov[1].iov_base = (void *)payload;
        iov[1].iov_len  = plen;
        niov = 2;
    }

    struct msghdr m = {0};
    m.msg_iov    = iov;
    m.msg_iovlen = niov;

    union { struct cmsghdr h; char buf[CMSG_SPACE(sizeof(int))]; } cmsg;
    if (fd >= 0) {
        memset(&cmsg, 0, sizeof(cmsg));
        m.msg_control    = cmsg.buf;
        m.msg_controllen = sizeof(cmsg.buf);
        struct cmsghdr *c = CMSG_FIRSTHDR(&m);
        c->cmsg_len   = CMSG_LEN(sizeof(int));
        c->cmsg_level = SOL_SOCKET;
        c->cmsg_type  = SCM_RIGHTS;
        memcpy(CMSG_DATA(c), &fd, sizeof(int));
    }

    ssize_t r = sendmsg(g_fd, &m, MSG_NOSIGNAL);
    return r < 0 ? -errno : 0;
}

/* Receive one message into hdr + caller-provided buffer. Returns bytes
 * read into buf (0..buflen), or negative errno. If an fd was attached,
 * writes it to *out_fd (or closes it if out_fd is NULL). */
int vyro_proto_recv(vyro_hdr_t *out_hdr, void *buf, uint32_t buflen, int *out_fd) {
    if (g_fd < 0) return -ENOTCONN;
    if (!out_hdr) return -EINVAL;

    struct iovec iov[2];
    iov[0].iov_base = out_hdr;
    iov[0].iov_len  = sizeof(*out_hdr);
    iov[1].iov_base = buf;
    iov[1].iov_len  = buflen;

    struct msghdr m = {0};
    m.msg_iov    = iov;
    m.msg_iovlen = 2;

    union { struct cmsghdr h; char buf[CMSG_SPACE(sizeof(int))]; } cmsg;
    memset(&cmsg, 0, sizeof(cmsg));
    m.msg_control    = cmsg.buf;
    m.msg_controllen = sizeof(cmsg.buf);

    ssize_t r = recvmsg(g_fd, &m, 0);
    if (r < 0) return -errno;
    if ((size_t)r < sizeof(*out_hdr)) return -EBADMSG;
    if (out_hdr->magic != VYRO_PROTO_MAGIC) return -EBADMSG;

    int fd = -1;
    for (struct cmsghdr *c = CMSG_FIRSTHDR(&m); c; c = CMSG_NXTHDR(&m, c)) {
        if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_RIGHTS) {
            memcpy(&fd, CMSG_DATA(c), sizeof(int));
            break;
        }
    }
    if (out_fd) *out_fd = fd;
    else if (fd >= 0) close(fd);

    return (int)(r - sizeof(*out_hdr));
}
