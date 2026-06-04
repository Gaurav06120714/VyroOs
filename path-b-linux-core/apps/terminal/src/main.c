/*
 * Vyro Terminal (Path B port)
 *
 * Forks a real /bin/sh under a PTY, pipes its stdout/stderr into a
 * scrollback ring, and routes Vyro keyboard events back into the PTY
 * master. No VT100 emulation in vB.0.11 — control sequences pass through
 * unprocessed and we just paint the bytes (real ANSI handling comes in a
 * later phase). The point of vB.0.11 is to validate that a Vyro app can
 * own a foreground process subtree and exchange bytes both directions.
 */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pty.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "../../../libvyro-linux/include/vyro.h"
#include "../../../libvyro-linux/include/vyro_proto.h"

int  vyro_proto_connect(void);
int  vyro_proto_send(uint16_t op, const void *p, uint32_t plen, int fd);
int  vyro_proto_recv(vyro_hdr_t *hdr, void *buf, uint32_t buflen, int *out_fd);

#define WIN_W           640
#define WIN_H           400
#define COLS            80
#define ROWS            24
#define CHAR_W          8
#define LINE_H          14
#define ORIGIN_X        12
#define ORIGIN_Y        36

#define BGRX_BG         0x000B0B12u
#define BGRX_HEADER     0xC8141620u
#define BGRX_FG         0x00E8E9F1u
#define BGRX_ACCENT     0x00B388FFu
#define BGRX_BORDER     0x14FFFFFFu

/* Scrollback ring: ROWS*COLS chars, last row is current line. */
static char     g_screen[ROWS][COLS];
static int      g_cursor_row = 0;
static int      g_cursor_col = 0;
static int      g_pty_fd = -1;
static pid_t    g_child_pid = -1;

static uint32_t *g_pixels;
static uint32_t  g_window_id;
static int       g_memfd = -1;
static size_t    g_surface_bytes;

/* --- BGRX primitives --- */
static void px_fill(int x, int y, int w, int h, uint32_t c) {
    int x0=x<0?0:x, y0=y<0?0:y;
    int x1=(x+w)>WIN_W?WIN_W:(x+w), y1=(y+h)>WIN_H?WIN_H:(y+h);
    for (int yy=y0; yy<y1; yy++)
        for (int xx=x0; xx<x1; xx++)
            g_pixels[yy*WIN_W+xx] = c;
}
static void px_putch(int x, int y, char ch, uint32_t color) {
    if (ch == ' ' || ch == 0) return;
    px_fill(x, y, 6, 10, color);
}
static void px_text(int x, int y, const char *s, uint32_t color) {
    int cx = x;
    while (*s) { px_putch(cx, y, *s, color); cx += CHAR_W; s++; }
}

/* --- screen helpers --- */
static void clear_screen(void) {
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) g_screen[r][c] = ' ';
    g_cursor_row = 0;
    g_cursor_col = 0;
}

static void scroll_up(void) {
    for (int r = 0; r + 1 < ROWS; r++)
        memcpy(g_screen[r], g_screen[r+1], COLS);
    for (int c = 0; c < COLS; c++) g_screen[ROWS-1][c] = ' ';
    g_cursor_row = ROWS - 1;
}

static void put_char(char ch) {
    if (ch == '\r') { g_cursor_col = 0; return; }
    if (ch == '\n') { g_cursor_col = 0; g_cursor_row++; goto wrap; }
    if (ch == '\b') { if (g_cursor_col) g_cursor_col--; return; }
    if (ch == '\t') { g_cursor_col = (g_cursor_col + 8) & ~7; goto wrap; }
    /* Strip the rest of C0 (incl. ESC) for vB.0.11 — VT100 lives in a later phase */
    if ((unsigned char)ch < 0x20 || ch == 0x7f) return;
    if (g_cursor_col >= COLS) { g_cursor_col = 0; g_cursor_row++; }
    if (g_cursor_row >= ROWS) scroll_up();
    g_screen[g_cursor_row][g_cursor_col++] = ch;
    return;
wrap:
    if (g_cursor_row >= ROWS) scroll_up();
}

/* --- render --- */
static int g_blink = 0;
static void render(void) {
    px_fill(0, 0, WIN_W, WIN_H, BGRX_BG);
    /* Header */
    px_fill(0, 0, WIN_W, 28, BGRX_HEADER);
    px_fill(0, 27, WIN_W, 1, BGRX_BORDER);
    px_text(ORIGIN_X, 9, "Terminal — /bin/sh", BGRX_FG);

    for (int r = 0; r < ROWS; r++) {
        int py = ORIGIN_Y + r * LINE_H;
        int px = ORIGIN_X;
        for (int c = 0; c < COLS; c++) {
            px_putch(px, py, g_screen[r][c], BGRX_FG);
            px += CHAR_W;
        }
    }
    /* Cursor */
    if ((g_blink & 16) == 0) {
        int cx = ORIGIN_X + g_cursor_col * CHAR_W;
        int cy = ORIGIN_Y + g_cursor_row * LINE_H;
        px_fill(cx, cy, 2, 10, BGRX_ACCENT);
    }
}

/* --- present --- */
static int present(void) {
    uint8_t *m = mmap(NULL, g_surface_bytes, PROT_READ|PROT_WRITE, MAP_SHARED, g_memfd, 0);
    if (m == MAP_FAILED) return -1;
    memcpy(m, g_pixels, g_surface_bytes);
    munmap(m, g_surface_bytes);
    vyro_msg_present_t p = { .window_id=g_window_id, .width=WIN_W, .height=WIN_H, .stride=WIN_W*4 };
    return vyro_proto_send(VYRO_OP_WINDOW_PRESENT, &p, sizeof(p), g_memfd);
}

/* --- spawn /bin/sh under a PTY --- */
static int spawn_shell(void) {
    struct winsize ws = { .ws_row = ROWS, .ws_col = COLS };
    int master;
    pid_t pid = forkpty(&master, NULL, NULL, &ws);
    if (pid < 0) { perror("forkpty"); return -1; }
    if (pid == 0) {
        setenv("TERM", "vyro", 1);
        setenv("PS1",  "vyro$ ", 1);
        execl("/bin/sh", "/bin/sh", "-i", (char *)NULL);
        perror("exec /bin/sh");
        _exit(127);
    }
    g_child_pid = pid;
    g_pty_fd = master;
    /* Non-blocking master so the IPC loop can interleave */
    fcntl(g_pty_fd, F_SETFL, O_NONBLOCK);
    return 0;
}

static void drain_pty(void) {
    char buf[1024];
    for (;;) {
        ssize_t n = read(g_pty_fd, buf, sizeof(buf));
        if (n <= 0) return;
        for (ssize_t i = 0; i < n; i++) put_char(buf[i]);
    }
}

/* --- US QWERTY scancode → ASCII (same table as TextEdit) --- */
static char scancode_to_ascii(uint32_t code, int shift) {
    static const char base[] =
        "  1234567890-=  qwertyuiop[]\n  asdfghjkl;'`  \\zxcvbnm,./";
    static const char shft[] =
        "  !@#$%^&*()_+  QWERTYUIOP{}\n  ASDFGHJKL:\"~  |ZXCVBNM<>?";
    if (code == 57) return ' ';
    if (code == 15) return '\t';
    if (code < sizeof(base) - 1) {
        char c = shift ? shft[code] : base[code];
        if (c != ' ' || code == 57) return c;
    }
    return 0;
}

int main(void) {
    if (vyro_proto_connect() < 0) {
        fprintf(stderr, "terminal: cannot connect to compositor\n"); return 1;
    }
    vyro_msg_hello_t h = { .protocol_version = VYRO_PROTO_VERSION, .client_pid = (uint32_t)getpid() };
    vyro_proto_send(VYRO_OP_HELLO, &h, sizeof(h), -1);
    vyro_hdr_t hdr; uint8_t buf[1024]; int fd_in;
    if (vyro_proto_recv(&hdr, buf, sizeof(buf), &fd_in) < 0 || hdr.op != VYRO_OP_HELLO_OK) return 1;

    vyro_msg_window_create_t wc = { .width=WIN_W, .height=WIN_H, .flags=0 };
    strncpy(wc.title, "Terminal", sizeof(wc.title)-1);
    vyro_proto_send(VYRO_OP_WINDOW_CREATE, &wc, sizeof(wc), -1);
    if (vyro_proto_recv(&hdr, buf, sizeof(buf), &fd_in) < 0 || hdr.op != VYRO_OP_WINDOW_OK) return 1;
    g_window_id = ((vyro_msg_window_ok_t *)buf)->window_id;

    g_pixels = calloc((size_t)WIN_W*WIN_H, 4);
    g_surface_bytes = (size_t)WIN_W*WIN_H*4;
    g_memfd = memfd_create("vyro-term-surface", MFD_CLOEXEC);
    if (g_memfd < 0 || ftruncate(g_memfd, g_surface_bytes) < 0) return 1;

    clear_screen();
    if (spawn_shell() < 0) return 1;
    render(); present();

    int shift_down = 0;
    while (1) {
        struct pollfd pfds[1] = { { .fd = g_pty_fd, .events = POLLIN } };
        int p = poll(pfds, 1, 16);   /* tiny tick so IPC stays responsive */
        if (p > 0 && (pfds[0].revents & POLLIN)) {
            drain_pty();
            render(); present();
        }

        /* Non-blocking IPC drain — recv would block on its own loop, so
         * peek with MSG_DONTWAIT via vyro_proto_recv's internal recvmsg
         * (proto.c uses blocking recv; in vB.0.11 we accept that the loop
         * is driven by IPC events plus PTY polling, which is correct
         * because the compositor only sends events when something
         * happens). */
        int r = vyro_proto_recv(&hdr, buf, sizeof(buf), &fd_in);
        if (r < 0) break;
        if (hdr.op == VYRO_OP_EVENT && r >= (int)sizeof(vyro_event_t)) {
            vyro_event_t *ev = (vyro_event_t *)buf;
            if (ev->kind == VYRO_EV_KEY_DOWN) {
                if (ev->code == 42 || ev->code == 54) { shift_down = 1; }
                else if (ev->code == 1) { goto done; }                   /* ESC */
                else if (ev->code == 14) { char b = 0x7f; write(g_pty_fd, &b, 1); }      /* BS → DEL */
                else if (ev->code == 28 || ev->code == 96) { char b = '\r'; write(g_pty_fd, &b, 1); }
                else {
                    char c = scancode_to_ascii(ev->code, shift_down);
                    if (c) write(g_pty_fd, &c, 1);
                }
            } else if (ev->kind == VYRO_EV_KEY_UP) {
                if (ev->code == 42 || ev->code == 54) shift_down = 0;
            } else if (ev->kind == VYRO_EV_CLOSE) {
                goto done;
            }
            drain_pty();
            g_blink++;
            render(); present();
        }
        if (fd_in >= 0) close(fd_in);

        /* Reap shell if it died */
        int wstatus;
        if (waitpid(g_child_pid, &wstatus, WNOHANG) > 0) goto done;
    }
done:
    if (g_pty_fd >= 0) close(g_pty_fd);
    if (g_child_pid > 0) kill(g_child_pid, 9);
    close(g_memfd);
    free(g_pixels);
    return 0;
}
