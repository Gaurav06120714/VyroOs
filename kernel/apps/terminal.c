#include "apps.h"
#include "../widgets.h"
#include "../theme.h"
#include "../compositor.h"

#define MAX_LINES   16
#define MAX_LINELEN 60

static char  buffer[MAX_LINES][MAX_LINELEN + 1];
static int   line_count = 0;
static char  input_buf[MAX_LINELEN + 1] = "";
static int   input_len = 0;
static int   initted = 0;

static void scpy(char* d, const char* s, int max) {
    int i = 0; while (s[i] && i < max - 1) { d[i] = s[i]; i++; } d[i] = '\0';
}

static void push_line(const char* s) {
    if (line_count >= MAX_LINES) {
        for (int i = 1; i < MAX_LINES; i++)
            scpy(buffer[i-1], buffer[i], MAX_LINELEN + 1);
        line_count = MAX_LINES - 1;
    }
    scpy(buffer[line_count++], s, MAX_LINELEN + 1);
}

static int s_eq(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; } return *a == *b;
}

static void run_command(const char* cmd) {
    if (!cmd[0]) return;
    char echo[MAX_LINELEN + 4] = "$ ";
    int i = 0; while (cmd[i] && i < MAX_LINELEN) { echo[2+i] = cmd[i]; i++; }
    echo[2+i] = 0;
    push_line(echo);
    if (s_eq(cmd, "help"))      push_line("commands: help clear date hello version");
    else if (s_eq(cmd, "clear")) line_count = 0;
    else if (s_eq(cmd, "date"))  push_line("Vyro OS - terminal app");
    else if (s_eq(cmd, "hello")) push_line("Hello from Vyro OS terminal!");
    else if (s_eq(cmd, "version")) push_line("Vyro OS v2.0.0");
    else push_line("unknown command");
}

static void render_terminal(app_ctx_t* c) {
    if (!initted) {
        push_line("Vyro OS Terminal v2.0");
        push_line("Type 'help' for commands.");
        initted = 1;
    }
    const theme_t* t = theme();

    comp_rect(c->origin_x, c->origin_y, c->width, c->height, 0x0F1018);

    for (int i = 0; i < line_count; i++) {
        comp_text_bg_alpha(c->origin_x + 8, c->origin_y + 8 + i * 18,
                           buffer[i], 0xA0FF90);
    }

    int iy = c->origin_y + 8 + line_count * 18;
    if (iy < c->origin_y + c->height - 22) {
        comp_text_bg_alpha(c->origin_x + 8, iy, "> ", t->accent_hi);
        comp_text_bg_alpha(c->origin_x + 24, iy, input_buf, 0xFFFFFF);

        int cx = c->origin_x + 24 + input_len * 8;
        comp_rect(cx, iy, 8, 16, 0xFFFFFF);
    }

    if (c->key) {
        if (c->key == '\n') { run_command(input_buf); input_len = 0; input_buf[0] = 0; }
        else if (c->key == '\b') { if (input_len > 0) input_buf[--input_len] = 0; }
        else if (c->key >= 32 && c->key < 127 && input_len < MAX_LINELEN) {
            input_buf[input_len++] = (char)c->key; input_buf[input_len] = 0;
        }
    }
}

const app_def_t APP_TERMINAL = { "Terminal", '>', 0x40FF60, render_terminal, 600, 400 };
