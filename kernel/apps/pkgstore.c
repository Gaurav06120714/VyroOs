#include "apps.h"
#include "../widgets.h"
#include "../theme.h"
#include "../compositor.h"
#include "../pkg.h"
#include "../notify.h"

#define TOOLBAR_H   44
#define CARD_H      72
#define SIDEBAR_W  130

static int   sel_cat   = 0;
static int   scroll    = 0;
static char  search[32] = "";
static int   search_len = 0;
static int   search_focus = 0;

static const char* cats[] = { "All", "System", "Utilities", "Network", "Games" };

static int slen(const char* s){int i=0;while(s[i])i++;return i;}
static int shas(const char* hay, const char* needle) {
    int hl = slen(hay), nl = slen(needle);
    if (nl == 0) return 1;
    for (int i = 0; i <= hl - nl; i++) {
        int ok = 1;
        for (int j = 0; j < nl; j++) {
            char a = hay[i+j], b = needle[j];
            if (a>='A'&&a<='Z') a+=32;
            if (b>='A'&&b<='Z') b+=32;
            if (a!=b){ok=0;break;}
        }
        if (ok) return 1;
    }
    return 0;
}

static void render_pkgstore(app_ctx_t* c) {
    const theme_t* t = theme();
    int abs_mx = c->mx + c->origin_x, abs_my = c->my + c->origin_y;

    uint32_t bg = t->is_dark ? 0x0D1117 : 0xF4F6FA;
    comp_rect(c->origin_x, c->origin_y, c->width, c->height, bg);

    /* ---- sidebar (categories) ---- */
    uint32_t sbg  = t->is_dark ? 0x161B22 : 0xE4E8F0;
    uint32_t sbdr = t->is_dark ? 0x252D3A : 0xC0C8D8;
    comp_rect(c->origin_x, c->origin_y, SIDEBAR_W, c->height, sbg);
    comp_rect(c->origin_x + SIDEBAR_W - 1, c->origin_y, 1, c->height, sbdr);
    w_label_color(c->origin_x + 8, c->origin_y + 10, "CATEGORIES", t->text_dim);

    int sy = c->origin_y + 30;
    for (int i = 0; i < 5; i++) {
        int is_sel = (i == sel_cat);
        uint32_t cbg2 = is_sel ? t->accent : sbg;
        int hov = (abs_mx >= c->origin_x+4 && abs_mx < c->origin_x+SIDEBAR_W-4 &&
                   abs_my >= sy && abs_my < sy+28);
        if (!is_sel && hov) cbg2 = t->is_dark ? 0x252D3A : 0xD0D8E8;
        comp_rect(c->origin_x + 4, sy, SIDEBAR_W - 8, 28, cbg2);
        comp_text(c->origin_x + 12, sy + 7, cats[i],
                  is_sel ? 0xFFFFFF : t->text, cbg2);
        if (hov && c->clicked) { sel_cat = i; scroll = 0; }
        sy += 32;
    }

    /* ---- main area ---- */
    int mx = c->origin_x + SIDEBAR_W + 8;
    int my = c->origin_y;
    int mw = c->width - SIDEBAR_W - 8;

    /* header */
    uint32_t hbg = t->is_dark ? 0x1C2230 : 0xEEF1F8;
    comp_rect(c->origin_x + SIDEBAR_W, my, mw + 8, TOOLBAR_H, hbg);
    comp_rect(c->origin_x + SIDEBAR_W, my + TOOLBAR_H - 1, mw + 8, 1, sbdr);
    w_label_color(mx, my + 12, "Vyro App Store", t->accent_hi);

    /* search bar */
    int sbx = mx + 160, sbw = mw - 170;
    uint32_t sfbg = search_focus ? (t->is_dark ? 0x1A2236 : 0xFFFFFF) : (t->is_dark ? 0x0D1117 : 0xF0F0F0);
    comp_rect(sbx, my + 10, sbw, 24, sfbg);
    comp_border(sbx, my + 10, sbw, 24, search_focus ? t->accent : sbdr);
    comp_text(sbx + 6, my + 14, search[0] ? search : "Search packages...",
              search[0] ? t->text : t->text_dim, sfbg);
    int shov = (abs_mx >= sbx && abs_mx < sbx+sbw && abs_my >= my+10 && abs_my < my+34);
    if (shov && c->clicked) search_focus = 1;
    else if (c->clicked) search_focus = 0;
    if (search_focus && c->key) {
        if (c->key == '\b' && search_len > 0) search[--search_len] = 0;
        else if (c->key >= 32 && c->key < 127 && search_len < 31) {
            search[search_len++] = (char)c->key; search[search_len] = 0;
        }
        scroll = 0;
    }

    /* ---- package list ---- */
    int list_y = my + TOOLBAR_H + 4;
    int list_h = c->height - TOOLBAR_H - 4;
    int count; package_t* repo = pkg_repo(&count);
    int visible = 0;
    int card_y  = list_y - scroll * (CARD_H + 6);

    for (int i = 0; i < count; i++) {
        /* filter by search */
        if (search_len > 0 && !shas(repo[i].name, search) && !shas(repo[i].desc, search))
            continue;
        visible++;

        if (card_y < list_y - CARD_H || card_y > list_y + list_h) {
            card_y += CARD_H + 6; continue;
        }

        /* card */
        uint32_t card_bg = t->is_dark ? 0x1C2230 : 0xFFFFFF;
        int cx2 = mx, cw2 = mw - 8;
        comp_rect(cx2, card_y, cw2, CARD_H, card_bg);
        comp_border(cx2, card_y, cw2, CARD_H, sbdr);

        /* icon placeholder */
        uint32_t icon_col = 0x58A6FF + i * 0x112233;
        comp_rect(cx2 + 8, card_y + 10, 48, 48, icon_col & 0xFFFFFF);
        comp_border(cx2 + 8, card_y + 10, 48, 48, sbdr);
        char ic[2] = { repo[i].name[0], 0 };
        comp_text(cx2 + 24, card_y + 26, ic, 0xFFFFFF, icon_col & 0xFFFFFF);

        /* text */
        comp_text(cx2 + 66, card_y + 8,  repo[i].name, t->text,     card_bg);
        /* version */
        char vb[20] = "v"; int vp = 1;
        for (int j = 0; repo[i].version[j] && vp < 18; j++) vb[vp++] = repo[i].version[j];
        vb[vp] = 0;
        comp_text(cx2 + 66 + slen(repo[i].name)*8 + 8, card_y + 8, vb, t->accent_hi, card_bg);
        comp_text(cx2 + 66, card_y + 26, repo[i].desc, t->text_dim, card_bg);

        /* deps */
        if (repo[i].deps[0]) {
            char db[32] = "Deps: "; int dp = 6;
            for (int d = 0; d < 4 && repo[i].deps[d]; d++) {
                if (d > 0 && dp < 28) { db[dp++]=','; db[dp++]=' '; }
                for (int j = 0; repo[i].deps[d][j] && dp < 30; j++) db[dp++] = repo[i].deps[d][j];
            }
            db[dp] = 0;
            comp_text(cx2 + 66, card_y + 44, db, t->text_dim, card_bg);
        }

        /* install / installed button */
        int bbx = cx2 + cw2 - 100, bby = card_y + CARD_H/2 - 14;
        if (repo[i].installed) {
            comp_rect(bbx, bby, 88, 28, t->success);
            comp_border(bbx, bby, 88, 28, 0x3FB950);
            comp_text(bbx + 8, bby + 7, "Installed", 0xFFFFFF, t->success);
        } else {
            if (w_button(bbx, bby, 88, 28, "Install", abs_mx, abs_my, c->clicked)) {
                int r2 = pkg_install(repo[i].name);
                if (r2 > 0) {
                    char msg[40] = "Installed "; int mp = 10;
                    for (int j = 0; repo[i].name[j] && mp < 38; j++) msg[mp++] = repo[i].name[j];
                    msg[mp] = 0;
                    notify_post("App Store", msg);
                }
            }
        }
        card_y += CARD_H + 6;
    }

    if (visible == 0) {
        w_label_dim(mx + mw/2 - 60, list_y + list_h/2, "No packages found");
    }

    /* scroll buttons */
    int max_scroll = visible - list_h / (CARD_H + 6);
    if (max_scroll < 0) max_scroll = 0;
    if (visible > 4) {
        if (w_button(c->origin_x + c->width - 40, list_y + 4, 32, 28, "^", abs_mx, abs_my, c->clicked))
            { if (scroll > 0) scroll--; }
        if (w_button(c->origin_x + c->width - 40, list_y + 36, 32, 28, "v", abs_mx, abs_my, c->clicked))
            { if (scroll < max_scroll) scroll++; }
    }
    (void)abs_my;
}

const app_def_t APP_PKGSTORE = { "App Store", '*', 0x80FF80, render_pkgstore, 620, 520 };
