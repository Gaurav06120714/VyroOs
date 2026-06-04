/*
 * Vyro Settings (Path B port)
 *
 * Fifth concurrent client. Renders a vertical stack of stateful
 * widgets — toggles (on/off pill), sliders (track + accent fill +
 * draggable handle), and section labels. Settings persist to
 * ~/.config/vyro/settings.conf as `key=value` lines.
 *
 * Demonstrates: stateful interactive widgets, hit-testing against
 * widget bounds, fractional drag dynamics on a slider track, persistent
 * config I/O. Real settings ("set wallpaper", "open at login") would
 * dispatch on the value changing — for vB.0.13 the values are stored
 * but actions are no-ops.
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
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "../../../libvyro-linux/include/vyro.h"
#include "../../../libvyro-linux/include/vyro_proto.h"
#include "../../../libvyro-linux/include/vyro_font.h"

int  vyro_proto_connect(void);
int  vyro_proto_send(uint16_t op, const void *p, uint32_t plen, int fd);
int  vyro_proto_recv(vyro_hdr_t *hdr, void *buf, uint32_t buflen, int *out_fd);

#define WIN_W           520
#define WIN_H           480
#define ROW_H           56
#define ROW_TOP         60
#define LEFT_PAD        24
#define RIGHT_PAD       24

#define BGRX_BG         0x00141620u
#define BGRX_HEADER     0xC8141620u
#define BGRX_ROW        0x05FFFFFFu
#define BGRX_FG         0x00E8E9F1u
#define BGRX_FG_MUTED   0x00A0A4B6u
#define BGRX_ACCENT     0x00B388FFu
#define BGRX_TRACK      0x14FFFFFFu

typedef enum { W_TOGGLE, W_SLIDER, W_SECTION } widget_kind_t;

typedef struct {
    widget_kind_t kind;
    const char    *label;
    const char    *key;
    int            value;       /* toggle: 0/1.  slider: 0..100 */
} widget_t;

static widget_t WIDGETS[] = {
    { W_SECTION, "Appearance",      NULL,         0 },
    { W_TOGGLE,  "Dark mode",       "dark_mode",  1 },
    { W_TOGGLE,  "Reduce motion",   "reduce_motion", 0 },
    { W_SLIDER,  "Accent intensity","accent",     70 },

    { W_SECTION, "System",          NULL,         0 },
    { W_TOGGLE,  "Open at login",   "autostart",  1 },
    { W_SLIDER,  "Brightness",      "brightness", 80 },
    { W_SLIDER,  "Volume",          "volume",     55 },
};
#define N_WIDGETS  (sizeof(WIDGETS)/sizeof(WIDGETS[0]))

static uint32_t *g_pixels;
static uint32_t  g_window_id;
static int       g_memfd = -1;
static size_t    g_surface_bytes;
static int       g_drag_widget = -1;
static char      g_config_path[1024];

/* --- BGRX primitives --- */
static void px_fill(int x, int y, int w, int h, uint32_t c) {
    int x0=x<0?0:x, y0=y<0?0:y;
    int x1=(x+w)>WIN_W?WIN_W:(x+w), y1=(y+h)>WIN_H?WIN_H:(y+h);
    for (int yy=y0; yy<y1; yy++)
        for (int xx=x0; xx<x1; xx++)
            g_pixels[yy*WIN_W+xx] = c;
}

static void px_round_rect(int x, int y, int w, int h, int r, uint32_t color) {
    /* core fill */
    px_fill(x + r, y, w - 2*r, h, color);
    px_fill(x, y + r, w, h - 2*r, color);
    /* approximate the corners with circle masks */
    int r2 = r * r;
    int corners[4][2] = { {x+r, y+r}, {x+w-r-1, y+r}, {x+r, y+h-r-1}, {x+w-r-1, y+h-r-1} };
    for (int k = 0; k < 4; k++) {
        int cx = corners[k][0], cy = corners[k][1];
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                if (dx*dx + dy*dy <= r2) px_fill(cx + dx, cy + dy, 1, 1, color);
            }
        }
    }
}

static void text_at(int x, int y, const char *s, uint32_t color) {
    vyro_font_text(g_pixels, WIN_W, WIN_W, WIN_H, x, y, s, color);
}

/* --- row geometry (only counts non-section widgets toward y position) --- */
static int row_y(int idx) {
    int y = ROW_TOP;
    for (size_t i = 0; i < (size_t)idx; i++) {
        y += (WIDGETS[i].kind == W_SECTION) ? 36 : ROW_H;
    }
    return y;
}

/* --- render each widget --- */
static void render_toggle(int x, int y, int on) {
    int W = 44, H = 22;
    int track_x = x + WIN_W - RIGHT_PAD - LEFT_PAD - W;
    px_round_rect(track_x, y + (ROW_H - H)/2, W, H, H/2,
                  on ? BGRX_ACCENT : BGRX_TRACK);
    int knob_r = (H - 4) / 2;
    int knob_cx = on ? track_x + W - knob_r - 2 : track_x + knob_r + 2;
    int knob_cy = y + ROW_H/2;
    px_round_rect(knob_cx - knob_r, knob_cy - knob_r, knob_r*2, knob_r*2, knob_r,
                  0x00FFFFFFu);
}

static void render_slider(int x, int y, int value) {
    int track_w = 200;
    int track_x = x + WIN_W - RIGHT_PAD - LEFT_PAD - track_w;
    int track_y = y + ROW_H/2 - 2;
    px_round_rect(track_x, track_y, track_w, 4, 2, BGRX_TRACK);

    int fill_w = (track_w * value) / 100;
    px_round_rect(track_x, track_y, fill_w, 4, 2, BGRX_ACCENT);

    int handle_cx = track_x + fill_w;
    int handle_cy = y + ROW_H/2;
    int handle_r = 7;
    px_round_rect(handle_cx - handle_r, handle_cy - handle_r,
                  handle_r*2, handle_r*2, handle_r, 0x00FFFFFFu);
}

/* --- render full window --- */
static void render(void) {
    px_fill(0, 0, WIN_W, WIN_H, BGRX_BG);
    px_fill(0, 0, WIN_W, 44, BGRX_HEADER);
    px_fill(0, 43, WIN_W, 1, BGRX_TRACK);
    text_at(LEFT_PAD, 16, "Settings", BGRX_FG);

    for (size_t i = 0; i < N_WIDGETS; i++) {
        int y = row_y((int)i);
        widget_t *w = &WIDGETS[i];
        if (w->kind == W_SECTION) {
            text_at(LEFT_PAD, y + 10, w->label, BGRX_FG_MUTED);
            continue;
        }
        px_fill(LEFT_PAD - 8, y, WIN_W - 2*(LEFT_PAD - 8), ROW_H - 4, BGRX_ROW);
        text_at(LEFT_PAD, y + (ROW_H/2) - 5, w->label, BGRX_FG);
        if (w->kind == W_TOGGLE) render_toggle(LEFT_PAD, y, w->value);
        else if (w->kind == W_SLIDER) render_slider(LEFT_PAD, y, w->value);
    }
}

/* --- hit-test: which widget did this click land in, plus per-kind helper --- */
static int hit_widget(int x, int y, int *out_track_x, int *out_track_w) {
    for (size_t i = 0; i < N_WIDGETS; i++) {
        if (WIDGETS[i].kind == W_SECTION) continue;
        int wy = row_y((int)i);
        if (y < wy || y >= wy + ROW_H) continue;
        if (x < LEFT_PAD - 8 || x >= WIN_W - (LEFT_PAD - 8)) continue;

        if (WIDGETS[i].kind == W_SLIDER) {
            int track_w = 200;
            int track_x = LEFT_PAD + WIN_W - RIGHT_PAD - LEFT_PAD - track_w;
            if (out_track_x) *out_track_x = track_x;
            if (out_track_w) *out_track_w = track_w;
        }
        return (int)i;
    }
    return -1;
}

/* --- persistence: ~/.config/vyro/settings.conf --- */
static void load_config(void) {
    FILE *f = fopen(g_config_path, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        int val = atoi(eq + 1);
        for (size_t i = 0; i < N_WIDGETS; i++)
            if (WIDGETS[i].key && strcmp(WIDGETS[i].key, line) == 0)
                WIDGETS[i].value = val;
    }
    fclose(f);
}

static void save_config(void) {
    /* ensure ~/.config/vyro/ exists */
    const char *home = getenv("HOME");
    if (home) {
        char dir[512];
        snprintf(dir, sizeof(dir), "%s/.config", home);       mkdir(dir, 0755);
        snprintf(dir, sizeof(dir), "%s/.config/vyro", home);  mkdir(dir, 0755);
    }
    FILE *f = fopen(g_config_path, "w");
    if (!f) return;
    for (size_t i = 0; i < N_WIDGETS; i++) {
        if (!WIDGETS[i].key) continue;
        fprintf(f, "%s=%d\n", WIDGETS[i].key, WIDGETS[i].value);
    }
    fclose(f);
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

int main(void) {
    const char *home = getenv("HOME");
    snprintf(g_config_path, sizeof(g_config_path),
             "%s/.config/vyro/settings.conf", home ? home : "/tmp");

    if (vyro_proto_connect() < 0) { fprintf(stderr, "settings: no compositor\n"); return 1; }
    vyro_msg_hello_t h = { .protocol_version = VYRO_PROTO_VERSION, .client_pid = (uint32_t)getpid() };
    vyro_proto_send(VYRO_OP_HELLO, &h, sizeof(h), -1);
    vyro_hdr_t hdr; uint8_t buf[1024]; int fd_in;
    if (vyro_proto_recv(&hdr, buf, sizeof(buf), &fd_in) < 0 || hdr.op != VYRO_OP_HELLO_OK) return 1;

    vyro_msg_window_create_t wc = { .width=WIN_W, .height=WIN_H, .flags=0 };
    strncpy(wc.title, "Settings", sizeof(wc.title)-1);
    vyro_proto_send(VYRO_OP_WINDOW_CREATE, &wc, sizeof(wc), -1);
    if (vyro_proto_recv(&hdr, buf, sizeof(buf), &fd_in) < 0 || hdr.op != VYRO_OP_WINDOW_OK) return 1;
    g_window_id = ((vyro_msg_window_ok_t *)buf)->window_id;

    g_pixels = calloc((size_t)WIN_W*WIN_H, 4);
    g_surface_bytes = (size_t)WIN_W*WIN_H*4;
    g_memfd = memfd_create("vyro-settings-surface", MFD_CLOEXEC);
    if (g_memfd < 0 || ftruncate(g_memfd, g_surface_bytes) < 0) return 1;

    load_config();
    render(); present();

    while (1) {
        int r = vyro_proto_recv(&hdr, buf, sizeof(buf), &fd_in);
        if (r < 0) break;
        if (hdr.op != VYRO_OP_EVENT || r < (int)sizeof(vyro_event_t)) {
            if (fd_in >= 0) close(fd_in); continue;
        }
        vyro_event_t *ev = (vyro_event_t *)buf;
        int dirty = 0;

        if (ev->kind == VYRO_EV_MOUSE_DOWN) {
            int tx = 0, tw = 0;
            int idx = hit_widget(ev->x, ev->y, &tx, &tw);
            if (idx >= 0) {
                widget_t *w = &WIDGETS[idx];
                if (w->kind == W_TOGGLE) { w->value = !w->value; dirty = 1; save_config(); }
                else if (w->kind == W_SLIDER) {
                    g_drag_widget = idx;
                    int rel = ev->x - tx; if (rel < 0) rel = 0; if (rel > tw) rel = tw;
                    w->value = (rel * 100) / tw;
                    dirty = 1;
                }
            }
        } else if (ev->kind == VYRO_EV_MOUSE_MOVE && g_drag_widget >= 0) {
            int tx = 0, tw = 0;
            (void)hit_widget(0, row_y(g_drag_widget) + ROW_H/2, &tx, &tw);
            int rel = ev->x - tx; if (rel < 0) rel = 0; if (rel > tw) rel = tw;
            WIDGETS[g_drag_widget].value = (rel * 100) / tw;
            dirty = 1;
        } else if (ev->kind == VYRO_EV_MOUSE_UP) {
            if (g_drag_widget >= 0) { save_config(); g_drag_widget = -1; }
        } else if (ev->kind == VYRO_EV_KEY_DOWN && ev->code == 1) {
            break;
        } else if (ev->kind == VYRO_EV_CLOSE) {
            break;
        }

        if (dirty) { render(); present(); }
        if (fd_in >= 0) close(fd_in);
    }

    save_config();
    close(g_memfd);
    free(g_pixels);
    return 0;
}
