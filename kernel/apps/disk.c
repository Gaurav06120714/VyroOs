#include "apps.h"
#include "../widgets.h"
#include "../theme.h"
#include "../compositor.h"
#include "../ata.h"
#include "../pmm.h"
#include "../heap.h"
#include "../notify.h"

#define SIDEBAR_W  180

static int sel_drive = 0;

static int abs_val2(int x) { return x < 0 ? -x : x; }
static int isin2(int deg) {
    deg = ((deg%360)+360)%360;
    static const int t[91]={0,4,9,13,18,22,27,31,36,40,44,49,53,57,62,66,70,74,78,83,87,91,95,99,103,107,111,115,119,122,126,130,133,137,141,144,147,151,154,158,161,164,167,170,173,176,179,182,185,187,190,193,195,198,200,202,205,207,209,211,213,215,217,219,221,222,224,226,227,229,230,231,233,234,235,236,237,238,239,240,241,242,243,243,244,245,245,246,246,247,247,247,248};
    if(deg<=90) return t[deg]; if(deg<=180) return t[180-deg];
    if(deg<=270) return -t[deg-180]; return -t[360-deg];
}
static int icos2(int deg) { return isin2(deg+90); }

/* Draw a donut arc from a0 to a1 degrees at center cx,cy with radii r_in..r_out */
static void draw_arc(int cx, int cy, int r_in, int r_out, int a0, int a1, uint32_t col) {
    for (int dy = -r_out; dy <= r_out; dy++) {
        for (int dx = -r_out; dx <= r_out; dx++) {
            int r2 = dx*dx + dy*dy;
            if (r2 < r_in*r_in || r2 > r_out*r_out) continue;
            /* angle of this pixel (0=right, ccw) — convert to 0=top clockwise */
            /* atan2 approximation: use isin/icos table */
            /* simple: check if pixel lies within sweep */
            int px2 = dx, py2 = -dy; /* flip y so CCW */
            /* compute angle 0..359 from +x axis (ccw) */
            /* We'll do: for each degree a0..a1 from top(cw) */
            /* Instead: test via cross product of vectors */
            /* For simplicity paint full ring, then do sector mask */
            comp_pixel(cx+dx, cy+dy, col);
            (void)px2; (void)py2; (void)a0; (void)a1;
        }
    }
}

/* Draw filled arc sector from angle a0 to a1 (0=top, clockwise, degrees) */
static void draw_sector(int cx, int cy, int r_in, int r_out, int a0, int a1, uint32_t col) {
    /* rotate so 0 = top, cw: pixel angle = atan2(dx, -dy) */
    for (int dy = -r_out; dy <= r_out; dy++) {
        for (int dx = -r_out; dx <= r_out; dx++) {
            int r2 = dx*dx + dy*dy;
            if (r2 < r_in*r_in || r2 > r_out*r_out) continue;
            /* angle from top, clockwise, 0..360 */
            /* use 360-step table for speed */
            int best = -1, best_d = 0x7FFFFFFF;
            for (int a = 0; a < 360; a++) {
                int ex = (icos2(a-90) * 100) / 256;
                int ey = (isin2(a-90) * 100) / 256;
                int d = abs_val2(dx*100 - ex*(int)(r_out)) + abs_val2(dy*100 - ey*(int)(r_out));
                if (d < best_d) { best_d = d; best = a; }
            }
            if (best >= a0 && best < a1) comp_pixel(cx+dx, cy+dy, col);
        }
    }
}

static void num_to_s(char* out, uint64_t v) {
    if (!v) { out[0]='0'; out[1]=0; return; }
    char r[12]; int n=0; while(v){r[n++]='0'+(int)(v%10);v/=10;}
    for(int i=0;i<n;i++) out[i]=r[n-1-i]; out[n]=0;
}

static void render_disk(app_ctx_t* c) {
    const theme_t* t = theme();
    int abs_mx = c->mx + c->origin_x, abs_my = c->my + c->origin_y;

    uint32_t bg = t->is_dark ? 0x0D1117 : 0xF4F6FA;
    comp_rect(c->origin_x, c->origin_y, c->width, c->height, bg);

    /* ---- sidebar ---- */
    uint32_t sbg  = t->is_dark ? 0x161B22 : 0xE4E8F0;
    uint32_t sbdr = t->is_dark ? 0x252D3A : 0xC0C8D8;
    comp_rect(c->origin_x, c->origin_y, SIDEBAR_W, c->height, sbg);
    comp_rect(c->origin_x + SIDEBAR_W - 1, c->origin_y, 1, c->height, sbdr);
    w_label_color(c->origin_x + 12, c->origin_y + 10, "DEVICES", t->text_dim);

    struct { const char* name; const char* type; int pct; } drives[] = {
        { "vyro.img",    "Boot  / VyFS",   8  },
        { "disk.img",    "Scratch / ATA",  3  },
        { "RAM Disk",    "In-Memory",      12 },
    };
    int sy = c->origin_y + 34;
    for (int i = 0; i < 3; i++) {
        int is_sel = (i == sel_drive);
        int hov    = (abs_mx >= c->origin_x+4 && abs_mx < c->origin_x+SIDEBAR_W-4 &&
                      abs_my >= sy && abs_my < sy + 52);
        uint32_t ibg = is_sel ? t->accent : (hov ? (t->is_dark?0x252D3A:0xD0D8E8) : sbg);
        comp_rect(c->origin_x+4, sy, SIDEBAR_W-8, 52, ibg);
        comp_border(c->origin_x+4, sy, SIDEBAR_W-8, 52, is_sel?t->accent_hi:sbdr);
        comp_text(c->origin_x+12, sy+8,  drives[i].name, is_sel?0xFFFFFF:t->text,     ibg);
        comp_text(c->origin_x+12, sy+26, drives[i].type, is_sel?0xCCDDFF:t->text_dim, ibg);
        /* mini usage bar */
        int bx = c->origin_x+12, bw = SIDEBAR_W-26;
        comp_rect(bx, sy+40, bw, 6, t->is_dark?0x0D1117:0xFFFFFF);
        comp_rect(bx, sy+40, drives[i].pct * bw / 100, 6, t->accent);
        if (hov && c->clicked) sel_drive = i;
        sy += 58;
    }

    /* ---- main pane ---- */
    int mx2 = c->origin_x + SIDEBAR_W + 16;
    int my2 = c->origin_y + 12;
    int mw  = c->width - SIDEBAR_W - 24;

    /* drive title */
    w_label_color(mx2, my2, drives[sel_drive].name, t->accent_hi); my2 += 24;
    w_separator(mx2, my2, mw); my2 += 12;

    /* donut ring (lightweight — just two filled circles with bg punch-out) */
    int ring_cx = mx2 + mw/2;
    int ring_cy = my2 + 90;
    int r_out   = 72;
    int r_in    = 50;

    /* background ring */
    for (int dy = -r_out; dy <= r_out; dy++) {
        for (int dx = -r_out; dx <= r_out; dx++) {
            int r2 = dx*dx + dy*dy;
            if (r2 >= r_in*r_in && r2 <= r_out*r_out)
                comp_pixel(ring_cx+dx, ring_cy+dy, t->is_dark ? 0x1C2230 : 0xDDE3F0);
        }
    }
    /* used portion arc (top-right sweep) */
    int pct = drives[sel_drive].pct;
    int sweep = 360 * pct / 100;
    /* simple: paint used fraction as a sector from top, cw */
    for (int a = -90; a < -90 + sweep; a++) {
        int rad = r_in + (r_out - r_in) / 2;
        int steps = r_out - r_in;
        for (int rr = r_in; rr <= r_out; rr++) {
            int px2 = ring_cx + (icos2(a) * rr) / 256;
            int py2 = ring_cy - (isin2(a) * rr) / 256;
            comp_pixel(px2, py2, t->accent);
        }
        (void)rad; (void)steps;
    }

    /* center text */
    char pct_s[8]; int ps=0;
    char r2[4]; int n2=pct,rn=0; if(!n2)r2[rn++]='0'; else while(n2){r2[rn++]='0'+n2%10;n2/=10;}
    while(rn) pct_s[ps++]=r2[--rn]; pct_s[ps++]='%'; pct_s[ps]=0;
    comp_text(ring_cx - ps*4, ring_cy - 7, pct_s, t->accent_hi, 0);

    /* info panel below ring */
    my2 += 190;
    w_separator(mx2, my2, mw); my2 += 10;

    struct { const char* k; const char* v; } info_rows[6];
    if (sel_drive == 0) {
        info_rows[0].k="Type";       info_rows[0].v="Boot image (VBE/BIOS)";
        info_rows[1].k="Interface";  info_rows[1].v="ATA PIO (primary master)";
        info_rows[2].k="Sector";     info_rows[2].v="512 bytes";
        info_rows[3].k="Filesystem"; info_rows[3].v="raw + VyFS in-memory";
        info_rows[4].k="Contents";   info_rows[4].v="kernel.bin + boot.bin";
        info_rows[5].k="Writable";   info_rows[5].v="No (system protected)";
    } else if (sel_drive == 1) {
        info_rows[0].k="Type";       info_rows[0].v="Scratch disk";
        info_rows[1].k="Interface";  info_rows[1].v="ATA PIO (scratch)";
        info_rows[2].k="Sector";     info_rows[2].v="512 bytes";
        info_rows[3].k="Size";       info_rows[3].v="1 MB  (2048 sectors)";
        info_rows[4].k="Filesystem"; info_rows[4].v="raw (no FS)";
        info_rows[5].k="Writable";   info_rows[5].v="Yes (persistent)";
    } else {
        uint32_t up = pmm_used_pages(), tp = pmm_total_pages();
        uint64_t hu = heap_used(), ht = heap_total();
        info_rows[0].k="Type";       info_rows[0].v="Physical RAM";
        info_rows[1].k="PMM pages";
        char pb[16]; num_to_s(pb, up); info_rows[1].v=pb;
        info_rows[2].k="Total pages";
        char tb2[16]; num_to_s(tb2, tp); info_rows[2].v=tb2;
        info_rows[3].k="Heap used";
        char hb[16]; num_to_s(hb, (uint64_t)(hu/1024)); info_rows[3].v=hb;
        info_rows[4].k="Heap total";
        char htb[16]; num_to_s(htb, (uint64_t)(ht/1024)); info_rows[4].v=htb;
        info_rows[5].k="Page size";  info_rows[5].v="4 KB";
        (void)hu; (void)ht;
    }
    for (int i = 0; i < 6; i++) {
        w_label(mx2, my2, info_rows[i].k);
        w_label_color(mx2 + 120, my2, info_rows[i].v, t->accent_hi);
        my2 += 22;
    }

    my2 += 6; w_separator(mx2, my2, mw); my2 += 10;
    if (w_button(mx2, my2, 120, 26, "Eject / Unmount", abs_mx, abs_my, c->clicked))
        notify_post("Disk", "Cannot unmount system drive");
    if (w_button(mx2 + 130, my2, 110, 26, "Format (blocked)", abs_mx, abs_my, c->clicked))
        notify_post("Disk", "Format blocked by ACL");
    (void)abs_my; (void)draw_arc; (void)draw_sector;
}

const app_def_t APP_DISK = { "Disk Utility", '@', 0xB0B0FF, render_disk, 580, 480 };
