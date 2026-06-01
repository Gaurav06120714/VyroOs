# Vyro OS 2.0

> A 64-bit **desktop operating system** built entirely from scratch — no Linux,
> no BSD, no existing kernel. From a 512-byte boot sector to a polished
> windowed desktop with apps, a dock, notifications, and a full app framework.
> **NASM + C · x86_64 · MIT · $0 budget.**

![status](https://img.shields.io/badge/version-2.0-blue)
![arch](https://img.shields.io/badge/arch-x86__64-green)
![license](https://img.shields.io/badge/license-MIT-yellow)
![phases](https://img.shields.io/badge/phases-50%2F50-success)

---

## What is Vyro OS 2.0?

A genuinely real operating system written from nothing across **50 phases**.
v1.0 (Phases 0–30) gave us a real 64-bit kernel with a shell, networking,
disk, security, ELF programs, and a basic window manager. **v2.0 (Phases
31–50)** turned it into a **modern desktop OS** — double-buffered compositor,
themed widget toolkit, an app framework, and a full suite of native apps:
Files, Settings, Terminal, TextEdit, Calculator, Clock, Task Manager,
Launchpad, Notification Center, Control Center, Browser, App Store.

## Quick start

```bash
brew install nasm qemu x86_64-elf-gcc x86_64-elf-binutils make
make
```

A 1024×768 QEMU window opens into the Vyro OS shell. Type `gui` to launch
the graphical desktop.

## The 2.0 desktop

In the GUI:

- **Dock** (bottom-center, macOS-style) — Files, Terminal, Settings, Browser, Launchpad, Notifications
- **Top bar** with live RTC clock
- **Windows** with traffic-light close/minimize/maximize, drop shadows, drag, resize, snap
- **Launchpad** — click the Apps icon to see every installed app
- **Press T** anywhere to flip dark/light theme
- **ESC** to exit

## What's new in 2.0 (Phases 31-50)

| Phase | Feature |
|-------|---------|
| 31 | Double-buffered compositor + theme system (dark/light) |
| 32 | Bitmap app icons + notification toasts + window-open animation |
| 33 | Window controls — minimize, maximize, resize grip, snap shortcuts (1-4, 0) |
| 34-35 | Widget toolkit (button, label, panel, toggle, progress, list, tile) + app framework v2 |
| 36 | **Settings app** — 8 sections (General, Display, Personal, Accounts, Network, Security, Storage, About) |
| 37 | **Files app** — VyFS browser with toolbar, breadcrumb, status |
| 38 | **Terminal app** — windowed shell with command history |
| 39 | **TextEdit app** — visual text editor |
| 40 | **Calculator + Clock apps** — big readable display, real RTC |
| 41 | **Task Manager** — CPU/memory bars, process list |
| 42 | **Launchpad** — app grid launcher |
| 43 | **Notification Center** — toasts + history |
| 44 | **Control Center** — Wi-Fi/Bluetooth/DND/AirDrop tiles, brightness, volume |
| 45 | **Multi-window VyroBrowser** — HTML renderer in a real app window |
| 46 | **Sockets API** — Berkeley sockets with TCP state machine (RFC 793) |
| 47 | **DHCP + DNS** — DHCP packet structures, DNS resolver with hosts table |
| 48 | **App Store GUI** — graphical vyropkg with install buttons |
| 49 | **IPC** — pipes, message queues, POSIX-style signals |
| 50 | Release: docs, README v2 |

## All-up feature matrix

| Subsystem | Status |
|-----------|--------|
| Bootloader (BIOS + UEFI), 64-bit long mode | ✅ |
| Interrupts (IDT/PIC), exceptions, syscalls (`int 0x80`) | ✅ |
| Memory: PMM + heap + 4GB paging | ✅ |
| Scheduler, ring-3 user mode, ELF64 loader | ✅ |
| Filesystem (VyFS), ATA disk (persistent) | ✅ |
| **Compositor v2 (back buffer), theme system** | ✅ NEW |
| **Window manager: drag/min/max/resize/snap** | ✅ NEW |
| **Widget toolkit + app framework v2** | ✅ NEW |
| **12 native apps in launcher + dock** | ✅ NEW |
| **Notifications + Control Center + Task Manager** | ✅ NEW |
| Networking (Eth/ARP/IPv4/ICMP) + **Sockets API + DHCP + DNS** | ✅ |
| **IPC: pipes, queues, signals** | ✅ NEW |
| Security: users + SHA-256 auth, ring 0/3 | ✅ |
| Package manager (vyropkg) + **graphical App Store** | ✅ NEW |
| Sound (PC speaker), mouse, keyboard, RTC | ✅ |
| SMP detection (ACPI), ACPI power off/reboot | ✅ |

## Documentation

- **[docs/DESIGN.md](docs/DESIGN.md)** — full architecture, memory map, diagrams
- **[docs/DESIGN-2.0.md](docs/DESIGN-2.0.md)** — desktop-layer architecture
- **[ROADMAP.md](ROADMAP.md)** — v3.0+
- **[boot/uefi/README.md](boot/uefi/README.md)** — UEFI boot path

## Source layout

```
boot/        bootloaders (BIOS asm + UEFI C)
kernel/      kernel + subsystems
kernel/apps/ 12 native desktop applications
drivers/     hardware drivers
user/        sample ELF app + libvyro framework
docs/        design documents
```

## License

MIT.
