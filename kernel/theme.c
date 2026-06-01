#include "theme.h"

#define RGB(r,g,b) (((uint32_t)(r)<<16)|((uint32_t)(g)<<8)|(b))

static theme_t cur;

static void load_dark() {
    cur.desktop_bg       = RGB( 22,  28,  44);
    cur.desktop_bg2      = RGB( 12,  16,  28);
    cur.taskbar_bg       = RGB( 18,  20,  30);
    cur.dock_bg          = RGB( 34,  38,  56);
    cur.dock_border      = RGB( 70,  80, 110);
    cur.win_title        = RGB( 50,  55,  78);
    cur.win_title_focus  = RGB( 90, 120, 220);
    cur.win_body         = RGB( 30,  34,  48);
    cur.win_border       = RGB( 70,  80, 110);
    cur.win_shadow       = RGB(  0,   0,   0);
    cur.text             = RGB(235, 238, 245);
    cur.text_dim         = RGB(150, 160, 180);
    cur.text_invert      = RGB( 20,  22,  30);
    cur.accent           = RGB( 90, 150, 255);
    cur.accent_hi        = RGB(130, 180, 255);
    cur.danger           = RGB(230,  80,  80);
    cur.success          = RGB( 90, 200, 120);
    cur.is_dark          = 1;
}

static void load_light() {
    cur.desktop_bg       = RGB(225, 232, 245);
    cur.desktop_bg2      = RGB(200, 212, 230);
    cur.taskbar_bg       = RGB(245, 247, 252);
    cur.dock_bg          = RGB(255, 255, 255);
    cur.dock_border      = RGB(200, 210, 225);
    cur.win_title        = RGB(220, 225, 235);
    cur.win_title_focus  = RGB(110, 160, 240);
    cur.win_body         = RGB(248, 250, 253);
    cur.win_border       = RGB(180, 190, 210);
    cur.win_shadow       = RGB( 80,  80,  90);
    cur.text             = RGB( 20,  22,  30);
    cur.text_dim         = RGB(110, 120, 140);
    cur.text_invert      = RGB(245, 247, 252);
    cur.accent           = RGB( 50, 120, 230);
    cur.accent_hi        = RGB( 90, 160, 255);
    cur.danger           = RGB(220,  70,  70);
    cur.success          = RGB( 60, 175,  90);
    cur.is_dark          = 0;
}

void theme_init() { load_dark(); }

void theme_set_dark(int dark) {
    if (dark) load_dark(); else load_light();
}

const theme_t* theme() { return &cur; }
