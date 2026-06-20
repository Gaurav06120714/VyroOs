#include "apps.h"
#include "../widgets.h"
#include "../theme.h"
#include "../compositor.h"
#include "../vfs.h"
#include "../fat32.h"

#define SIDEBAR_W   140
#define TOOLBAR_H    36
#define STATUSBAR_H  22
#define ROW_H        26

static vfs_node_t* cwd_node  = 0;
static int         show_fat  = 0;
static int         selected  = -1;
static int         sel_click = 0;

static int slen(const char* s) { int i=0; while(s[i]) i++; return i; }

static void draw_sidebar(app_ctx_t* c) {
    const theme_t* t = theme();
    int sx = c->origin_x;
    int sy = c->origin_y;
    int sh = c->height;
    uint32_t sbg = t->is_dark ? 0x161B22 : 0xE4E8F0;
    uint32_t sbdr = t->is_dark ? 0x252D3A : 0xC0C8D8;

    comp_rect(sx, sy, SIDEBAR_W, sh, sbg);
    comp_rect(sx + SIDEBAR_W - 1, sy, 1, sh, sbdr);

    int ay = sy + 12;
    comp_text(sx + 10, ay, "FAVOURITES", t->text_dim, sbg); ay += 22;

    struct { const char* label; int is_fat; } items[] = {
        { "  Home",       0 },
        { "  Root  /",    0 },
        { "  FAT32",      1 },
    };
    int abs_mx = c->mx + c->origin_x, abs_my = c->my + c->origin_y;
    for (int i = 0; i < 3; i++) {
        int hover = (abs_mx >= sx + 4 && abs_mx < sx + SIDEBAR_W - 4 &&
                     abs_my >= ay && abs_my < ay + 24);
        uint32_t ibg = hover ? t->accent : sbg;
        comp_rect(sx + 4, ay, SIDEBAR_W - 8, 24, ibg);
        comp_text(sx + 8, ay + 5, items[i].label,
                  hover ? 0xFFFFFF : t->text, ibg);
        if (hover && c->clicked) {
            show_fat = items[i].is_fat;
            if (!items[i].is_fat) cwd_node = vfs_root();
            selected = -1;
        }
        ay += 28;
    }

    ay += 8;
    comp_text(sx + 10, ay, "DRIVES", t->text_dim, sbg); ay += 22;
    int hover2 = (abs_mx >= sx+4 && abs_mx < sx+SIDEBAR_W-4 && abs_my >= ay && abs_my < ay+24);
    uint32_t ibg2 = hover2 ? t->accent : sbg;
    comp_rect(sx + 4, ay, SIDEBAR_W - 8, 24, ibg2);
    comp_text(sx + 8, ay + 5, "  VyFS (mem)", hover2 ? 0xFFFFFF : t->text, ibg2);
    ay += 28;

    if (fat32_is_mounted()) {
        int hover3 = (abs_mx >= sx+4 && abs_mx < sx+SIDEBAR_W-4 && abs_my >= ay && abs_my < ay+24);
        uint32_t ibg3 = hover3 ? t->accent : sbg;
        comp_rect(sx + 4, ay, SIDEBAR_W - 8, 24, ibg3);
        comp_text(sx + 8, ay + 5, "  FAT32 disk", hover3 ? 0xFFFFFF : t->text, ibg3);
        if (hover3 && c->clicked) { show_fat = 1; selected = -1; }
    }
}

static void draw_toolbar(app_ctx_t* c) {
    const theme_t* t = theme();
    int tx = c->origin_x + SIDEBAR_W;
    int ty = c->origin_y;
    int tw = c->width - SIDEBAR_W;
    uint32_t tbg = t->is_dark ? 0x1C2230 : 0xEEF1F8;
    uint32_t tbdr = t->is_dark ? 0x252D3A : 0xC8D0DC;

    comp_rect(tx, ty, tw, TOOLBAR_H, tbg);
    comp_rect(tx, ty + TOOLBAR_H - 1, tw, 1, tbdr);

    int abs_mx = c->mx + c->origin_x, abs_my = c->my + c->origin_y;

    if (w_button(tx + 6, ty + 6, 48, 24, "< Up", abs_mx, abs_my, c->clicked)) {
        if (!show_fat && cwd_node && cwd_node->parent) {
            cwd_node = cwd_node->parent; selected = -1;
        }
    }
    if (w_button(tx + 60, ty + 6, 52, 24, "Home", abs_mx, abs_my, c->clicked)) {
        cwd_node = vfs_root(); show_fat = 0; selected = -1;
    }

    char path[128];
    if (show_fat) {
        path[0]='F'; path[1]='A'; path[2]='T'; path[3]='3'; path[4]='2'; path[5]=':'; path[6]='/'; path[7]=0;
    } else {
        if (cwd_node) vfs_full_path(cwd_node, path, sizeof(path));
        else { path[0]='/'; path[1]=0; }
    }
    comp_rect(tx + 118, ty + 7, tw - 126, 20, t->is_dark ? 0x0D1117 : 0xFFFFFF);
    comp_border(tx + 118, ty + 7, tw - 126, 20, tbdr);
    comp_text(tx + 124, ty + 10, path, t->accent_hi, t->is_dark ? 0x0D1117 : 0xFFFFFF);
}

static void draw_filelist(app_ctx_t* c) {
    const theme_t* t = theme();
    int lx = c->origin_x + SIDEBAR_W;
    int ly = c->origin_y + TOOLBAR_H;
    int lw = c->width - SIDEBAR_W;
    int lh = c->height - TOOLBAR_H - STATUSBAR_H;
    int abs_mx = c->mx + c->origin_x, abs_my = c->my + c->origin_y;

    comp_rect(lx, ly, lw, lh, t->win_body);

    uint32_t hdr_bg = t->is_dark ? 0x1C2230 : 0xE4E8F0;
    uint32_t hdr_bdr = t->is_dark ? 0x252D3A : 0xC0C8D8;
    comp_rect(lx, ly, lw, 22, hdr_bg);
    comp_rect(lx, ly + 21, lw, 1, hdr_bdr);
    comp_text(lx + 10,  ly + 4, "Name",         t->text_dim, hdr_bg);
    comp_text(lx + 280, ly + 4, "Type",         t->text_dim, hdr_bg);
    comp_text(lx + 360, ly + 4, "Size",         t->text_dim, hdr_bg);
    comp_rect(lx + 270, ly, 1, 22, hdr_bdr);
    comp_rect(lx + 350, ly, 1, 22, hdr_bdr);

    int y = ly + 24;
    int item_count = 0;

    if (show_fat) {
        fat32_dirent_t es[32];
        int n = fat32_list_root(es, 32);
        for (int i = 0; i < n && y < ly + lh - 4; i++) {
            int is_dir = (es[i].attr & 0x10) != 0;
            int hover  = (abs_mx >= lx && abs_mx < lx+lw && abs_my >= y && abs_my < y+ROW_H);
            int sel    = (selected == i);
            uint32_t rbg = sel    ? t->accent :
                           hover  ? (t->is_dark ? 0x252D3A : 0xDDE3F0) : t->win_body;
            uint32_t rtx = sel ? 0xFFFFFF : t->text;
            comp_rect(lx, y, lw, ROW_H, rbg);
            comp_text(lx + 28, y + 6, is_dir ? "[D]" : "[F]",
                      is_dir ? t->accent_hi : t->text_dim, rbg);
            comp_text(lx + 56, y + 6, es[i].name, rtx, rbg);
            comp_text(lx + 280, y + 6, is_dir ? "Folder" : "File", t->text_dim, rbg);
            if (!is_dir) {
                char sz[16]; int sv = es[i].size; int si=0;
                if (sv == 0) { sz[0]='0'; sz[1]='B'; sz[2]=0; }
                else {
                    char tmp[10]; int tn=0;
                    while(sv) { tmp[tn++]='0'+(sv%10); sv/=10; }
                    while(tn) sz[si++]=tmp[--tn]; sz[si++]='B'; sz[si]=0;
                }
                comp_text(lx + 360, y + 6, sz, t->text_dim, rbg);
            }
            if (hover && c->clicked) { selected = i; sel_click = 1; }
            y += ROW_H;
            item_count++;
        }
    } else {
        if (!cwd_node) cwd_node = vfs_root();
        vfs_node_t* item = cwd_node->first_child;
        int idx = 0;
        while (item && y < ly + lh - 4) {
            int is_dir = (item->type == VFS_DIRECTORY);
            int hover  = (abs_mx >= lx && abs_mx < lx+lw && abs_my >= y && abs_my < y+ROW_H);
            int sel    = (selected == idx);
            uint32_t rbg = sel   ? t->accent :
                           hover ? (t->is_dark ? 0x252D3A : 0xDDE3F0) : t->win_body;
            uint32_t rtx = sel ? 0xFFFFFF : t->text;
            comp_rect(lx, y, lw, ROW_H, rbg);
            uint32_t icon_col = is_dir ? 0xF0A040 : 0x80C0FF;
            comp_rect(lx + 10, y + 5, 14, 14, icon_col);
            comp_border(lx + 10, y + 5, 14, 14, t->win_border);
            comp_text(lx + 30, y + 6, item->name, rtx, rbg);
            comp_text(lx + 280, y + 6, is_dir ? "Folder" : "File", t->text_dim, rbg);
            if (!is_dir && item->size > 0) {
                char sz[16]; uint32_t sv = item->size; int si=0;
                char tmp[10]; int tn=0;
                while(sv) { tmp[tn++]='0'+(sv%10); sv/=10; }
                while(tn) sz[si++]=tmp[--tn]; sz[si++]='B'; sz[si]=0;
                comp_text(lx + 360, y + 6, sz, t->text_dim, rbg);
            }
            if (hover && c->clicked) {
                if (is_dir) { cwd_node = item; selected = -1; }
                else selected = idx;
            }
            y += ROW_H;
            item = item->next_sibling;
            idx++;
            item_count++;
        }
    }

    int sby = c->origin_y + c->height - STATUSBAR_H;
    uint32_t sbg2 = t->is_dark ? 0x161B22 : 0xE4E8F0;
    comp_rect(c->origin_x + SIDEBAR_W, sby, c->width - SIDEBAR_W, STATUSBAR_H, sbg2);
    comp_rect(c->origin_x + SIDEBAR_W, sby, c->width - SIDEBAR_W, 1, t->is_dark ? 0x252D3A : 0xC0C8D8);
    char st[40];
    int si2 = 0;
    st[si2++]=' ';
    char tmp2[8]; int tv=item_count; int tn2=0;
    if(tv==0){tmp2[0]='0';tn2=1;}
    else { char r[8]; int rn=0; while(tv){r[rn++]='0'+(tv%10);tv/=10;} while(rn)tmp2[tn2++]=r[--rn]; }
    for(int j=0;j<tn2;j++) st[si2++]=tmp2[j];
    const char* itm = " items";
    for(int j=0;itm[j];j++) st[si2++]=itm[j];
    st[si2]=0;
    comp_text(lx + 8, sby + 4, st, t->text_dim, sbg2);
    (void)sel_click; (void)slen;
}

static void render_files(app_ctx_t* c) {
    if (!cwd_node) cwd_node = vfs_root();
    draw_sidebar(c);
    draw_toolbar(c);
    draw_filelist(c);
}

const app_def_t APP_FILES = { "Files", 'F', 0x50A0FF, render_files, 640, 460 };
