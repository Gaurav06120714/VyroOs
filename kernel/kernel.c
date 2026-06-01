#include "../drivers/screen.h"
#include "../drivers/pic.h"
#include "../drivers/keyboard.h"
#include "../include/types.h"
#include "idt.h"
#include "isr.h"

// ─────────────────────────────────────────────────
// Simple string compare
// ─────────────────────────────────────────────────
static int kstrcmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a - *b;
}

// ─────────────────────────────────────────────────
// Print boot banner
// ─────────────────────────────────────────────────
static void print_banner() {
    screen_set_color(CYAN_ON_BLACK);
    print("  +------------------------------------------+\n");
    print("  |  VYRO OS  v0.5.0   64-bit   x86_64       |\n");
    print("  |  MIT License       $0 Budget              |\n");
    print("  +------------------------------------------+\n");
    screen_set_color(WHITE_ON_BLACK);
    print_char('\n');
}

static void ok(const char* msg) {
    print_color("  [OK] ", MAKE_COLOR(COLOR_LIGHT_GREEN, COLOR_BLACK));
    print(msg);
    print_char('\n');
}

static void pending(const char* msg) {
    print_color("  [  ] ", MAKE_COLOR(COLOR_DARK_GREY, COLOR_BLACK));
    print_color(msg, MAKE_COLOR(COLOR_LIGHT_GREY, COLOR_BLACK));
    print_char('\n');
}

// ─────────────────────────────────────────────────
// Shell: process a command string
// ─────────────────────────────────────────────────
static void shell_run(const char* cmd) {
    if (kstrcmp(cmd, "help") == 0) {
        print_color("\n  Commands:\n", YELLOW_ON_BLACK);
        print("    help    — show this menu\n");
        print("    clear   — clear the screen\n");
        print("    about   — about Vyro OS\n");
        print("    hello   — say hello\n");
        print_char('\n');

    } else if (kstrcmp(cmd, "clear") == 0) {
        screen_clear(WHITE_ON_BLACK);
        print_banner();

    } else if (kstrcmp(cmd, "about") == 0) {
        print_color("\n  Vyro OS\n", CYAN_ON_BLACK);
        print("  A 64-bit OS built from scratch.\n");
        print("  Language:  NASM Assembly + C\n");
        print("  Arch:      x86_64\n");
        print("  License:   MIT\n");
        print("  Budget:    $0\n\n");

    } else if (kstrcmp(cmd, "hello") == 0) {
        print_color("\n  Hello from Vyro OS!\n\n", MAKE_COLOR(COLOR_LIGHT_GREEN, COLOR_BLACK));

    } else if (cmd[0] != '\0') {
        print_color("\n  Unknown command: ", MAKE_COLOR(COLOR_LIGHT_RED, COLOR_BLACK));
        print(cmd);
        print_color("  (type 'help' for commands)\n\n", MAKE_COLOR(COLOR_LIGHT_GREY, COLOR_BLACK));
    }
}

// ─────────────────────────────────────────────────
// Shell prompt
// ─────────────────────────────────────────────────
static void print_prompt() {
    print_color("vyro> ", MAKE_COLOR(COLOR_LIGHT_GREEN, COLOR_BLACK));
}

// ─────────────────────────────────────────────────
// kernel_main: OS entry point. Never returns.
// ─────────────────────────────────────────────────
void kernel_main() {
    screen_init();
    print_banner();

    // Initialize interrupt system
    idt_init();
    pic_init();

    print_color("  Boot Sequence:\n", WHITE_ON_BLACK);
    ok("Bootloader (CHS disk read)");
    ok("64-bit Long Mode");
    ok("Kernel at 0x10000");
    ok("Screen driver");
    ok("IDT (256 vectors)");
    ok("PIC (IRQs 32-47)");

    // Initialize keyboard BEFORE sti
    keyboard_init();
    ok("Keyboard driver (IRQ1)");

    pending("Memory manager     [Phase 7]");
    pending("Process scheduler  [Phase 12]");
    print_char('\n');

    // Enable interrupts — keyboard now live
    __asm__ volatile("sti");

    print_color("  Vyro OS ready. Type 'help' for commands.\n\n",
                MAKE_COLOR(COLOR_LIGHT_GREY, COLOR_BLACK));

    // ── Interactive shell loop ──
    char input[256];
    uint32_t input_len = 0;

    print_prompt();

    while (1) {
        // Halt until next interrupt (saves CPU cycles)
        __asm__ volatile("hlt");

        // Process any buffered keystrokes
        while (keyboard_has_input()) {
            char c = keyboard_getchar();

            if (c == KEY_BACKSPACE) {
                if (input_len > 0) {
                    input_len--;
                    // Erase character: move back, print space, move back
                    print_char('\b');
                    print_char(' ');
                    print_char('\b');
                }

            } else if (c == '\n') {
                print_char('\n');
                input[input_len] = '\0';
                shell_run(input);
                input_len = 0;
                print_prompt();

            } else if (c >= ' ' && c < 127 && input_len < 255) {
                input[input_len++] = c;
                print_char(c);
            }
        }
    }
}
