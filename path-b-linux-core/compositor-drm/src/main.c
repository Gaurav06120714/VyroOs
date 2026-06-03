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

    fprintf(stderr, "vyro-compositor: held %ux%u for 5s\n", mode.hdisplay, mode.vdisplay);
    sleep(5);

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
