#include "../drivers/screen.h"
#include "../drivers/framebuffer.h"
#include "../drivers/pic.h"
#include "../drivers/keyboard.h"
#include "../drivers/timer.h"
#include "../include/types.h"
#include "idt.h"
#include "isr.h"
#include "shell.h"
#include "pmm.h"
#include "heap.h"
#include "vfs.h"
#include "task.h"
#include "syscall.h"
#include "gdt.h"

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
    // Read LFB address stored by bootloader at physical 0x0500.
    // Use inline asm to prevent the optimizer from treating 0x0500 as
    // a NULL-region dereference (which triggers a spurious Warray-bounds).
    uint32_t lfb32 = 0;
    __asm__ volatile("movl (0x500), %0" : "=r"(lfb32));
    uint64_t lfb = (uint64_t)lfb32;
    fb_init(lfb);

    screen_init();

    print_color("  +--------------------------------------------------+\n", CYAN_ON_BLACK);
    print_color("  |    VYRO OS  v0.15.0    64-bit    x86_64            |\n", CYAN_ON_BLACK);
    print_color("  |    MIT License        $0 Budget                   |\n", CYAN_ON_BLACK);
    print_color("  +--------------------------------------------------+\n", CYAN_ON_BLACK);
    print_char('\n');

    print_color("  Boot Sequence:\n", WHITE_ON_BLACK);

    gdt_init();
    ok("GDT + TSS (ring 0/3 segments)");

    idt_init();
    ok("IDT initialized (256 vectors)");

    pic_init();
    ok("PIC remapped (IRQs 32-47)");

    keyboard_init();
    ok("Keyboard driver (PS/2, IRQ1)");

    timer_init(TIMER_HZ);
    ok("PIT timer (100 Hz, IRQ0)");

    pmm_init();
    ok("Physical Memory Manager (62MB managed)");

    heap_init();
    ok("Heap allocator (8MB, kmalloc/kfree)");

    vfs_init();
    ok("VyFS filesystem (in-memory)");

    tasking_init();
    ok("Scheduler (cooperative round-robin)");

    syscall_init();
    ok("System calls (int 0x80)");

    ok("User mode (ring 3 + TSS)");

    ok("ELF64 loader");

    ok("Shell (v0.15.0)");

    pending("Networking         [Phase 16]");
    pending("Disk driver        [Phase 17]");
    print_char('\n');

    __asm__ volatile("sti");

    shell_init();
    shell_run();
}
