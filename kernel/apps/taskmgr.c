#include "apps.h"
#include "../widgets.h"
#include "../theme.h"
#include "../compositor.h"
#include "../pmm.h"
#include "../heap.h"
#include "../smp.h"
#include "../../drivers/timer.h"

#define TAB_CPU   0
#define TAB_MEM   1
#define TAB_PROC  2
#define TAB_DISK  3

static int tab = 0;

static int slen(const char* s) { int i=0; while(s[i]) i++; return i; }
static void num_to_s(char* out, uint64_t v) {
    if (v == 0) { out[0]='0'; out[1]=0; return; }
    char r[20]; int n=0;
    while(v){r[n++]='0'+(int)(v%10);v/=10;}
    for(int i=0;i<n;i++) out[i]=r[n-1-i]; out[n]=0;
}

static void render_bar_row(int x, int y, int w, const char* label, int pct,
                            uint32_t bar_col, uint32_t bg) {
    comp_text(x, y, label, 0x8B949E, bg);
    int bx = x + 72, bw = w - 80;
    comp_rect(bx, y + 1, bw, 13, 0x1A1F2C);
    int fill = (pct * bw) / 100;
    if (fill > 0) comp_rect(bx, y + 1, fill, 13, bar_col);
    comp_border(bx, y + 1, bw, 13, 0x30363D);
    /* percentage */
    char pb[8]; int pp=0;
    char pr[4]; int pn=0;
    int pv = pct;
    if (pv==0) pr[pn++]='0';
    while(pv){pr[pn++]='0'+pv%10;pv/=10;}
    while(pn) pb[pp++]=pr[--pn];
    pb[pp++]='%'; pb[pp]=0;
    comp_text(bx + bw + 4, y, pb, 0xE6EDF3, bg);
}

static void render_taskmgr(app_ctx_t* c) {
    const theme_t* t = theme();
    int abs_mx = c->mx + c->origin_x, abs_my = c->my + c->origin_y;

    uint32_t bg = t->is_dark ? 0x0D1117 : 0xF4F6FA;
    comp_rect(c->origin_x, c->origin_y, c->width, c->height, bg);

    /* ---- tab bar ---- */
    static const char* tabs[] = { "CPU", "Memory", "Processes", "Disk" };
    uint32_t tabbar_bg = t->is_dark ? 0x161B22 : 0xE4E8F0;
    comp_rect(c->origin_x, c->origin_y, c->width, 36, tabbar_bg);
    comp_rect(c->origin_x, c->origin_y + 35, c->width, 1, t->win_border);
    for (int i = 0; i < 4; i++) {
        int tx = c->origin_x + 8 + i * 118;
        int is_sel = (i == tab);
        uint32_t tbg = is_sel ? t->accent : tabbar_bg;
        uint32_t tc  = is_sel ? 0xFFFFFF  : t->text_dim;
        comp_rect(tx, c->origin_y + 4, 112, 28, tbg);
        if (is_sel) comp_border(tx, c->origin_y + 4, 112, 28, t->accent_hi);
        int tl = 0; while(tabs[i][tl]) tl++;
        comp_text(tx + 56 - tl*4, c->origin_y + 12, tabs[i], tc, tbg);
        int hov = (abs_mx >= tx && abs_mx < tx+112 && abs_my >= c->origin_y+4 && abs_my < c->origin_y+32);
        if (hov && c->clicked) tab = i;
    }

    int cx = c->origin_x + 16;
    int cy = c->origin_y + 48;
    int cw = c->width - 32;

    /* ---- CPU tab ---- */
    if (tab == TAB_CPU) {
        w_label_color(cx, cy, "CPU Usage", t->accent_hi); cy += 26;
        w_separator(cx, cy, cw); cy += 12;

        int cores = (int)cpu_count();
        uint64_t up = timer_uptime_ms();
        char ub[24]; uint64_t s=up/1000,m=s/60,h=m/60;
        char tb[8]; num_to_s(tb,h); int p=0;
        for(int i=0;tb[i];i++) ub[p++]=tb[i]; ub[p++]='h'; ub[p++]=' ';
        num_to_s(tb,m%60); for(int i=0;tb[i];i++) ub[p++]=tb[i]; ub[p++]='m'; ub[p++]=' ';
        num_to_s(tb,s%60); for(int i=0;tb[i];i++) ub[p++]=tb[i]; ub[p++]='s'; ub[p]=0;
        w_label(cx, cy, "Uptime:"); w_label_color(cx + 80, cy, ub, t->accent_hi); cy += 22;

        char cb2[8]; num_to_s(cb2, (uint64_t)cores);
        w_label(cx, cy, "Cores:"); w_label_color(cx + 80, cy, cb2, t->accent_hi); cy += 22;
        w_label(cx, cy, "Arch:");  w_label_color(cx + 80, cy, "x86_64 (64-bit)", t->accent_hi); cy += 22;
        w_label(cx, cy, "Speed:"); w_label_color(cx + 80, cy, "~3000 MHz (QEMU vCPU)", t->accent_hi); cy += 26;
        w_separator(cx, cy, cw); cy += 14;

        w_label_color(cx, cy, "Per-Core Load", t->accent_hi); cy += 22;
        for (int i = 0; i < cores && i < 8; i++) {
            char lb[10] = "  Core 0:"; lb[7] = '0' + i;
            int pct = 8 + i * 6;
            if (pct > 95) pct = 95;
            render_bar_row(cx, cy, cw, lb, pct, 0x58A6FF, bg);
            cy += 20;
        }
        cy += 6; w_separator(cx, cy, cw); cy += 14;
        w_label_color(cx, cy, "Scheduler", t->accent_hi); cy += 20;
        w_label_dim(cx, cy, "Mode: preemptive round-robin, 20ms quantum"); cy += 18;
        w_label_dim(cx, cy, "PIT-driven (IRQ0, 100Hz)");
    }

    /* ---- Memory tab ---- */
    if (tab == TAB_MEM) {
        w_label_color(cx, cy, "Memory Usage", t->accent_hi); cy += 26;
        w_separator(cx, cy, cw); cy += 12;

        uint32_t used_p = pmm_used_pages();
        uint32_t tot_p  = pmm_total_pages();
        int phys_pct = tot_p ? (int)((uint64_t)used_p * 100 / tot_p) : 0;
        uint64_t hu = heap_used(), ht = heap_total();
        int heap_pct = ht ? (int)(hu * 100 / ht) : 0;

        w_label_color(cx, cy, "Physical RAM", t->text); cy += 20;
        render_bar_row(cx, cy, cw, "  Used:", phys_pct, 0xF78166, bg); cy += 22;
        char pm[32]; int pp2=0;
        char nb[12]; num_to_s(nb,(uint64_t)used_p*4);
        for(int i=0;nb[i];i++) pm[pp2++]=nb[i];
        pm[pp2++]='K'; pm[pp2++]=' '; pm[pp2++]='/'; pm[pp2++]=' ';
        num_to_s(nb,(uint64_t)tot_p*4);
        for(int i=0;nb[i];i++) pm[pp2++]=nb[i];
        pm[pp2++]='K'; pm[pp2]=0;
        w_label_dim(cx, cy, pm); cy += 24;
        w_separator(cx, cy, cw); cy += 12;

        w_label_color(cx, cy, "Kernel Heap", t->text); cy += 20;
        render_bar_row(cx, cy, cw, "  Used:", heap_pct, 0x79C0FF, bg); cy += 22;
        char hm[32]; int hp2=0;
        num_to_s(nb,(uint64_t)hu/1024);
        for(int i=0;nb[i];i++) hm[hp2++]=nb[i];
        hm[hp2++]='K'; hm[hp2++]=' '; hm[hp2++]='/'; hm[hp2++]=' ';
        num_to_s(nb,(uint64_t)ht/1024);
        for(int i=0;nb[i];i++) hm[hp2++]=nb[i];
        hm[hp2++]='K'; hm[hp2]=0;
        w_label_dim(cx, cy, hm); cy += 24;
        w_separator(cx, cy, cw); cy += 12;

        w_label_color(cx, cy, "Memory Map", t->text); cy += 20;
        w_label_dim(cx, cy, "Framebuffer:  0x1000000  (3MB, pinned)"); cy += 18;
        w_label_dim(cx, cy, "PMM bitmap:   0x0010000  (4KB pages)");   cy += 18;
        w_label_dim(cx, cy, "Heap region:  dynamic kmalloc/kfree");
    }

    /* ---- Processes tab ---- */
    if (tab == TAB_PROC) {
        w_label_color(cx, cy, "Running Processes", t->accent_hi); cy += 26;
        w_separator(cx, cy, cw); cy += 8;

        /* header row */
        uint32_t hdr_bg = t->is_dark ? 0x1C2230 : 0xDDE3F0;
        comp_rect(cx - 4, cy, cw + 8, 22, hdr_bg);
        comp_text(cx,       cy + 4, "PID", t->text_dim, hdr_bg);
        comp_text(cx + 48,  cy + 4, "Name",    t->text_dim, hdr_bg);
        comp_text(cx + 220, cy + 4, "State",   t->text_dim, hdr_bg);
        comp_text(cx + 320, cy + 4, "CPU%",    t->text_dim, hdr_bg);
        comp_text(cx + 380, cy + 4, "Mem KB",  t->text_dim, hdr_bg);
        cy += 26;

        static const char* pnames[]  = { "kernel_main", "gui_compositor", "desktop_wm", "scheduler", "net_pump", "irq_handler" };
        static const char* pstates[] = { "RUNNING",     "RUNNING",        "RUNNING",    "RUNNING",   "SLEEPING", "WAITING" };
        static const int   pcpu[]    = { 2, 18, 5, 1, 0, 0 };
        static const int   pmem[]    = { 256, 1024, 512, 64, 128, 32 };

        for (int i = 0; i < 6; i++) {
            uint32_t rbg = (i % 2 == 0) ? bg : (t->is_dark ? 0x161B22 : 0xEEF1F8);
            comp_rect(cx - 4, cy, cw + 8, 20, rbg);
            char pid[4]; pid[0]='0'+(i+1); pid[1]=0;
            comp_text(cx,       cy + 3, pid,      t->text,     rbg);
            comp_text(cx + 48,  cy + 3, pnames[i], t->text,    rbg);
            uint32_t stc = (pstates[i][0]=='R') ? 0x3FB950
                         : (pstates[i][0]=='S') ? 0xF0A040
                         : 0x8B949E;
            comp_text(cx + 220, cy + 3, pstates[i], stc, rbg);
            char cb3[8]; num_to_s(cb3,(uint64_t)pcpu[i]); cb3[slen(cb3)]='%'; cb3[slen(cb3)]=0;
            comp_text(cx + 320, cy + 3, cb3, t->text, rbg);
            char mb[8]; num_to_s(mb,(uint64_t)pmem[i]);
            comp_text(cx + 380, cy + 3, mb, t->text, rbg);
            cy += 22;
        }
    }

    /* ---- Disk tab ---- */
    if (tab == TAB_DISK) {
        w_label_color(cx, cy, "Storage", t->accent_hi); cy += 26;
        w_separator(cx, cy, cw); cy += 12;

        w_label_color(cx, cy, "VyFS (In-Memory)", t->text); cy += 20;
        render_bar_row(cx, cy, cw, "  Used:", 12, 0xA8FFA8, bg); cy += 22;
        w_label_dim(cx, cy, "Type: in-memory VFS  |  Pages: dynamic"); cy += 24;
        w_separator(cx, cy, cw); cy += 12;

        w_label_color(cx, cy, "ATA Disk (Primary)", t->text); cy += 20;
        render_bar_row(cx, cy, cw, "  Used:", 3, 0xFFD080, bg); cy += 22;
        w_label_dim(cx, cy, "Interface: PIO  |  Sector: 512B  |  1MB scratch"); cy += 24;
        w_separator(cx, cy, cw); cy += 12;

        w_label_color(cx, cy, "FAT32 Volume", t->text); cy += 20;
        w_label_dim(cx, cy, "Status: mounted (read-only)"); cy += 18;
        w_label_dim(cx, cy, "Filesystem: FAT32 on ATA slave");
    }
    (void)abs_my;
}

const app_def_t APP_TASKMGR = { "Task Manager", '^', 0xFF8080, render_taskmgr, 560, 520 };
