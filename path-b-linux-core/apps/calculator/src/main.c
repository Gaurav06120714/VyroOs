/*
 * Vyro Calculator (Path B port)
 *
 * Ported from kernel/apps/calculator on Path C. The arithmetic logic is
 * unchanged — only the rendering and input loop differ: instead of the
 * Vyro microkernel's int 0x80 syscalls, this version creates a window
 * over libvyro-linux's IPC client, presents into a memfd-backed surface,
 * and consumes input events the compositor dispatches.
 *
 * Build (host or Buildroot): see ../Makefile.
 */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "../../../libvyro-linux/include/vyro.h"
#include "../../../libvyro-linux/include/vyro_proto.h"

/* libvyro-linux IPC entry points (proto.c) */
int  vyro_proto_connect(void);
int  vyro_proto_send(uint16_t op, const void *p, uint32_t plen, int fd);
int  vyro_proto_recv(vyro_hdr_t *hdr, void *buf, uint32_t buflen, int *out_fd);

/* --- visual config (lock-stepped with theme tokens) --- */
#define WIN_W            320
#define WIN_H            420
#define BGRX_BG          0x00141620u
#define BGRX_DISPLAY     0x000B0B12u
#define BGRX_KEY         0x142A2D40u
#define BGRX_KEY_HOVER   0x30B388FFu
#define BGRX_KEY_OP      0x00B388FFu
#define BGRX_KEY_EQUALS  0x00C7A6FFu
#define BGRX_FG          0x00E8E9F1u

/* --- state --- */
static double  g_acc = 0;
static double  g_cur = 0;
static char    g_op  = 0;      /* +, -, *, /, 0=none */
static int     g_has_dot = 0;
static double  g_dot_div = 1;
static char    g_display[32] = "0";

static uint32_t *g_pixels;
static uint32_t  g_window_id;
static int       g_memfd = -1;
static size_t    g_surface_bytes;

/* --- minimal rendering helpers --- */

static void px_fill(int x, int y, int w, int h, uint32_t color) {
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = (x + w) > WIN_W ? WIN_W : (x + w);
    int y1 = (y + h) > WIN_H ? WIN_H : (y + h);
    for (int yy = y0; yy < y1; yy++)
        for (int xx = x0; xx < x1; xx++)
            g_pixels[yy * WIN_W + xx] = color;
}

/* 6x10 stub glyph block — real font glyph rasterizer lands later. */
static void px_text(int x, int y, const char *s, uint32_t color) {
    int cx = x;
    while (*s) {
        if (*s != ' ') px_fill(cx, y, 6, 10, color);
        cx += 8;
        s++;
    }
}

/* --- calculator logic (unchanged from Path C) --- */

static void format_display(double v) {
    if (v == (long long)v && v >= -1e15 && v <= 1e15)
        snprintf(g_display, sizeof(g_display), "%lld", (long long)v);
    else
        snprintf(g_display, sizeof(g_display), "%.6g", v);
}

static double apply_op(double a, char op, double b) {
    switch (op) {
    case '+': return a + b;
    case '-': return a - b;
    case '*': return a * b;
    case '/': return (b == 0.0) ? 0.0 : a / b;
    default:  return b;
    }
}

static void key_digit(int d) {
    if (g_has_dot) { g_dot_div *= 10.0; g_cur = g_cur + ((double)d) / g_dot_div; }
    else            { g_cur = g_cur * 10.0 + (double)d; }
    format_display(g_cur);
}

static void key_dot(void)   { g_has_dot = 1; }
static void key_clear(void) { g_acc = g_cur = 0; g_op = 0; g_has_dot = 0; g_dot_div = 1; format_display(0); }

static void key_op(char op) {
    if (g_op) g_acc = apply_op(g_acc, g_op, g_cur);
    else      g_acc = g_cur;
    g_cur = 0; g_has_dot = 0; g_dot_div = 1;
    g_op = op;
    format_display(g_acc);
}

static void key_equals(void) {
    if (g_op) g_acc = apply_op(g_acc, g_op, g_cur);
    else      g_acc = g_cur;
    g_cur = 0; g_op = 0; g_has_dot = 0; g_dot_div = 1;
    format_display(g_acc);
}

/* --- layout: 4 cols x 5 rows of 64x48 keys --- */

static const struct { int row, col; char label[4]; char action; int color; } KEYS[] = {
    {0,0,"C",  'C', 0}, {0,1,"+/-",'N', 0}, {0,2,"%", '%', 0}, {0,3,"/", '/', 1},
    {1,0,"7",  '7', 0}, {1,1,"8", '8', 0}, {1,2,"9", '9', 0}, {1,3,"*", '*', 1},
    {2,0,"4",  '4', 0}, {2,1,"5", '5', 0}, {2,2,"6", '6', 0}, {2,3,"-", '-', 1},
    {3,0,"1",  '1', 0}, {3,1,"2", '2', 0}, {3,2,"3", '3', 0}, {3,3,"+", '+', 1},
    {4,0,"0",  '0', 0}, {4,1,".", '.', 0}, {4,2,"=", '=', 2}, {4,3,"=", '=', 2},
};
#define KEY_W 64
#define KEY_H 48
#define KEY_PAD 8
#define KEYS_X0 16
#define KEYS_Y0 120

static void render(void) {
    px_fill(0, 0, WIN_W, WIN_H, BGRX_BG);
    /* display */
    px_fill(16, 16, WIN_W - 32, 80, BGRX_DISPLAY);
    int tx = 16 + (WIN_W - 32) - 12 - (int)strlen(g_display) * 8;
    if (tx < 24) tx = 24;
    px_text(tx, 60, g_display, BGRX_FG);

    /* keys */
    for (size_t i = 0; i < sizeof(KEYS) / sizeof(KEYS[0]); i++) {
        int x = KEYS_X0 + KEYS[i].col * (KEY_W + KEY_PAD);
        int y = KEYS_Y0 + KEYS[i].row * (KEY_H + KEY_PAD);
        uint32_t fill = BGRX_KEY;
        if (KEYS[i].color == 1) fill = BGRX_KEY_OP;
        if (KEYS[i].color == 2) fill = BGRX_KEY_EQUALS;
        if (KEYS[i].action == '=' && KEYS[i].col == 3) continue; /* '=' spans cols 2-3 */
        int w = KEY_W;
        if (KEYS[i].action == '=') w = KEY_W * 2 + KEY_PAD;
        px_fill(x, y, w, KEY_H, fill);
        int lx = x + w / 2 - ((int)strlen(KEYS[i].label) * 8) / 2;
        px_text(lx, y + 18, KEYS[i].label, BGRX_FG);
    }
}

static void on_click(int cx, int cy) {
    for (size_t i = 0; i < sizeof(KEYS) / sizeof(KEYS[0]); i++) {
        int x = KEYS_X0 + KEYS[i].col * (KEY_W + KEY_PAD);
        int y = KEYS_Y0 + KEYS[i].row * (KEY_H + KEY_PAD);
        int w = KEY_W;
        if (KEYS[i].action == '=' && KEYS[i].col == 3) continue;
        if (KEYS[i].action == '=') w = KEY_W * 2 + KEY_PAD;
        if (cx < x || cx >= x + w) continue;
        if (cy < y || cy >= y + KEY_H) continue;

        char a = KEYS[i].action;
        if (a >= '0' && a <= '9') key_digit(a - '0');
        else if (a == '.')        key_dot();
        else if (a == 'C')        key_clear();
        else if (a == '=')        key_equals();
        else if (a == '+' || a == '-' || a == '*' || a == '/')
                                  key_op(a);
        return;
    }
}

/* --- IPC plumbing: handshake, window create, present --- */

static int present(void) {
    /* Copy g_pixels (CPU heap) into the memfd-backed surface for the
     * compositor to mmap. In a real port we'd draw directly into the
     * memfd; copying keeps this minimal-port simple. */
    uint8_t *map = mmap(NULL, g_surface_bytes,
                       PROT_READ | PROT_WRITE, MAP_SHARED, g_memfd, 0);
    if (map == MAP_FAILED) { perror("mmap memfd"); return -1; }
    memcpy(map, g_pixels, g_surface_bytes);
    munmap(map, g_surface_bytes);

    vyro_msg_present_t p = {
        .window_id = g_window_id,
        .width     = WIN_W,
        .height    = WIN_H,
        .stride    = WIN_W * 4,
    };
    return vyro_proto_send(VYRO_OP_WINDOW_PRESENT, &p, sizeof(p), g_memfd);
}

int main(void) {
    if (vyro_proto_connect() < 0) {
        fprintf(stderr, "calculator: cannot connect to compositor at %s\n",
                VYRO_SOCKET_PATH);
        return 1;
    }

    /* HELLO */
    vyro_msg_hello_t h = { .protocol_version = VYRO_PROTO_VERSION, .client_pid = (uint32_t)getpid() };
    vyro_proto_send(VYRO_OP_HELLO, &h, sizeof(h), -1);

    vyro_hdr_t hdr; uint8_t buf[512]; int fd_in;
    if (vyro_proto_recv(&hdr, buf, sizeof(buf), &fd_in) < 0 || hdr.op != VYRO_OP_HELLO_OK) {
        fprintf(stderr, "calculator: HELLO failed\n"); return 1;
    }

    /* WINDOW_CREATE */
    vyro_msg_window_create_t wc = { .width = WIN_W, .height = WIN_H, .flags = 0 };
    strncpy(wc.title, "Calculator", sizeof(wc.title) - 1);
    vyro_proto_send(VYRO_OP_WINDOW_CREATE, &wc, sizeof(wc), -1);
    if (vyro_proto_recv(&hdr, buf, sizeof(buf), &fd_in) < 0 || hdr.op != VYRO_OP_WINDOW_OK) {
        fprintf(stderr, "calculator: WINDOW_CREATE failed\n"); return 1;
    }
    vyro_msg_window_ok_t *wo = (vyro_msg_window_ok_t *)buf;
    g_window_id = wo->window_id;

    /* Allocate a CPU pixel buffer + a memfd-backed surface for present */
    g_pixels = calloc((size_t)WIN_W * WIN_H, sizeof(uint32_t));
    if (!g_pixels) return 1;
    g_surface_bytes = (size_t)WIN_W * WIN_H * 4;
    g_memfd = memfd_create("vyro-calc-surface", MFD_CLOEXEC);
    if (g_memfd < 0 || ftruncate(g_memfd, g_surface_bytes) < 0) {
        perror("memfd_create"); return 1;
    }

    format_display(0);
    render(); present();

    /* event loop */
    while (1) {
        int r = vyro_proto_recv(&hdr, buf, sizeof(buf), &fd_in);
        if (r < 0) break;
        if (hdr.op == VYRO_OP_EVENT && r >= (int)sizeof(vyro_event_t)) {
            vyro_event_t *ev = (vyro_event_t *)buf;
            if (ev->kind == VYRO_EV_MOUSE_DOWN) {
                on_click(ev->x, ev->y);
                render(); present();
            } else if (ev->kind == VYRO_EV_KEY_DOWN) {
                /* a few quick keyboard bindings: Linux KEY_* codes */
                switch (ev->code) {
                case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: case 10: case 11:
                    /* KEY_1..KEY_0 → '1'..'0' */
                    if (ev->code == 11) key_digit(0); else key_digit(ev->code - 1);
                    break;
                case 78: key_op('+'); break;       /* KEY_KPPLUS */
                case 74: key_op('-'); break;       /* KEY_KPMINUS */
                case 55: key_op('*'); break;       /* KEY_KPASTERISK */
                case 98: key_op('/'); break;       /* KEY_KPSLASH */
                case 28: case 96: key_equals(); break; /* KEY_ENTER / KEY_KPENTER */
                case 14: case 1:  key_clear(); break;  /* BACKSPACE / ESC */
                default: break;
                }
                render(); present();
            } else if (ev->kind == VYRO_EV_CLOSE) {
                break;
            }
        }
        if (fd_in >= 0) close(fd_in);
    }

    close(g_memfd);
    free(g_pixels);
    return 0;
}
