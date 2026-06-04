/*
 * Vyro TextEdit (Path B port)
 *
 * Ported from kernel/apps/textedit on Path C. A minimal single-document
 * editor: monospace text buffer, blinking cursor, arrow keys, backspace,
 * Enter, character input via the Linux input event codes mapped through
 * a US-QWERTY scancode → ASCII table. Ctrl+S writes the buffer back to
 * disk (argv[1] is the path; defaults to /tmp/untitled.txt).
 *
 * Third concurrent client to exercise the IPC server's keyboard event
 * dispatch — Calculator (vB.0.6) and Files (vB.0.8) only consumed a
 * handful of keys; TextEdit consumes the full character set.
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
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "../../../libvyro-linux/include/vyro.h"
#include "../../../libvyro-linux/include/vyro_proto.h"

int  vyro_proto_connect(void);
int  vyro_proto_send(uint16_t op, const void *p, uint32_t plen, int fd);
int  vyro_proto_recv(vyro_hdr_t *hdr, void *buf, uint32_t buflen, int *out_fd);

#define WIN_W           640
#define WIN_H           480
#define GUTTER          16
#define LINE_H          14
#define CHAR_W          8
#define VISIBLE_LINES   ((WIN_H - 40) / LINE_H)

#define BGRX_BG         0x00141620u
#define BGRX_HEADER     0xC8141620u
#define BGRX_FG         0x00E8E9F1u
#define BGRX_FG_MUTED   0x00A0A4B6u
#define BGRX_ACCENT     0x00B388FFu
#define BGRX_CURSOR     0x00C7A6FFu
#define BGRX_BORDER     0x14FFFFFFu

#define BUF_CAP   (64 * 1024)
static char     g_buf[BUF_CAP];
static int      g_len = 0;
static int      g_cur = 0;     /* byte offset of caret */
static int      g_scroll = 0;  /* top visible line */
static int      g_dirty = 0;
static char     g_path[1024];
static uint32_t g_blink_tick = 0;

static uint32_t *g_pixels;
static uint32_t  g_window_id;
static int       g_memfd = -1;
static size_t    g_surface_bytes;

/* --- BGRX primitives (same shape as Calculator/Files) --- */
static void px_fill(int x, int y, int w, int h, uint32_t c) {
    int x0 = x<0?0:x, y0 = y<0?0:y;
    int x1 = (x+w)>WIN_W?WIN_W:(x+w), y1 = (y+h)>WIN_H?WIN_H:(y+h);
    for (int yy=y0; yy<y1; yy++)
        for (int xx=x0; xx<x1; xx++)
            g_pixels[yy*WIN_W+xx] = c;
}

/* Stub mono-glyph: 6x10 filled rect per non-space, advance CHAR_W. */
static void px_putch(int x, int y, char c, uint32_t color) {
    if (c == ' ') return;
    px_fill(x, y, 6, 10, color);
}

static void px_text(int x, int y, const char *s, uint32_t color) {
    int cx = x;
    while (*s) { px_putch(cx, y, *s, color); cx += CHAR_W; s++; }
}

/* --- buffer + cursor helpers --- */

static void insert_byte(char c) {
    if (g_len + 1 >= BUF_CAP) return;
    memmove(&g_buf[g_cur+1], &g_buf[g_cur], g_len - g_cur);
    g_buf[g_cur++] = c;
    g_len++;
    g_dirty = 1;
}

static void delete_byte_before_cur(void) {
    if (g_cur == 0) return;
    memmove(&g_buf[g_cur-1], &g_buf[g_cur], g_len - g_cur);
    g_cur--;
    g_len--;
    g_dirty = 1;
}

static void cursor_left (void) { if (g_cur > 0)     g_cur--; }
static void cursor_right(void) { if (g_cur < g_len) g_cur++; }

static int find_line_start(int off) {
    while (off > 0 && g_buf[off-1] != '\n') off--;
    return off;
}
static int find_line_end(int off) {
    while (off < g_len && g_buf[off] != '\n') off++;
    return off;
}

static void cursor_up(void) {
    int ls = find_line_start(g_cur);
    if (ls == 0) return;
    int col = g_cur - ls;
    int prev_end = ls - 1;
    int prev_start = find_line_start(prev_end);
    int prev_len = prev_end - prev_start;
    g_cur = prev_start + (col < prev_len ? col : prev_len);
}
static void cursor_down(void) {
    int ls = find_line_start(g_cur);
    int le = find_line_end(g_cur);
    if (le >= g_len) return;
    int col = g_cur - ls;
    int next_start = le + 1;
    int next_end = find_line_end(next_start);
    int next_len = next_end - next_start;
    g_cur = next_start + (col < next_len ? col : next_len);
}

/* --- file I/O --- */
static void load_file(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) { g_len = 0; g_cur = 0; return; }
    ssize_t n = read(fd, g_buf, BUF_CAP);
    close(fd);
    if (n < 0) n = 0;
    g_len = (int)n;
    g_cur = 0;
    g_dirty = 0;
}

static int save_file(const char *path) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return 0;
    ssize_t n = write(fd, g_buf, g_len);
    close(fd);
    if (n != g_len) return 0;
    g_dirty = 0;
    return 1;
}

/* --- render --- */
static void render(void) {
    px_fill(0, 0, WIN_W, WIN_H, BGRX_BG);

    /* Header bar with file name + dirty marker */
    px_fill(0, 0, WIN_W, 28, BGRX_HEADER);
    px_fill(0, 27, WIN_W, 1, BGRX_BORDER);

    char hdr[1100];
    snprintf(hdr, sizeof(hdr), "%s%s — Ctrl+S to save", g_path, g_dirty ? " *" : "");
    px_text(GUTTER, 9, hdr, BGRX_FG);

    /* Walk the buffer line-by-line into the visible window. */
    int line_idx = 0;
    int off = 0;
    int caret_x = -1, caret_y = -1;

    while (off <= g_len && line_idx < g_scroll + VISIBLE_LINES) {
        int line_end = find_line_end(off);
        if (line_idx >= g_scroll) {
            int py = 40 + (line_idx - g_scroll) * LINE_H;
            int px = GUTTER;
            for (int i = off; i < line_end && px + CHAR_W < WIN_W - GUTTER; i++) {
                px_putch(px, py, g_buf[i], BGRX_FG);
                px += CHAR_W;
            }
            if (g_cur >= off && g_cur <= line_end) {
                caret_x = GUTTER + (g_cur - off) * CHAR_W;
                caret_y = py;
            }
        }
        if (line_end >= g_len) break;
        off = line_end + 1;
        line_idx++;
    }

    /* Blinking caret */
    if (caret_x >= 0 && (g_blink_tick & 16) == 0) {
        px_fill(caret_x, caret_y, 2, 10, BGRX_CURSOR);
    }
}

static void ensure_caret_visible(void) {
    /* Count line number of caret. */
    int line = 0;
    for (int i = 0; i < g_cur; i++) if (g_buf[i] == '\n') line++;
    if (line < g_scroll) g_scroll = line;
    if (line >= g_scroll + VISIBLE_LINES) g_scroll = line - VISIBLE_LINES + 1;
    if (g_scroll < 0) g_scroll = 0;
}

/* --- present --- */
static int present(void) {
    uint8_t *m = mmap(NULL, g_surface_bytes, PROT_READ|PROT_WRITE, MAP_SHARED, g_memfd, 0);
    if (m == MAP_FAILED) return -1;
    memcpy(m, g_pixels, g_surface_bytes);
    munmap(m, g_surface_bytes);

    vyro_msg_present_t p = {
        .window_id = g_window_id, .width = WIN_W, .height = WIN_H, .stride = WIN_W * 4,
    };
    return vyro_proto_send(VYRO_OP_WINDOW_PRESENT, &p, sizeof(p), g_memfd);
}

/* --- US QWERTY scancode → ASCII --- */
static char scancode_to_ascii(uint32_t code, int shift) {
    /* code = Linux KEY_* (input-event-codes.h). Cover the common subset. */
    static const char base[] =
        "  1234567890-=  qwertyuiop[]\n  asdfghjkl;'`  \\zxcvbnm,./";
    static const char shft[] =
        "  !@#$%^&*()_+  QWERTYUIOP{}\n  ASDFGHJKL:\"~  |ZXCVBNM<>?";
    if (code == 57) return ' ';                    /* KEY_SPACE */
    if (code == 15) return '\t';                   /* KEY_TAB */
    if (code < sizeof(base) - 1) {
        char c = shift ? shft[code] : base[code];
        if (c != ' ' || code == 57) return c;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc > 1) strncpy(g_path, argv[1], sizeof(g_path) - 1);
    else          strncpy(g_path, "/tmp/untitled.txt", sizeof(g_path) - 1);

    if (vyro_proto_connect() < 0) {
        fprintf(stderr, "textedit: cannot connect to compositor\n"); return 1;
    }

    vyro_msg_hello_t h = { .protocol_version = VYRO_PROTO_VERSION, .client_pid = (uint32_t)getpid() };
    vyro_proto_send(VYRO_OP_HELLO, &h, sizeof(h), -1);

    vyro_hdr_t hdr; uint8_t buf[1024]; int fd_in;
    if (vyro_proto_recv(&hdr, buf, sizeof(buf), &fd_in) < 0 || hdr.op != VYRO_OP_HELLO_OK) return 1;

    vyro_msg_window_create_t wc = { .width = WIN_W, .height = WIN_H, .flags = 0 };
    strncpy(wc.title, "TextEdit", sizeof(wc.title) - 1);
    vyro_proto_send(VYRO_OP_WINDOW_CREATE, &wc, sizeof(wc), -1);
    if (vyro_proto_recv(&hdr, buf, sizeof(buf), &fd_in) < 0 || hdr.op != VYRO_OP_WINDOW_OK) return 1;
    g_window_id = ((vyro_msg_window_ok_t *)buf)->window_id;

    g_pixels = calloc((size_t)WIN_W*WIN_H, 4);
    g_surface_bytes = (size_t)WIN_W*WIN_H*4;
    g_memfd = memfd_create("vyro-textedit-surface", MFD_CLOEXEC);
    if (g_memfd < 0 || ftruncate(g_memfd, g_surface_bytes) < 0) return 1;

    load_file(g_path);
    render(); present();

    int shift_down = 0, ctrl_down = 0;
    while (1) {
        int r = vyro_proto_recv(&hdr, buf, sizeof(buf), &fd_in);
        if (r < 0) break;
        if (hdr.op != VYRO_OP_EVENT || r < (int)sizeof(vyro_event_t)) { if (fd_in>=0) close(fd_in); continue; }
        vyro_event_t *ev = (vyro_event_t *)buf;
        int dirty = 0;

        if (ev->kind == VYRO_EV_KEY_DOWN) {
            switch (ev->code) {
            case 42: case 54: shift_down = 1; break;             /* L/R SHIFT */
            case 29: case 97: ctrl_down  = 1; break;             /* L/R CTRL */
            case 1:           goto done;                          /* ESC */
            case 14:          delete_byte_before_cur(); ensure_caret_visible(); dirty=1; break; /* BS */
            case 28: case 96: insert_byte('\n'); ensure_caret_visible(); dirty=1; break;       /* ENTER */
            case 103:         cursor_up();    ensure_caret_visible(); dirty=1; break;          /* UP */
            case 108:         cursor_down();  ensure_caret_visible(); dirty=1; break;          /* DOWN */
            case 105:         cursor_left();  ensure_caret_visible(); dirty=1; break;          /* LEFT */
            case 106:         cursor_right(); ensure_caret_visible(); dirty=1; break;          /* RIGHT */
            case 31: /* KEY_S */
                if (ctrl_down) { save_file(g_path); dirty = 1; break; }
                /* fallthrough */
            default: {
                char c = scancode_to_ascii(ev->code, shift_down);
                if (c) { insert_byte(c); ensure_caret_visible(); dirty = 1; }
                break;
            }
            }
        } else if (ev->kind == VYRO_EV_KEY_UP) {
            if (ev->code == 42 || ev->code == 54) shift_down = 0;
            if (ev->code == 29 || ev->code == 97) ctrl_down  = 0;
        } else if (ev->kind == VYRO_EV_CLOSE) {
            goto done;
        }

        g_blink_tick++;
        if (dirty || (g_blink_tick % 16) == 0) { render(); present(); }
        if (fd_in >= 0) close(fd_in);
    }
done:
    close(g_memfd);
    free(g_pixels);
    return 0;
}
