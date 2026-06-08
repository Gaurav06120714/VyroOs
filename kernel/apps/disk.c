#include "apps.h"
#include "../widgets.h"
#include "../theme.h"
#include "../compositor.h"
#include "../ata.h"
#include "../pmm.h"
#include "../heap.h"
#include "../notify.h"

static int sel_drive = 0;

static void render_disk(app_ctx_t* c) {
    const theme_t* t = theme();
    comp_rect(c->origin_x, c->origin_y, c->width, c->height, t->win_body);
    int abs_mx = c->mx + c->origin_x, abs_my = c->my + c->origin_y;


    int sw = 180;
    comp_rect(c->origin_x, c->origin_y, sw, c->height, t->dock_bg);
    comp_text(c->origin_x + 12, c->origin_y + 12, "Devices", t->accent_hi, t->dock_bg);
    const char* drives[] = { "vyro.img (boot)", "disk.img (scratch)" };
    for (int i = 0; i < 2; i++) {
        if (w_list_item(c->origin_x + 6, c->origin_y + 40 + i * 36, sw - 12, 32,
                         drives[i], i == sel_drive, abs_mx, abs_my, c->clicked))
            sel_drive = i;
    }


    int dx = c->origin_x + sw + 16, dy = c->origin_y + 16;
    comp_text(dx, dy, "Drive Info", t->accent_hi, t->win_body); dy += 24;
    w_separator(dx, dy, c->width - sw - 32); dy += 12;
    comp_text(dx, dy, sel_drive == 0 ? "Boot disk" : "Scratch disk",
              t->text, t->win_body); dy += 24;
    comp_text(dx, dy, "Interface: ATA PIO", t->text_dim, t->win_body); dy += 20;
    comp_text(dx, dy, "Sector size: 512 bytes", t->text_dim, t->win_body); dy += 20;
    if (sel_drive == 0) {
        comp_text(dx, dy, "Type: bootable image (read-only in OS)", t->text_dim, t->win_body); dy += 20;
        comp_text(dx, dy, "Used: kernel + bootloader", t->text_dim, t->win_body); dy += 30;
    } else {
        comp_text(dx, dy, "Type: persistent scratch", t->text_dim, t->win_body); dy += 20;
        comp_text(dx, dy, "Size: 1 MB (2048 sectors)", t->text_dim, t->win_body); dy += 20;
        comp_text(dx, dy, "Persists across reboots", t->success, t->win_body); dy += 30;
    }


    comp_text(dx, dy, "RAM Usage", t->text, t->win_body); dy += 22;
    uint32_t up = pmm_used_pages(), tp = pmm_total_pages();
    int pct = tp ? (int)((uint64_t)up * 100 / tp) : 0;
    w_progress(dx, dy, c->width - sw - 32, 14, pct); dy += 30;

    if (w_button(dx, dy, 130, 28, "Eject / Unmount", abs_mx, abs_my, c->clicked))
        notify_post("Disk", "Cannot unmount system drive");
    if (w_button(dx + 140, dy, 130, 28, "Format (test)", abs_mx, abs_my, c->clicked))
        notify_post("Disk", "Format blocked by ACL");
}

const app_def_t APP_DISK = { "Disk Utility", '@', 0xB0B0FF, render_disk, 560, 380 };
