# Vyro OS — Design Document (v1.0)

> A 64-bit operating system built entirely from scratch — no Linux, no BSD,
> no existing kernel. From a 512-byte boot sector to a windowed desktop.
> Language: NASM + C. Budget: $0. License: MIT.

---

## 1. Overview

Vyro OS is a monolithic x86_64 operating system developed across 30 phases.
It boots via a custom bootloader into 64-bit long mode, runs a custom kernel
with preemptive-ready scheduling, drives real hardware (framebuffer, keyboard,
mouse, ATA disk, PC speaker, PCI/network/USB), enforces a multi-user security
model with SHA-256, runs unprivileged ELF programs in ring 3, and presents both
a text shell and a graphical window manager.

---

## 2. System Architecture

```
┌───────────────────────────────────────────────────────────────┐
│                        APPLICATIONS                           │
│   ELF programs (ring 3)  •  libvyro app framework  •  VyroBrowser │
├───────────────────────────────────────────────────────────────┤
│                     SYSTEM CALL INTERFACE                     │
│              int 0x80  •  11 syscalls  •  user→kernel          │
├───────────────────────────────────────────────────────────────┤
│                          KERNEL (ring 0)                      │
│  ┌────────────┬────────────┬────────────┬──────────────────┐  │
│  │ Scheduler  │  Memory     │ Filesystem │  Security        │  │
│  │ (tasks,    │  (PMM +     │ (VyFS RAM, │  (users, SHA-256,│  │
│  │  ctx switch)│  heap)      │  ATA disk) │  ring 0/3)       │  │
│  ├────────────┼────────────┼────────────┼──────────────────┤  │
│  │ Interrupts │  Timers     │ Package    │  Window manager  │  │
│  │ (IDT/PIC)  │ (PIT/RTC)   │ (vyropkg)  │  (compositor)    │  │
│  └────────────┴────────────┴────────────┴──────────────────┘  │
├───────────────────────────────────────────────────────────────┤
│                       DEVICE DRIVERS                          │
│ framebuffer • keyboard • mouse • ATA • PC speaker • PCI • net  │
├───────────────────────────────────────────────────────────────┤
│                         HARDWARE (x86_64)                     │
│   CPU (long mode) • RAM • APIC • PCI bus • disks • NIC • VGA   │
└───────────────────────────────────────────────────────────────┘
```

---

## 3. Boot Sequence

```
Power On
   │
BIOS / UEFI firmware
   │
boot.asm (512 bytes, MBR)            [BIOS path — default]
   │  • VBE 1024x768 framebuffer
   │  • copy BIOS font to 0x80000
   │  • LBA load kernel → 0x10000
   │  • A20, GDT, 16→32 protected mode
   │  • identity-map 0–4GB (2MB pages, user bit)
   │  • enable PAE, long mode (EFER.LME), paging
   │  • 32→64 long mode
   ▼
kernel_entry.asm  → kernel_main()
   │  GDT+TSS → IDT → PIC → keyboard → timer → PMM → heap →
   │  VyFS → scheduler → syscalls → user mode → ELF →
   │  PCI → net → ATA → USB → speaker → mouse → GUI →
   │  security → vyropkg → SMP → power → dev tools → shell
   ▼
Interactive shell (50+ commands) / GUI desktop
```

---

## 4. Memory Map

```
0x000000 – 0x0004FF   Real-mode IVT / BIOS data
0x000500             Framebuffer LFB address (set by bootloader)
0x000600             VBE mode-info block
0x001000 – 0x002FFF   PML4 + PDPT
0x007C00 – 0x007DFF   Bootloader
0x010000 – 0x02FFFF   Kernel (~68KB, grows via LBA read)
0x070000 – 0x073FFF   Page directories (4GB identity map, 2MB pages)
0x080000             BIOS 8x16 font
0x090000             Boot/kernel stack
0x100000 – 0x1FFFFF   PMM bitmap
0x200000 – 0x4FFFFF   PMM-managed physical pages
0x500000 – 0xCFFFFF   Kernel heap (8MB)
0xFD000000           VESA linear framebuffer (QEMU std VGA)
```

---

## 5. Subsystem Reference

### Memory Management
- **PMM** — bitmap allocator, 1 bit/4KB page, 62MB managed.
- **Heap** — first-fit `kmalloc`/`kfree` with block coalescing, 8MB.
- **Paging** — 4GB identity map with 2MB huge pages, user bit set for ring 3.

### Scheduler
- Cooperative round-robin, per-task 8KB stacks, 64-bit context switch
  (callee-saved regs swap in `switch.asm`). Fake initial stack frame so the
  first switch `ret`s into the task entry.

### Interrupts
- 256-entry IDT. PIC remapped to vectors 32–47. Exception handlers (panic
  screen), IRQ dispatch table, `int 0x80` syscall gate (DPL 3).

### Filesystem
- **VyFS** — RAM tree (first-child/next-sibling), files + dirs, `kmalloc`-backed.
- **ATA PIO** — persistent 512-byte sector read/write to a real disk.

### Security
- Users with per-user salts, **SHA-256(salt‖password)** (FIPS-verified).
- Admin vs standard roles; ring 0/3 privilege separation via GDT + TSS.

### Graphics
- VESA 1024×768×24bpp framebuffer, 8×16 font, shadow buffer for scrolling.
- Compositor: desktop, windows, draggable title bars, z-order, Start menu.
- PS/2 mouse (IRQ12) with save-under cursor.

### Networking / Buses
- PCI enumeration (config ports 0xCF8/0xCFC).
- Ethernet/ARP/IPv4/ICMP structures + RFC-1071 checksum.
- USB controller detection (xHCI via PCI).
- SMP/core detection via ACPI MADT.

---

## 6. System Call ABI

`int 0x80`, number in `rax`, args in `rdi/rsi/rdx`, return in `rax`.

| # | Name | Arg | Returns |
|---|------|-----|---------|
| 1 | WRITE | string ptr | bytes written |
| 2 | GETPID | — | pid |
| 3 | SLEEP | ms | 0 |
| 4 | UPTIME | — | ms since boot |
| 5 | CLEAR | — | 0 |
| 6 | PUTCHAR | char | 0 |
| 7 | VERSION | — | version |
| 8 | EXIT | — | (no return) |
| 9 | TICKS | — | timer ticks |
| 10 | RAND | — | random u32 |

---

## 7. Build System

```
make          build + run in QEMU (BIOS path, 4 cores, NIC, USB, audio, 2 disks)
make build    build only
make clean    remove build artifacts
make debug    QEMU with GDB stub on :1234
```

- Cross-compiler: `x86_64-elf-gcc` (freestanding, `-mno-sse` to avoid
  unaligned-SSE faults, no red zone).
- Kernel linked as a flat binary at `0x10000`.
- User ELF programs compiled separately, embedded as a byte array.

---

## 8. Source Layout

```
VyroOs/
├── boot/
│   ├── boot.asm          BIOS bootloader (MBR)
│   └── uefi/uefi_boot.c  UEFI bootloader (alternative path)
├── kernel/               kernel + subsystems (idt, pmm, heap, vfs, task,
│                         syscall, gdt, elf, pci, net, ata, usb, smp,
│                         security, sha256, pkg, power, klog, html, gui, shell)
├── drivers/              screen, framebuffer, keyboard, mouse, timer, rtc,
│                         pic, speaker
├── user/                 init.c (sample app), libvyro.h (app framework)
├── include/types.h       freestanding types
├── docs/DESIGN.md        this document
├── link.ld               kernel linker script
└── Makefile              build system
```

---

## 9. Engineering Notes & Hard-Won Fixes

- **SSE triple-fault**: `-O2` auto-vectorized framebuffer loops into `movaps`;
  fixed by forbidding SSE/MMX/FPU codegen (`-mno-sse …`).
- **Ring-3 page fault**: pages were supervisor-only; set the **user bit (0x04)**
  on every paging level so ring 3 can execute.
- **Black-screen hang**: CHS `int 0x13` can't cross track boundaries → switched
  to **LBA extended reads** in 16-sector chunks.
- **Framebuffer scrolling**: write-only framebuffer can't be read back → added a
  **shadow text buffer** in RAM and re-render on scroll.

---

## 10. Roadmap → v2.0

See `ROADMAP.md`. Highlights: real NIC TX/RX driver, full USB enumeration,
preemptive scheduling, SMP core bring-up (not just detection), a real JS engine
for VyroBrowser, on-disk persistent filesystem, and ARM64/RISC-V ports.
