#include "../drivers/screen.h"
#include "../drivers/pic.h"
#include "../drivers/keyboard.h"
#include "../include/types.h"
#include "idt.h"
#include "isr.h"
#include "shell.h"

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

void kernel_main() {
    screen_init();

    // Print header
    print_color("  +------------------------------------------+\n", CYAN_ON_BLACK);
    print_color("  |  VYRO OS  v0.6.0   64-bit   x86_64       |\n", CYAN_ON_BLACK);
    print_color("  |  MIT License       $0 Budget              |\n", CYAN_ON_BLACK);
    print_color("  +------------------------------------------+\n", CYAN_ON_BLACK);
    print_char('\n');

    // Boot sequence
    print_color("  Boot Sequence:\n", WHITE_ON_BLACK);

    idt_init();
    ok("IDT initialized (256 vectors)");

    pic_init();
    ok("PIC remapped (IRQs 32-47)");

    keyboard_init();
    ok("Keyboard driver (PS/2, IRQ1)");

    ok("Shell initialized (v0.6.0)");

    pending("Memory manager     [Phase 7]");
    pending("Process scheduler  [Phase 12]");
    print_char('\n');

    // Enable interrupts
    __asm__ volatile("sti");

    // Hand off to shell — never returns
    shell_init();
    shell_run();
}
