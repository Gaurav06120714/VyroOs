/*
 * Vyro OS Core — DRM/KMS compositor skeleton (Path B, phase B3)
 *
 * Opens /dev/dri/card0, finds the first connected connector + CRTC, allocates
 * a dumb buffer the size of the preferred mode, fills it with the Vyro
 * accent color, and page-flips. Hold for 5 seconds, then exit.
 *
 * This is the minimum viable "we own the screen" demo. Subsequent phases
 * grow this into the full Vyro compositor (window list, dirty regions,
 * damage tracking, input dispatch).
 *
 * Build (on target): gcc -o vyro-compositor main.c -ldrm
 */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#define VYRO_ACCENT_BGRX 0x00B388FFu  /* glassmorphism accent (BGRX8888) */

struct vyro_fb {
    uint32_t fb_id;
    uint32_t handle;
    uint32_t pitch;
    uint64_t size;
    uint8_t *map;
    uint32_t width;
    uint32_t height;
};

/* Single screen for vB.0.3; multi-monitor is a later phase. */
static struct vyro_fb *g_screen_fb = NULL;

/* --- vB.0.3: surface for compositor-drm/src/server.c --- */

void vyro_screen_info(uint32_t *w, uint32_t *h) {
    if (g_screen_fb) { if (w) *w = g_screen_fb->width;  if (h) *h = g_screen_fb->height; }
    else             { if (w) *w = 0;                   if (h) *h = 0; }
}

void vyro_screen_blit(int dst_x, int dst_y,
                      const uint8_t *src, uint32_t src_w,
                      uint32_t src_h, uint32_t src_stride) {
    if (!g_screen_fb || !g_screen_fb->map || !src) return;
    int sw = (int)g_screen_fb->width;
    int sh = (int)g_screen_fb->height;
    int dpitch = (int)g_screen_fb->pitch;

    for (uint32_t row = 0; row < src_h; row++) {
        int yy = dst_y + (int)row;
        if (yy < 0 || yy >= sh) continue;
        for (uint32_t col = 0; col < src_w; col++) {
            int xx = dst_x + (int)col;
            if (xx < 0 || xx >= sw) continue;
            uint32_t *dst32 = (uint32_t *)(g_screen_fb->map + yy * dpitch);
            const uint32_t *src32 = (const uint32_t *)(src + row * src_stride);
            dst32[xx] = src32[col];
        }
    }
}

void vyro_screen_flush(void) {
    /* dumb buffer is mmap'd write-back; no explicit flush needed on x86 */
}

/* --- vB.0.4: simple fill primitives used by chrome.c --- */

void vyro_screen_fill_rect(int x, int y, int w, int h, uint32_t bgrx) {
    if (!g_screen_fb || !g_screen_fb->map) return;
    int sw = (int)g_screen_fb->width;
    int sh = (int)g_screen_fb->height;
    int dpitch = (int)g_screen_fb->pitch;

    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = (x + w) > sw ? sw : (x + w);
    int y1 = (y + h) > sh ? sh : (y + h);
    for (int yy = y0; yy < y1; yy++) {
        uint32_t *row = (uint32_t *)(g_screen_fb->map + yy * dpitch);
        for (int xx = x0; xx < x1; xx++) row[xx] = bgrx;
    }
}

void vyro_screen_fill_circle(int cx, int cy, int r, uint32_t bgrx) {
    if (!g_screen_fb || !g_screen_fb->map || r <= 0) return;
    int sw = (int)g_screen_fb->width;
    int sh = (int)g_screen_fb->height;
    int dpitch = (int)g_screen_fb->pitch;

    int r2 = r * r;
    for (int dy = -r; dy <= r; dy++) {
        int yy = cy + dy;
        if (yy < 0 || yy >= sh) continue;
        int dx_max = 0;
        /* simple int sqrt: scan-line width for this row */
        for (int t = r; t >= 0; t--) {
            if (dy * dy + t * t <= r2) { dx_max = t; break; }
        }
        uint32_t *row = (uint32_t *)(g_screen_fb->map + yy * dpitch);
        int x0 = cx - dx_max; if (x0 < 0)  x0 = 0;
        int x1 = cx + dx_max; if (x1 >= sw) x1 = sw - 1;
        for (int xx = x0; xx <= x1; xx++) row[xx] = bgrx;
    }
}

static int alloc_fb(int fd, uint32_t w, uint32_t h, struct vyro_fb *out) {
    struct drm_mode_create_dumb creq = {.width = w, .height = h, .bpp = 32};
    if (ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq) < 0) {
        perror("DRM_IOCTL_MODE_CREATE_DUMB");
        return -1;
    }
    out->handle = creq.handle;
    out->pitch  = creq.pitch;
    out->size   = creq.size;
    out->width  = w;
    out->height = h;

    if (drmModeAddFB(fd, w, h, 24, 32, creq.pitch, creq.handle, &out->fb_id)) {
        perror("drmModeAddFB");
        return -1;
    }

    struct drm_mode_map_dumb mreq = {.handle = creq.handle};
    if (ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq) < 0) {
        perror("DRM_IOCTL_MODE_MAP_DUMB");
        return -1;
    }
    out->map = mmap(NULL, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mreq.offset);
    if (out->map == MAP_FAILED) {
        perror("mmap");
        return -1;
    }
    return 0;
}

static void fill_solid(struct vyro_fb *fb, uint32_t bgrx) {
    uint32_t *px = (uint32_t *)fb->map;
    size_t n = (size_t)fb->pitch * fb->height / 4;
    for (size_t i = 0; i < n; i++) px[i] = bgrx;
}

int main(void) {
    int fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        fprintf(stderr, "vyro-compositor: open /dev/dri/card0: %s\n", strerror(errno));
        return 1;
    }

    drmModeRes *res = drmModeGetResources(fd);
    if (!res) {
        perror("drmModeGetResources");
        close(fd);
        return 1;
    }

    drmModeConnector *conn = NULL;
    for (int i = 0; i < res->count_connectors; i++) {
        drmModeConnector *c = drmModeGetConnector(fd, res->connectors[i]);
        if (c && c->connection == DRM_MODE_CONNECTED && c->count_modes > 0) {
            conn = c;
            break;
        }
        drmModeFreeConnector(c);
    }
    if (!conn) {
        fprintf(stderr, "vyro-compositor: no connected display\n");
        return 1;
    }

    drmModeModeInfo mode = conn->modes[0];
    drmModeEncoder *enc = drmModeGetEncoder(fd, conn->encoder_id);
    uint32_t crtc_id = enc ? enc->crtc_id : res->crtcs[0];
    drmModeCrtc *saved = drmModeGetCrtc(fd, crtc_id);

    struct vyro_fb fb = {0};
    if (alloc_fb(fd, mode.hdisplay, mode.vdisplay, &fb) < 0) return 1;
    fill_solid(&fb, VYRO_ACCENT_BGRX);

    if (drmModeSetCrtc(fd, crtc_id, fb.fb_id, 0, 0, &conn->connector_id, 1, &mode) < 0) {
        perror("drmModeSetCrtc");
        return 1;
    }

    /* vB.0.3: expose the dumb buffer to the IPC server and run its loop. */
    g_screen_fb = &fb;

    extern int  vyro_server_init(void);
    extern int  vyro_server_tick(int timeout_ms);
    extern int  vyro_input_init(void);
    extern void vyro_input_tick(void);
    if (vyro_server_init() < 0) return 1;
    if (vyro_input_init() < 0) {
        fprintf(stderr, "vyro-compositor: input init failed, continuing without input\n");
    }

    fprintf(stderr, "vyro-compositor: %ux%u — entering main loop\n",
            mode.hdisplay, mode.vdisplay);
    while (1) {
        if (vyro_server_tick(16) < 0) break;   /* ~60 Hz IPC + display tick */
        vyro_input_tick();                      /* drain libinput each frame */
    }

    if (saved) {
        drmModeSetCrtc(fd, saved->crtc_id, saved->buffer_id, saved->x, saved->y,
                       &conn->connector_id, 1, &saved->mode);
        drmModeFreeCrtc(saved);
    }
    drmModeFreeEncoder(enc);
    drmModeFreeConnector(conn);
    drmModeFreeResources(res);
    close(fd);
    return 0;
}
