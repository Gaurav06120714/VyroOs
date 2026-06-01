#include "gui.h"
#include "../drivers/framebuffer.h"
#include "../drivers/mouse.h"
#include "../drivers/keyboard.h"
#include "../drivers/timer.h"
#include "../include/types.h"

#define DESKTOP_BG   FB_COLOR(30, 40, 70)     // dark blue
#define TASKBAR_BG   FB_COLOR(20, 20, 30)
#define WIN_TITLE    FB_COLOR(70, 90, 160)
#define WIN_BODY     FB_COLOR(235, 235, 240)
#define WIN_TEXT     FB_COLOR(20, 20, 30)

#define CURSOR_W 12
#define CURSOR_H 18

// Saved pixels under the cursor (so we can restore on move)
static uint32_t cursor_save[CURSOR_W * CURSOR_H];

// ─────────────────────────────────────────────────
// Draw a window with a title bar
// ─────────────────────────────────────────────────
static void draw_window(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                        const char* title, const char* body) {
    fb_fill_rect(x, y, w, 22, WIN_TITLE);             // title bar
    fb_fill_rect(x, y + 22, w, h - 22, WIN_BODY);     // body
    fb_draw_rect(x, y, w, h, FB_BLACK);               // border
    fb_draw_text(x + 6, y + 4, title, FB_WHITE, WIN_TITLE);
    // close button
    fb_fill_rect(x + w - 18, y + 4, 14, 14, FB_RED);
    fb_draw_text(x + w - 15, y + 4, "x", FB_WHITE, FB_RED);
    // body text
    fb_draw_text(x + 8, y + 32, body, WIN_TEXT, WIN_BODY);
}

// ─────────────────────────────────────────────────
// Draw the full desktop scene
// ─────────────────────────────────────────────────
static void draw_desktop() {
    fb_clear(DESKTOP_BG);

    // Top bar
    fb_fill_rect(0, 0, FB_WIDTH, 24, TASKBAR_BG);
    fb_draw_text(8, 4, "Vyro OS  Desktop", FB_CYAN, TASKBAR_BG);
    fb_draw_text(FB_WIDTH - 200, 4, "Press ESC to exit GUI", FB_GREY, TASKBAR_BG);

    // Windows
    draw_window(120, 120, 360, 200, "Welcome",
                "Vyro OS graphical desktop - Phase 20");
    draw_window(540, 200, 360, 220, "About",
                "64-bit OS built from scratch. $0.");

    // Bottom taskbar
    fb_fill_rect(0, FB_HEIGHT - 28, FB_WIDTH, 28, TASKBAR_BG);
    fb_fill_rect(6, FB_HEIGHT - 24, 80, 20, WIN_TITLE);
    fb_draw_text(16, FB_HEIGHT - 22, "Start", FB_WHITE, WIN_TITLE);
}

// ─────────────────────────────────────────────────
// Cursor: save the area under it, draw an arrow, restore
// ─────────────────────────────────────────────────
static const char* cursor_shape[CURSOR_H] = {
    "X           ",
    "XX          ",
    "XwX         ",
    "XwwX        ",
    "XwwwX       ",
    "XwwwwX      ",
    "XwwwwwX     ",
    "XwwwwwwX    ",
    "XwwwwwwwX   ",
    "XwwwwwwwwX  ",
    "XwwwwwXXXXX ",
    "XwwXwwX     ",
    "XwX XwwX    ",
    "XX  XwwX    ",
    "X    XwwX   ",
    "     XwwX   ",
    "      XX    ",
    "            ",
};

static void cursor_save_under(int mx, int my) {
    for (int j = 0; j < CURSOR_H; j++)
        for (int i = 0; i < CURSOR_W; i++)
            cursor_save[j * CURSOR_W + i] = fb_get_pixel(mx + i, my + j);
}

static void cursor_restore(int mx, int my) {
    for (int j = 0; j < CURSOR_H; j++)
        for (int i = 0; i < CURSOR_W; i++)
            fb_putpixel(mx + i, my + j, cursor_save[j * CURSOR_W + i]);
}

static void cursor_draw(int mx, int my) {
    for (int j = 0; j < CURSOR_H; j++) {
        for (int i = 0; i < CURSOR_W; i++) {
            char c = cursor_shape[j][i];
            if (c == 'X') fb_putpixel(mx + i, my + j, FB_BLACK);
            else if (c == 'w') fb_putpixel(mx + i, my + j, FB_WHITE);
        }
    }
}

// ─────────────────────────────────────────────────
// gui_run: the desktop event loop
// ─────────────────────────────────────────────────
void gui_run() {
    if (!fb_available()) return;

    draw_desktop();

    int last_x = -1, last_y = -1;

    while (1) {
        // Exit on ESC
        if (keyboard_has_input()) {
            char c = keyboard_getchar();
            if (c == 0x1B) break;   // ESC
        }

        int mx = mouse_x();
        int my = mouse_y();

        if (mx != last_x || my != last_y) {
            // Restore old cursor area
            if (last_x >= 0) cursor_restore(last_x, last_y);
            // Save + draw at new position
            cursor_save_under(mx, my);
            cursor_draw(mx, my);
            last_x = mx;
            last_y = my;
        }

        // Small delay to reduce flicker / CPU
        sleep_ms(8);
    }
}
