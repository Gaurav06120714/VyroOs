#include "apps.h"
#include "../widgets.h"
#include "../theme.h"
#include "../compositor.h"
#include "../vfs.h"

static vfs_node_t* cwd_node = 0;

static void breadcrumb(int x, int y, vfs_node_t* node) {
    if (!node) return;
    char path[256];
    vfs_full_path(node, path, sizeof(path));
    w_label_color(x, y, path, theme()->accent_hi);
}

static int s_cmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; } return *a - *b;
}

static void render_files(app_ctx_t* c) {
    if (!cwd_node) cwd_node = vfs_root();
    const theme_t* t = theme();

    // Toolbar
    comp_rect(c->origin_x, c->origin_y, c->width, 36, t->win_title);
    int abs_mx = c->mx + c->origin_x, abs_my = c->my + c->origin_y;

    if (w_button(c->origin_x + 6, c->origin_y + 6, 64, 24, "Up",
                 abs_mx, abs_my, c->clicked))
        if (cwd_node->parent) cwd_node = cwd_node->parent;
    if (w_button(c->origin_x + 76, c->origin_y + 6, 64, 24, "Home",
                 abs_mx, abs_my, c->clicked))
        cwd_node = vfs_root();
    breadcrumb(c->origin_x + 156, c->origin_y + 12, cwd_node);

    // File list
    comp_rect(c->origin_x, c->origin_y + 36, c->width, c->height - 36, t->win_body);
    int y = c->origin_y + 44;
    vfs_node_t* item = cwd_node->first_child;
    while (item && y < c->origin_y + c->height - 28) {
        char buf[80];
        int p = 0;
        if (item->type == VFS_DIRECTORY) { buf[p++] = '['; }
        for (int i = 0; item->name[i] && p < 70; i++) buf[p++] = item->name[i];
        if (item->type == VFS_DIRECTORY) { buf[p++] = ']'; }
        buf[p] = 0;

        if (w_list_item(c->origin_x + 8, y, c->width - 16, 24, buf,
                         0, abs_mx, abs_my, c->clicked)) {
            if (item->type == VFS_DIRECTORY) cwd_node = item;
        }
        y += 26;
        item = item->next_sibling;
    }

    // Status bar
    int sy = c->origin_y + c->height - 22;
    comp_rect(c->origin_x, sy, c->width, 22, t->dock_bg);
    int n = 0; for (vfs_node_t* x = cwd_node->first_child; x; x = x->next_sibling) n++;
    char st[40] = "  items: ";
    if (n >= 100) { st[8] = '0' + (n/100); st[9] = '0' + ((n/10)%10); st[10] = '0' + (n%10); st[11]=0; }
    else if (n >= 10) { st[8] = '0' + (n/10); st[9] = '0' + (n%10); st[10]=0; }
    else { st[8] = '0' + n; st[9] = 0; }
    w_label_dim(c->origin_x + 4, sy + 4, st);

    (void)s_cmp;
}

const app_def_t APP_FILES = { "Files", 'F', 0x50A0FF, render_files, 560, 420 };
