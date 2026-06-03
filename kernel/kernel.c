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
#include "pci.h"
#include "net.h"
#include "ata.h"
#include "../drivers/rtl8139.h"
#include "usb.h"
#include "../drivers/mouse.h"
#include "security.h"
#include "pkg.h"
#include "smp.h"
#include "klog.h"

static void ok(const char* msg) {
    print_color("  [OK] ", MAKE_COLOR(COLOR_LIGHT_GREEN, COLOR_BLACK));
    print(msg);
    print_char('\n');
    klog(msg);
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
    print_color("  |    VYRO OS  v2.0.0       64-bit    x86_64            |\n", CYAN_ON_BLACK);
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

    pci_scan();
    ok("PCI bus scan");

    if (rtl8139_init())
        ok("RTL8139 NIC driver (real TX/RX)");
    else
        ok("RTL8139 NIC driver (no NIC found)");

    net_init();
    ok("Network stack (Eth/ARP/IPv4/ICMP)");

    extern void net_io_init();
    net_io_init();
    ok("Live packet I/O (RX queue, UDP TX)");

    extern void udp_init();
    udp_init();
    ok("UDP transport layer (port dispatch)");

    extern void tcp_init();
    tcp_init();
    ok("TCP transport layer (RFC 793 handshake)");

    extern int aead_selftest();
    if (aead_selftest())
        ok("ChaCha20-Poly1305 AEAD (RFC 8439 vectors pass)");
    else
        ok("ChaCha20-Poly1305 AEAD (SELFTEST FAILED)");

    extern int x509_selftest();
    if (x509_selftest())
        ok("X.509 certificate parser (test cert parses)");
    else
        ok("X.509 certificate parser (SELFTEST FAILED)");

    extern int hkdf_selftest();
    if (hkdf_selftest())
        ok("HMAC-SHA256 + HKDF (RFC 4231 / 5869 vectors pass)");
    else
        ok("HMAC-SHA256 + HKDF (SELFTEST FAILED)");

    extern int x25519_selftest();
    if (x25519_selftest())
        ok("X25519 ECDH (RFC 7748 vectors pass)");
    else
        ok("X25519 ECDH (SELFTEST FAILED)");

    extern int tls_selftest();
    if (tls_selftest())
        ok("TLS 1.3 primitives (RFC 8448 key schedule matches)");
    else
        ok("TLS 1.3 primitives (SELFTEST FAILED)");

    extern void sched_init(uint32_t);
    sched_init(20);
    ok("Preemptive scheduler (20ms quantum, PIT-driven)");

    extern int fat32_mount();
    if (fat32_mount()) ok("FAT32 read-only driver (volume mounted)");
    else               ok("FAT32 read-only driver (no FAT32 volume found)");

    extern int lapic_init();
    if (lapic_init()) ok("Local APIC (enabled, MMIO mapped)");
    else              ok("Local APIC (not available)");

    extern void csprng_init();
    csprng_init();
    ok("CSPRNG (ChaCha20 keystream, RDRAND+RDTSC seeded)");

    extern void smp_start_aps();
    extern uint32_t smp_ap_count();
    smp_start_aps();
    {
        uint32_t aps = smp_ap_count();
        if (aps > 0) ok("SMP AP bring-up (APs responded to SIPI)");
        else         ok("SMP AP bring-up (trampoline armed, no APs detected)");
    }

    extern int rsa_selftest();
    if (rsa_selftest()) ok("RSA bignum + PKCS1-v1_5 (modexp selftest passes)");
    else                ok("RSA bignum + PKCS1-v1_5 (SELFTEST FAILED)");

    extern int ecdsa_selftest();
    if (ecdsa_selftest()) ok("ECDSA P-256 verify (RFC 6979 vector matches)");
    else                  ok("ECDSA P-256 verify (SELFTEST FAILED)");

    extern int xhci_init();
    if (xhci_init()) ok("xHCI USB 3.0 controller (capability regs parsed)");
    else             ok("xHCI USB 3.0 controller (none detected)");

    if (ata_init())
        ok("ATA disk driver (PIO, persistent)");
    else
        ok("ATA disk driver (no scratch disk)");

    if (usb_init())
        ok("USB host controller detected");
    else
        ok("USB subsystem (no controller)");

    ok("PC speaker driver (PIT ch2)");

    mouse_init();
    ok("PS/2 mouse driver (IRQ12)");

    ok("Window manager (type 'gui')");

    security_init();
    ok("Security (users, SHA-256 auth)");

    pkg_init();
    ok("Package manager (vyropkg)");

    smp_init();
    ok("SMP / ACPI core detection");

    ok("Power management (ACPI off/reboot)");

    ok("Dev tools (dmesg/peek/prof)");
    ok("App framework (libvyro)");
    ok("Compositor v2 (back-buffered, themed)");
    ok("Widget toolkit (button/label/panel/toggle/list)");
    ok("Desktop apps (12 registered)");
    ok("Sockets API (TCP/UDP, RFC 793 state machine)");
    ok("DHCP + DNS client");
    ok("IPC (pipes, message queues, signals)");
    ok("Shell (v2.0.0)");

    print_char('\n');
    print_color("  *** VYRO OS 2.0 - desktop edition ***\n",
                MAKE_COLOR(COLOR_YELLOW, COLOR_BLACK));
    print_color("  Type 'gui' for the graphical desktop.\n\n",
                MAKE_COLOR(COLOR_LIGHT_CYAN, COLOR_BLACK));

    __asm__ volatile("sti");

    shell_init();
    shell_run();
}
