#include "../drivers/screen.h"
#include "../drivers/pic.h"
#include "../include/types.h"
#include "idt.h"
#include "isr.h"

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

static void print_info() {
    print_color("  ┌──────────────────────────────────────────┐\n", YELLOW_ON_BLACK);
    print_color("  │           VYRO OS  v0.4.0                │\n", YELLOW_ON_BLACK);
    print_color("  │     64-bit Kernel  |  x86_64             │\n", YELLOW_ON_BLACK);
    print_color("  │     MIT License    |  $0 Budget          │\n", YELLOW_ON_BLACK);
    print_color("  └──────────────────────────────────────────┘\n", YELLOW_ON_BLACK);
    print_char('\n');
}

static void ok(const char* msg) {
    print_color("  [OK] ", MAKE_COLOR(COLOR_LIGHT_GREEN, COLOR_BLACK));
    print_color(msg, WHITE_ON_BLACK);
    print_char('\n');
}

static void pending(const char* msg) {
    print_color("  [  ] ", MAKE_COLOR(COLOR_DARK_GREY, COLOR_BLACK));
    print_color(msg, MAKE_COLOR(COLOR_LIGHT_GREY, COLOR_BLACK));
    print_char('\n');
}

void kernel_main() {
    screen_init();
    print_banner();
    print_info();

    idt_init();
    pic_init();

    print_color("  Boot Sequence:\n", WHITE_ON_BLACK);
    ok("Bootloader — CHS disk read");
    ok("64-bit Long Mode active");
    ok("Kernel loaded at 0x10000");
    ok("Screen driver initialized");
    ok("IDT loaded (256 vectors)");
    ok("PIC remapped (IRQs 32-47)");
    pending("Interrupts             [enabling now]");
    pending("Keyboard driver        [Phase 5]");
    pending("Memory manager         [Phase 7]");
    pending("Process scheduler      [Phase 12]");
    print_char('\n');
    print_color("  Vyro OS running.\n", MAKE_COLOR(COLOR_LIGHT_GREY, COLOR_BLACK));

    __asm__ volatile("sti");   // Enable interrupts last — after all output
    while (1) __asm__ volatile("hlt");
}
