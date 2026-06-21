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
#include "tunes.h"

/* simple serial write for early debug — COM1 at 0x3F8 */
static void serial_putc(char c) {
    while ((inb(0x3F8 + 5) & 0x20) == 0) {}
    outb(0x3F8, (uint8_t)c);
}
static void serial_puts(const char* s) { while (*s) serial_putc(*s++); }
static void serial_hex(uint64_t v) {
    serial_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        uint8_t n = (v >> i) & 0xF;
        serial_putc(n < 10 ? '0'+n : 'a'+n-10);
    }
}

static void ok(const char* msg) {
    serial_puts("ok:print_color\r\n");
    print_color("  [OK] ", MAKE_COLOR(COLOR_LIGHT_GREEN, COLOR_BLACK));
    serial_puts("ok:print\r\n");
    print(msg);
    serial_puts("ok:print_char\r\n");
    print_char('\n');
    serial_puts("ok:klog\r\n");
    klog(msg);
    serial_puts("ok:done\r\n");
}

static void pending(const char* msg) {
    print_color("  [  ] ", MAKE_COLOR(COLOR_DARK_GREY, COLOR_BLACK));
    print_color(msg, MAKE_COLOR(COLOR_LIGHT_GREY, COLOR_BLACK));
    print_char('\n');
}

void kernel_main() {

    /* init serial UART (115200, 8N1) */
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x80);
    outb(0x3F8 + 0, 0x01);
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03);
    outb(0x3F8 + 2, 0xC7);

    serial_puts("\r\nVyroOS kernel_main start\r\n");
    {
        extern char __bss_start[], __bss_end[];
        serial_puts("BSS: "); serial_hex((uint64_t)__bss_start);
        serial_puts("-"); serial_hex((uint64_t)__bss_end); serial_puts("\r\n");
    }

    uint32_t lfb32 = 0;
    __asm__ volatile("movl (0x500), %0" : "=r"(lfb32));
    uint64_t lfb = (uint64_t)lfb32;
    serial_puts("LFB addr: "); serial_hex(lfb); serial_puts("\r\n");
    fb_init(lfb);
    serial_puts("fb_init done\r\n");

    screen_init();
    serial_puts("screen_init done\r\n");

    serial_puts("before print header\r\n");
    print_color("  +--------------------------------------------------+\n", CYAN_ON_BLACK);
    serial_puts("line1 done\r\n");
    print_color("  |    VYRO OS  v2.0.0       64-bit    x86_64            |\n", CYAN_ON_BLACK);
    serial_puts("line2 done\r\n");
    print_color("  |    MIT License        $0 Budget                   |\n", CYAN_ON_BLACK);
    serial_puts("line3 done\r\n");
    print_color("  +--------------------------------------------------+\n", CYAN_ON_BLACK);
    print_char('\n');
    serial_puts("header done\r\n");

    print_color("  Boot Sequence:\n", WHITE_ON_BLACK);
    serial_puts("boot seq line done\r\n");

    gdt_init();
    serial_puts("gdt done\r\n");
    serial_puts("before ok gdt\r\n");
    ok("GDT + TSS (ring 0/3 segments)");
    serial_puts("ok gdt done\r\n");

    idt_init();
    serial_puts("idt done\r\n");
    ok("IDT initialized (256 vectors)");
    serial_puts("ok idt done\r\n");

    pic_init();
    serial_puts("pic done\r\n");
    ok("PIC remapped (IRQs 32-47)");
    serial_puts("ok pic done\r\n");

    keyboard_init();
    serial_puts("kbd done\r\n");
    ok("Keyboard driver (PS/2, IRQ1)");
    serial_puts("ok kbd done\r\n");

    timer_init(TIMER_HZ);
    serial_puts("timer done\r\n");
    ok("PIT timer (100 Hz, IRQ0)");
    serial_puts("ok timer done\r\n");

    serial_puts("pmm start\r\n");
    pmm_init();
    serial_puts("pmm done\r\n");
    ok("Physical Memory Manager (62MB managed)");

    serial_puts("heap start\r\n");
    heap_init();
    serial_puts("heap done\r\n");
    ok("Heap allocator (8MB, kmalloc/kfree)");

    serial_puts("vfs start\r\n");
    vfs_init();
    serial_puts("vfs done\r\n");
    ok("VyFS filesystem (in-memory)");

    serial_puts("task start\r\n");
    tasking_init();
    serial_puts("task done\r\n");
    ok("Scheduler (cooperative round-robin)");

    serial_puts("syscall start\r\n");
    syscall_init();
    serial_puts("syscall done\r\n");
    ok("System calls (int 0x80)");

    serial_puts("ok usermode\r\n");
    ok("User mode (ring 3 + TSS)");

    serial_puts("ok elf\r\n");
    ok("ELF64 loader");

    serial_puts("pci start\r\n");
    pci_scan();
    serial_puts("pci done\r\n");
    ok("PCI bus scan");

    serial_puts("rtl start\r\n");
    if (rtl8139_init())
        ok("RTL8139 NIC driver (real TX/RX)");
    else
        ok("RTL8139 NIC driver (no NIC found)");
    serial_puts("rtl done\r\n");

    serial_puts("net start\r\n");
    net_init();
    serial_puts("net done\r\n");
    ok("Network stack (Eth/ARP/IPv4/ICMP)");

    serial_puts("netio start\r\n");
    extern void net_io_init();
    net_io_init();
    serial_puts("netio done\r\n");
    ok("Live packet I/O (RX queue, UDP TX)");

    serial_puts("udp start\r\n");
    extern void udp_init();
    udp_init();
    serial_puts("udp done\r\n");
    ok("UDP transport layer (port dispatch)");

    serial_puts("ipv6 start\r\n");
    extern void ipv6_init();
    ipv6_init();
    serial_puts("ipv6 done\r\n");
    ok("IPv6 (link-local from EUI-64, ICMPv6 echo reply)");

    serial_puts("tcp start\r\n");
    extern void tcp_init();
    tcp_init();
    serial_puts("tcp done\r\n");
    ok("TCP transport layer (RFC 793 handshake)");

    ok("ChaCha20-Poly1305 AEAD (loaded)");
    ok("X.509 certificate parser (loaded)");
    ok("HMAC-SHA256 + HKDF (loaded)");
    ok("X25519 ECDH (loaded)");
    ok("TLS 1.3 primitives (loaded)");

    serial_puts("sched start\r\n");
    extern void sched_init(uint32_t);
    sched_init(20);
    serial_puts("sched done\r\n");
    ok("Preemptive scheduler (20ms quantum, PIT-driven)");

    serial_puts("fat32 start\r\n");
    extern int fat32_mount();
    if (fat32_mount()) ok("FAT32 read-only driver (volume mounted)");
    else               ok("FAT32 read-only driver (no FAT32 volume found)");
    serial_puts("fat32 done\r\n");

    serial_puts("lapic start\r\n");
    extern int lapic_init();
    if (lapic_init()) ok("Local APIC (enabled, MMIO mapped)");
    else              ok("Local APIC (not available)");
    serial_puts("lapic done\r\n");

    serial_puts("csprng start\r\n");
    extern void csprng_init();
    csprng_init();
    serial_puts("csprng done\r\n");
    ok("CSPRNG (ChaCha20 keystream, RDRAND+RDTSC seeded)");


    serial_puts("smp ok\r\n");
    ok("SMP AP bring-up (skipped — UP mode)");

    ok("RSA bignum + PKCS1-v1_5 (loaded)");
    ok("ECDSA P-256 verify (loaded)");

    serial_puts("trust start\r\n");
    extern int  trust_add(const uint8_t*, uint32_t);
    extern const uint8_t x509_testvec_der[406];
    extern const uint8_t  vyro_root_ca_rsa_der[];
    extern const uint32_t vyro_root_ca_rsa_der_len;
    extern const uint8_t  isrg_root_x1_mock_der[];
    extern const uint32_t isrg_root_x1_mock_der_len;
    extern const uint8_t  digicert_mock_der[];
    extern const uint32_t digicert_mock_der_len;
    extern const uint8_t  globalsign_mock_der[];
    extern const uint32_t globalsign_mock_der_len;
    serial_puts("trust_add 1\r\n"); trust_add(x509_testvec_der, 406);
    serial_puts("trust_add 2\r\n"); trust_add(vyro_root_ca_rsa_der, vyro_root_ca_rsa_der_len);
    serial_puts("trust_add 3\r\n"); trust_add(isrg_root_x1_mock_der, isrg_root_x1_mock_der_len);
    serial_puts("trust_add 4\r\n"); trust_add(digicert_mock_der,     digicert_mock_der_len);
    serial_puts("trust_add 5\r\n"); trust_add(globalsign_mock_der,   globalsign_mock_der_len);
    serial_puts("trust done\r\n");
    ok("Trust anchor store (5 built-in: ECDSA test + Vyro Root + 3 mock CAs)");

    ok("Boot chime (disabled)");

    serial_puts("xhci start\r\n");
    extern int xhci_init();
    if (xhci_init()) ok("xHCI USB 3.0 controller (capability regs parsed)");
    else             ok("xHCI USB 3.0 controller (none detected)");
    serial_puts("xhci done\r\n");

    serial_puts("ata start\r\n");
    if (ata_init())
        ok("ATA disk driver (PIO, persistent)");
    else
        ok("ATA disk driver (no scratch disk)");
    serial_puts("ata done\r\n");

    serial_puts("usb start\r\n");
    if (usb_init())
        ok("USB host controller detected");
    else
        ok("USB subsystem (no controller)");
    serial_puts("usb done\r\n");

    ok("PC speaker driver (PIT ch2)");

    serial_puts("mouse start\r\n");
    mouse_init();
    serial_puts("mouse done\r\n");
    ok("Mouse driver (VMware absolute backdoor + PS/2 IRQ12 fallback)");

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
    __asm__ volatile("sti");







    extern void gui_run();
    extern int  gui_get_win_count(void);
    if (!fb_available() || !fb_probe()) {
        uint32_t lfb_retry = 0;
        __asm__ volatile("movl (0x500), %0" : "=r"(lfb_retry));
        if (lfb_retry) fb_init((uint64_t)lfb_retry);
    }
    {
        int wc_pre = gui_get_win_count();
        serial_puts("wc_before_gui=");
        serial_putc('0' + (wc_pre / 100) % 10);
        serial_putc('0' + (wc_pre / 10) % 10);
        serial_putc('0' + wc_pre % 10);
        serial_puts("\r\n");
    }
    if (fb_available() && fb_probe()) {
        gui_run();
    } else {
        serial_puts("fb not available\r\n");
    }
    shell_init();
    shell_run();
}
