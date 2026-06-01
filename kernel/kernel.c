#include "../drivers/screen.h"
#include "../include/types.h"

// ─────────────────────────────────────────────────
// Print a horizontal divider line
// ─────────────────────────────────────────────────
static void print_divider(char c, int len) {
    for (int i = 0; i < len; i++) print_char(c);
    print_char('\n');
}

// ─────────────────────────────────────────────────
// Print the Vyro OS banner
// ─────────────────────────────────────────────────
static void print_banner() {
    print_color("\n", WHITE_ON_BLACK);
    print_color("  ██╗   ██╗██╗   ██╗██████╗  ██████╗      ██████╗ ███████╗\n", CYAN_ON_BLACK);
    print_color("  ██║   ██║╚██╗ ██╔╝██╔══██╗██╔═══██╗    ██╔═══██╗██╔════╝\n", CYAN_ON_BLACK);
    print_color("  ██║   ██║ ╚████╔╝ ██████╔╝██║   ██║    ██║   ██║███████╗\n", CYAN_ON_BLACK);
    print_color("  ╚██╗ ██╔╝  ╚██╔╝  ██╔══██╗██║   ██║    ██║   ██║╚════██║\n", CYAN_ON_BLACK);
    print_color("   ╚████╔╝    ██║   ██║  ██║╚██████╔╝    ╚██████╔╝███████║\n", CYAN_ON_BLACK);
    print_color("    ╚═══╝     ╚═╝   ╚═╝  ╚═╝ ╚═════╝      ╚═════╝ ╚══════╝\n", CYAN_ON_BLACK);
    print_char('\n');
}

// ─────────────────────────────────────────────────
// Print system info table
// ─────────────────────────────────────────────────
static void print_info() {
    print_color("  ┌──────────────────────────────────────────┐\n", YELLOW_ON_BLACK);
    print_color("  │           VYRO OS  v0.3.0                │\n", YELLOW_ON_BLACK);
    print_color("  │     64-bit Kernel  |  x86_64             │\n", YELLOW_ON_BLACK);
    print_color("  │     MIT License    |  $0 Budget          │\n", YELLOW_ON_BLACK);
    print_color("  └──────────────────────────────────────────┘\n", YELLOW_ON_BLACK);
    print_char('\n');
}

// ─────────────────────────────────────────────────
// Print boot status checklist
// ─────────────────────────────────────────────────
static void print_boot_status() {
    print_color("  Boot Status:\n", WHITE_ON_BLACK);
    print_color("  [OK] ", MAKE_COLOR(COLOR_LIGHT_GREEN, COLOR_BLACK));
    print_color("Bootloader Stage 1\n", WHITE_ON_BLACK);

    print_color("  [OK] ", MAKE_COLOR(COLOR_LIGHT_GREEN, COLOR_BLACK));
    print_color("64-bit Long Mode Active\n", WHITE_ON_BLACK);

    print_color("  [OK] ", MAKE_COLOR(COLOR_LIGHT_GREEN, COLOR_BLACK));
    print_color("Kernel Loaded at 0x100000\n", WHITE_ON_BLACK);

    print_color("  [OK] ", MAKE_COLOR(COLOR_LIGHT_GREEN, COLOR_BLACK));
    print_color("Screen Driver Initialized\n", WHITE_ON_BLACK);

    print_color("  [  ] ", MAKE_COLOR(COLOR_LIGHT_RED, COLOR_BLACK));
    print_color("Interrupt System        [Phase 4]\n", WHITE_ON_BLACK);

    print_color("  [  ] ", MAKE_COLOR(COLOR_LIGHT_RED, COLOR_BLACK));
    print_color("Keyboard Driver         [Phase 5]\n", WHITE_ON_BLACK);

    print_color("  [  ] ", MAKE_COLOR(COLOR_LIGHT_RED, COLOR_BLACK));
    print_color("Memory Manager          [Phase 7]\n", WHITE_ON_BLACK);

    print_char('\n');
    print_color("  Vyro OS kernel running. System halted.\n\n",
                MAKE_COLOR(COLOR_LIGHT_GREY, COLOR_BLACK));
}

// ─────────────────────────────────────────────────
// kernel_main: entry point called from kernel_entry.asm
// This function never returns.
// ─────────────────────────────────────────────────
void kernel_main() {
    screen_init();
    print_banner();
    print_info();
    print_boot_status();

    // Kernel idle loop — will be replaced by scheduler in Phase 12
    while (1) {
        __asm__ volatile("hlt");
    }
}
