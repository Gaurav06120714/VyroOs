#include "apps.h"
#include "../widgets.h"
#include "../theme.h"
#include "../compositor.h"
#include "../pmm.h"
#include "../heap.h"
#include "../smp.h"
#include "../../drivers/timer.h"

static void render_taskmgr(app_ctx_t* c) {
    const theme_t* t = theme();
    comp_rect(c->origin_x, c->origin_y, c->width, c->height, t->win_body);
    int x = c->origin_x + 16, y = c->origin_y + 16;

    w_label_color(x, y, "System Monitor", t->accent_hi); y += 24;
    w_separator(x, y, c->width - 32); y += 12;

    // CPU
    w_label(x, y, "CPU"); y += 18;
    int cores = cpu_count();
    for (int i = 0; i < cores && i < 8; i++) {
        char b[20] = "core 0: ";
        b[5] = '0' + i;
        w_label_dim(x + 8, y, b);
        w_progress(x + 90, y + 2, c->width - 130, 12, 12 + i*8);
        y += 18;
    }
    y += 4; w_separator(x, y, c->width - 32); y += 12;

    // Memory
    w_label(x, y, "Memory"); y += 18;
    uint32_t used_pages = pmm_used_pages();
    uint32_t tot_pages  = pmm_total_pages();
    int pct = tot_pages ? (int)((uint64_t)used_pages * 100 / tot_pages) : 0;
    w_label_dim(x + 8, y, "Physical:");
    w_progress(x + 90, y + 2, c->width - 130, 12, pct);
    y += 20;
    w_label_dim(x + 8, y, "Heap:");
    uint64_t hu = heap_used(); uint64_t ht = heap_total();
    int hpct = ht ? (int)(hu * 100 / ht) : 0;
    w_progress(x + 90, y + 2, c->width - 130, 12, hpct);
    y += 24;
    w_separator(x, y, c->width - 32); y += 12;

    // Processes
    w_label(x, y, "Processes"); y += 20;
    w_label_dim(x + 8, y, "1  kernel/shell   (RUNNING)"); y += 18;
    w_label_dim(x + 8, y, "2  compositor     (RUNNING)"); y += 18;
    w_label_dim(x + 8, y, "3  desktop/gui    (RUNNING)");
}

const app_def_t APP_TASKMGR = { "Task Manager", '^', 0xFF8080, render_taskmgr, 540, 480 };
