/*
 * Vyro Compositor — input layer (vB.0.5)
 *
 * Uses libinput's udev backend to enumerate every /dev/input/event* node,
 * pulls keyboard + pointer events, and dispatches them to clients as
 * VYRO_EV_* messages on the IPC socket.
 *
 * Focus model: pointer-on-window. The compositor tracks a cursor (x,y)
 * and an int focus_window. Whichever window's box the cursor is inside
 * gets every event. If the cursor is over chrome (the title bar), the
 * event is consumed by the compositor for drag-to-move.
 */

#define _GNU_SOURCE
#include "../../libvyro-linux/include/vyro_proto.h"

#include <errno.h>
#include <fcntl.h>
#include <libinput.h>
#include <libudev.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <unistd.h>

extern void vyro_screen_info(uint32_t *w, uint32_t *h);

/* Hooks into server.c — implemented there in vB.0.5 alongside this file. */
extern int  vyro_server_window_hit(int cursor_x, int cursor_y,
                                   int *out_in_chrome, int *out_dx, int *out_dy);
extern void vyro_server_window_move(int window_idx, int dx, int dy);
extern void vyro_server_send_event_to(int window_idx, const vyro_event_t *ev);

/* --- libinput callbacks: udev open/close --- */
static int open_restricted(const char *path, int flags, void *u) {
    (void)u;
    int fd = open(path, flags | O_CLOEXEC);
    return fd < 0 ? -errno : fd;
}
static void close_restricted(int fd, void *u) { (void)u; close(fd); }

static const struct libinput_interface g_iface = {
    .open_restricted  = open_restricted,
    .close_restricted = close_restricted,
};

/* --- cursor + focus state --- */
static struct libinput *g_li = NULL;
static int    g_cursor_x = 100, g_cursor_y = 100;
static int    g_screen_w = 1, g_screen_h = 1;
static int    g_dragging_window = -1;  /* window index being dragged via title bar */

static void clamp_cursor(void) {
    if (g_cursor_x < 0)             g_cursor_x = 0;
    if (g_cursor_x >= g_screen_w)   g_cursor_x = g_screen_w - 1;
    if (g_cursor_y < 0)             g_cursor_y = 0;
    if (g_cursor_y >= g_screen_h)   g_cursor_y = g_screen_h - 1;
}

int vyro_input_init(void) {
    struct udev *udev = udev_new();
    if (!udev) return -1;

    g_li = libinput_udev_create_context(&g_iface, NULL, udev);
    if (!g_li) { udev_unref(udev); return -1; }
    if (libinput_udev_assign_seat(g_li, "seat0") < 0) {
        libinput_unref(g_li); g_li = NULL; udev_unref(udev); return -1;
    }
    udev_unref(udev);

    uint32_t sw, sh; vyro_screen_info(&sw, &sh);
    g_screen_w = (int)(sw ? sw : 1024);
    g_screen_h = (int)(sh ? sh : 768);
    fprintf(stderr, "compositor: input ready (%dx%d cursor)\n", g_screen_w, g_screen_h);
    return 0;
}

int vyro_input_fd(void) { return g_li ? libinput_get_fd(g_li) : -1; }

static void dispatch_pointer_motion(double dx, double dy) {
    g_cursor_x += (int)dx;
    g_cursor_y += (int)dy;
    clamp_cursor();

    /* If we're mid-drag from the title bar, move the window with us. */
    if (g_dragging_window >= 0) {
        vyro_server_window_move(g_dragging_window, (int)dx, (int)dy);
        return;
    }

    /* Otherwise hit-test and forward as motion event to the window. */
    int in_chrome = 0, content_dx = 0, content_dy = 0;
    int idx = vyro_server_window_hit(g_cursor_x, g_cursor_y,
                                     &in_chrome, &content_dx, &content_dy);
    if (idx >= 0 && !in_chrome) {
        vyro_event_t ev = { .kind = VYRO_EV_MOUSE_MOVE,
                            .x = content_dx, .y = content_dy, .code = 0 };
        vyro_server_send_event_to(idx, &ev);
    }
}

static void dispatch_button(uint32_t button, int down) {
    int in_chrome = 0, content_dx = 0, content_dy = 0;
    int idx = vyro_server_window_hit(g_cursor_x, g_cursor_y,
                                     &in_chrome, &content_dx, &content_dy);
    if (in_chrome && down && button == 0x110 /* BTN_LEFT */) {
        g_dragging_window = idx;
        return;
    }
    if (button == 0x110 && !down) g_dragging_window = -1;

    if (idx >= 0 && !in_chrome) {
        vyro_event_t ev = {
            .kind = down ? VYRO_EV_MOUSE_DOWN : VYRO_EV_MOUSE_UP,
            .x = content_dx, .y = content_dy, .code = (int)button,
        };
        vyro_server_send_event_to(idx, &ev);
    }
}

static void dispatch_key(uint32_t key, int down) {
    /* Keyboard events go to whichever window currently holds the pointer.
     * Real focus-follows-click and explicit focus model lands in vB.0.6. */
    int in_chrome = 0, content_dx = 0, content_dy = 0;
    int idx = vyro_server_window_hit(g_cursor_x, g_cursor_y,
                                     &in_chrome, &content_dx, &content_dy);
    if (idx < 0) return;
    vyro_event_t ev = {
        .kind = down ? VYRO_EV_KEY_DOWN : VYRO_EV_KEY_UP,
        .x = 0, .y = 0, .code = (int)key,
    };
    vyro_server_send_event_to(idx, &ev);
}

void vyro_input_tick(void) {
    if (!g_li) return;
    libinput_dispatch(g_li);

    struct libinput_event *e;
    while ((e = libinput_get_event(g_li)) != NULL) {
        switch (libinput_event_get_type(e)) {
        case LIBINPUT_EVENT_POINTER_MOTION: {
            struct libinput_event_pointer *p = libinput_event_get_pointer_event(e);
            dispatch_pointer_motion(libinput_event_pointer_get_dx(p),
                                    libinput_event_pointer_get_dy(p));
            break;
        }
        case LIBINPUT_EVENT_POINTER_BUTTON: {
            struct libinput_event_pointer *p = libinput_event_get_pointer_event(e);
            uint32_t b = libinput_event_pointer_get_button(p);
            int down = libinput_event_pointer_get_button_state(p) ==
                       LIBINPUT_BUTTON_STATE_PRESSED;
            dispatch_button(b, down);
            break;
        }
        case LIBINPUT_EVENT_KEYBOARD_KEY: {
            struct libinput_event_keyboard *k = libinput_event_get_keyboard_event(e);
            uint32_t code = libinput_event_keyboard_get_key(k);
            int down = libinput_event_keyboard_get_key_state(k) ==
                       LIBINPUT_KEY_STATE_PRESSED;
            dispatch_key(code, down);
            break;
        }
        default:
            break;
        }
        libinput_event_destroy(e);
    }
}

void vyro_input_cursor(int *x, int *y) {
    if (x) *x = g_cursor_x;
    if (y) *y = g_cursor_y;
}
