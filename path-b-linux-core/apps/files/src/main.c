/*
 * Vyro Files (Path B port)
 *
 * Ported from kernel/apps/files on Path C. Walks a directory using
 * Linux's POSIX readdir() — same idea as the microkernel's vyfs_readdir
 * but landing on a real filesystem (the user's $HOME by default). UI is
 * a single-column list of names with kind tags (DIR/FILE), scrollable
 * and navigable with arrow keys, Enter to descend, Backspace to go up.
 *
 * Coexists with the Calculator from vB.0.6 so the compositor's
 * multi-client path gets real exercise.
 */

#define _GNU_SOURCE
#include <dirent.h>
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

#define WIN_W            560
#define WIN_H            420
#define ROW_H            28
#define VISIBLE_ROWS     ((WIN_H - 60) / ROW_H)

#define BGRX_BG          0x00141620u
#define BGRX_HEADER      0xC8141620u
#define BGRX_ROW         0x00141620u
#define BGRX_ROW_ALT     0x05FFFFFFu
#define BGRX_ROW_HOVER   0x30B388FFu
#define BGRX_FG          0x00E8E9F1u
#define BGRX_FG_MUTED    0x00A0A4B6u
#define BGRX_ACCENT      0x00B388FFu
#define BGRX_BORDER      0x14FFFFFFu

#define MAX_ENTRIES   512
#define MAX_NAME_LEN  255

typedef struct {
    char name[MAX_NAME_LEN + 1];
    int  is_dir;
} entry_t;

static entry_t  g_entries[MAX_ENTRIES];
static int      g_entry_count = 0;
static int      g_selected    = 0;
static int      g_scroll      = 0;
static char     g_cwd[1024]   = "";

static uint32_t *g_pixels;
static uint32_t  g_window_id;
static int       g_memfd = -1;
static size_t    g_surface_bytes;

/* --- minimal render primitives (same as calc, BGRX dumb-buffer) --- */
static void px_fill(int x, int y, int w, int h, uint32_t c) {
    int x0 = x<0?0:x, y0 = y<0?0:y;
    int x1 = (x+w)>WIN_W?WIN_W:(x+w), y1 = (y+h)>WIN_H?WIN_H:(y+h);
    for (int yy=y0; yy<y1; yy++)
        for (int xx=x0; xx<x1; xx++)
            g_pixels[yy*WIN_W+xx] = c;
}

static void px_text(int x, int y, const char *s, uint32_t c) {
    int cx = x;
    while (*s) { if (*s != ' ') px_fill(cx, y, 6, 10, c); cx += 8; s++; }
}

/* --- filesystem walk --- */
static int entry_cmp(const void *a, const void *b) {
    const entry_t *ea = a, *eb = b;
    if (ea->is_dir != eb->is_dir) return ea->is_dir ? -1 : 1;
    return strcmp(ea->name, eb->name);
}

static void load_cwd(void) {
    g_entry_count = 0;
    g_selected = 0;
    g_scroll   = 0;

    DIR *d = opendir(g_cwd);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) != NULL && g_entry_count < MAX_ENTRIES) {
        if (e->d_name[0] == '.' && e->d_name[1] == '\0') continue;
        entry_t *ent = &g_entries[g_entry_count++];
        strncpy(ent->name, e->d_name, MAX_NAME_LEN);
        ent->name[MAX_NAME_LEN] = '\0';
        if (e->d_type == DT_DIR)      ent->is_dir = 1;
        else if (e->d_type == DT_UNKNOWN) {
            /* Some filesystems (FUSE, network) return DT_UNKNOWN; stat() to disambiguate. */
            char path[1280];
            snprintf(path, sizeof(path), "%s/%s", g_cwd, ent->name);
            struct stat st;
            ent->is_dir = (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) ? 1 : 0;
        } else                            ent->is_dir = 0;
    }
    closedir(d);
    qsort(g_entries, g_entry_count, sizeof(entry_t), entry_cmp);
}

static void cd_into(const char *name) {
    if (strcmp(name, "..") == 0) {
        size_t n = strlen(g_cwd);
        while (n > 1 && g_cwd[n-1] != '/') n--;
        if (n > 1) n--;
        if (n == 0) n = 1;
        g_cwd[n] = '\0';
    } else {
        size_t cl = strlen(g_cwd);
        if (cl + 1 + strlen(name) + 1 >= sizeof(g_cwd)) return;
        if (g_cwd[cl-1] != '/') { g_cwd[cl++] = '/'; g_cwd[cl] = '\0'; }
        strncat(g_cwd, name, sizeof(g_cwd) - strlen(g_cwd) - 1);
    }
    load_cwd();
}

/* --- render --- */
static void render(void) {
    px_fill(0, 0, WIN_W, WIN_H, BGRX_BG);

    /* Header bar with cwd */
    px_fill(0, 0, WIN_W, 44, BGRX_HEADER);
    px_fill(0, 43, WIN_W, 1, BGRX_BORDER);
    px_text(16, 16, g_cwd, BGRX_FG);

    /* Rows */
    int y = 56;
    for (int i = g_scroll; i < g_entry_count && (i - g_scroll) < VISIBLE_ROWS; i++) {
        int row_y = y + (i - g_scroll) * ROW_H;
        uint32_t bg = (i == g_selected) ? BGRX_ROW_HOVER
                                        : ((i & 1) ? BGRX_ROW_ALT : BGRX_ROW);
        px_fill(8, row_y, WIN_W - 16, ROW_H - 2, bg);
        px_text(20,         row_y + 9, g_entries[i].name,
                g_entries[i].is_dir ? BGRX_ACCENT : BGRX_FG);
        px_text(WIN_W - 60, row_y + 9, g_entries[i].is_dir ? "DIR" : "FILE", BGRX_FG_MUTED);
    }

    /* Scrollbar */
    if (g_entry_count > VISIBLE_ROWS) {
        int bar_h = (VISIBLE_ROWS * (WIN_H - 60)) / g_entry_count;
        int bar_y = 56 + (g_scroll * (WIN_H - 60)) / g_entry_count;
        if (bar_h < 16) bar_h = 16;
        px_fill(WIN_W - 6, bar_y, 4, bar_h, BGRX_ACCENT);
    }
}

static void clamp_view(void) {
    if (g_selected < 0) g_selected = 0;
    if (g_selected >= g_entry_count) g_selected = g_entry_count - 1;
    if (g_selected < g_scroll) g_scroll = g_selected;
    if (g_selected >= g_scroll + VISIBLE_ROWS) g_scroll = g_selected - VISIBLE_ROWS + 1;
    if (g_scroll < 0) g_scroll = 0;
}

/* --- present (mmap memfd, memcpy, send WINDOW_PRESENT) --- */
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

/* --- main + event loop --- */
int main(int argc, char **argv) {
    /* Initial cwd: first argv or $HOME, fallback to / */
    const char *start = (argc > 1) ? argv[1] : getenv("HOME");
    if (!start || !*start) start = "/";
    strncpy(g_cwd, start, sizeof(g_cwd) - 1);

    if (vyro_proto_connect() < 0) {
        fprintf(stderr, "files: cannot connect to compositor\n"); return 1;
    }

    vyro_msg_hello_t h = { .protocol_version = VYRO_PROTO_VERSION, .client_pid = (uint32_t)getpid() };
    vyro_proto_send(VYRO_OP_HELLO, &h, sizeof(h), -1);

    vyro_hdr_t hdr; uint8_t buf[1024]; int fd_in;
    if (vyro_proto_recv(&hdr, buf, sizeof(buf), &fd_in) < 0 || hdr.op != VYRO_OP_HELLO_OK) return 1;

    vyro_msg_window_create_t wc = { .width = WIN_W, .height = WIN_H, .flags = 0 };
    strncpy(wc.title, "Files", sizeof(wc.title) - 1);
    vyro_proto_send(VYRO_OP_WINDOW_CREATE, &wc, sizeof(wc), -1);
    if (vyro_proto_recv(&hdr, buf, sizeof(buf), &fd_in) < 0 || hdr.op != VYRO_OP_WINDOW_OK) return 1;
    g_window_id = ((vyro_msg_window_ok_t *)buf)->window_id;

    g_pixels = calloc((size_t)WIN_W*WIN_H, 4);
    g_surface_bytes = (size_t)WIN_W*WIN_H*4;
    g_memfd = memfd_create("vyro-files-surface", MFD_CLOEXEC);
    if (g_memfd < 0 || ftruncate(g_memfd, g_surface_bytes) < 0) return 1;

    load_cwd();
    render(); present();

    while (1) {
        int r = vyro_proto_recv(&hdr, buf, sizeof(buf), &fd_in);
        if (r < 0) break;
        if (hdr.op != VYRO_OP_EVENT || r < (int)sizeof(vyro_event_t)) { if (fd_in>=0) close(fd_in); continue; }
        vyro_event_t *ev = (vyro_event_t *)buf;
        int dirty = 0;
        if (ev->kind == VYRO_EV_KEY_DOWN) {
            /* Linux KEY_* codes (input-event-codes.h) */
            switch (ev->code) {
            case 103: g_selected--; clamp_view(); dirty = 1; break;            /* KEY_UP */
            case 108: g_selected++; clamp_view(); dirty = 1; break;            /* KEY_DOWN */
            case 28:  case 96:                                                  /* KEY_ENTER / KP_ENTER */
                if (g_selected >= 0 && g_selected < g_entry_count
                    && g_entries[g_selected].is_dir) {
                    cd_into(g_entries[g_selected].name); dirty = 1;
                }
                break;
            case 14:  cd_into(".."); dirty = 1; break;                          /* KEY_BACKSPACE */
            case 1:   goto done;                                                /* KEY_ESC */
            default:  break;
            }
        } else if (ev->kind == VYRO_EV_MOUSE_DOWN) {
            int row = (ev->y - 56) / ROW_H + g_scroll;
            if (row >= 0 && row < g_entry_count) {
                g_selected = row;
                if (g_entries[row].is_dir) cd_into(g_entries[row].name);
                dirty = 1;
            }
        } else if (ev->kind == VYRO_EV_CLOSE) {
            goto done;
        }
        if (dirty) { render(); present(); }
        if (fd_in >= 0) close(fd_in);
    }
done:
    close(g_memfd);
    free(g_pixels);
    return 0;
}
