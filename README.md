# Vyro OS

> A 64-bit operating system built **entirely from scratch** — no Linux, no BSD,
> no existing kernel. From a 512-byte boot sector to a windowed graphical desktop.
> **NASM + C · x86_64 · MIT · $0 budget.**

![status](https://img.shields.io/badge/version-1.0-blue) ![arch](https://img.shields.io/badge/arch-x86__64-green) ![license](https://img.shields.io/badge/license-MIT-yellow)

---

## What is this?

Vyro OS is a real operating system written from nothing across **30 phases** —
each phase adding a genuine subsystem, verified to boot without faults. It boots
in 64-bit long mode, runs unprivileged ELF programs in ring 3, drives real
hardware, persists to disk, has a networking stack, a multi-user security model
with SHA-256, a package manager, and a mouse-driven window manager.

## Quick start (macOS / Linux)

```bash
# Install toolchain (macOS)
brew install nasm qemu x86_64-elf-gcc x86_64-elf-binutils make

# Build and run
make
```

A 1024×768 QEMU window opens into the Vyro OS shell. Type `help`.

## Try these

```
help            list all 50+ commands
sysinfo         full system dashboard
gui             graphical window manager (drag windows, ESC to exit)
browser         VyroBrowser — renders HTML on the framebuffer
exec            load & run an ELF64 program in ring 3
vyropkg install editor    package manager with dependency resolution
login root toor           multi-user auth (SHA-256)
diskwrite 5 hello / diskread 5    persistent disk I/O
cpuinfo         CPU cores via ACPI
ping            build & validate an ICMP packet
play            PC-speaker music
shutdown        ACPI power off
```

## Feature matrix

| Subsystem | Status |
|-----------|--------|
| 64-bit bootloader (BIOS + UEFI) | ✅ |
| Interrupts (IDT/PIC), exceptions | ✅ |
| Memory: PMM bitmap + heap + paging | ✅ |
| Cooperative scheduler + context switch | ✅ |
| Syscalls (`int 0x80`) + ring-3 user mode | ✅ |
| ELF64 loader | ✅ |
| VyFS filesystem + persistent ATA disk | ✅ |
| Framebuffer GUI + window manager + mouse | ✅ |
| Networking (Eth/ARP/IPv4/ICMP) + PCI + USB | ✅ |
| PC speaker sound, RTC clock, PIT timer | ✅ |
| Security: users + SHA-256 auth | ✅ |
| Package manager (vyropkg) | ✅ |
| SMP detection + ACPI power management | ✅ |
| Dev tools, app framework (libvyro), VyroBrowser | ✅ |

## Documentation

- **[docs/DESIGN.md](docs/DESIGN.md)** — full architecture, memory map, diagrams
- **[ROADMAP.md](ROADMAP.md)** — v2.0 / v3.0 plans
- **[boot/uefi/README.md](boot/uefi/README.md)** — UEFI boot path

## Project layout

```
boot/      bootloaders (BIOS asm + UEFI C)
kernel/    kernel & subsystems
drivers/   hardware drivers
user/      sample app + libvyro framework
docs/      design document
```

## License

MIT — build on it, learn from it, ship it.
