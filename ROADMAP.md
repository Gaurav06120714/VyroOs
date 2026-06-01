# Vyro OS — Roadmap

## ✅ v1.0 (shipped) — Phases 0-30

Full 64-bit OS: bootloader, kernel, interrupts, memory, scheduler, ring 3,
ELF loader, VyFS, ATA disk, networking stack, security, package manager,
SMP detection, ACPI power, UEFI boot, basic window manager.

## ✅ v2.0 (shipped) — Phases 31-50

Modern desktop OS layer:

- Double-buffered compositor + theme system (dark/light)
- macOS-style dock, Windows-style top bar, live clock
- Bitmap icons, drop shadows, window animations
- Full window controls: min/max/resize/snap
- Widget toolkit + app framework v2
- **12 native apps**: Files, Settings, Terminal, TextEdit, Calculator, Clock,
  Task Manager, Launchpad, Notification Center, Control Center, Browser,
  App Store
- Sockets API (TCP state machine), DHCP & DNS client architecture
- IPC: pipes, message queues, signals
- Notification toasts + notification center history

---

## 🔭 v3.0 — making the network real

| Area | v2.0 status | v3.0 goal |
|------|-------------|-----------|
| **Networking transport** | API + state machines | Real RTL8139 DMA TX/RX, live ping |
| **DHCP** | packet structures | Live DHCPDISCOVER over real NIC |
| **DNS** | static hosts table | Real UDP/53 queries |
| **TLS** | architecture-only | Real X.509 + ChaCha20-Poly1305 |
| **TCP** | state machine | Full segment reassembly, congestion control |
| **USB** | controller detected | xHCI ring buffers, real device enumeration |
| **Scheduler** | cooperative | Preemptive (timer-driven), per-process VMs |
| **SMP** | cores detected | AP bring-up (INIT-SIPI), per-core queues |
| **Disk FS** | raw sectors | FAT32 read+write, persistent VyFS across boots |

## 🌐 v4.0 — portability + self-hosting

- ARM64 (AArch64) and RISC-V ports
- Dynamic linking + shared libraries
- A real userland: coreutils compiled against libvyro
- Self-hosting: build Vyro OS *on* Vyro OS
- Real font rasterizer (TrueType subset)
- GPU acceleration via virtio-gpu
