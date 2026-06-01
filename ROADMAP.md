# Vyro OS — Roadmap

## ✅ v1.0 (shipped) — 30 phases

A complete, bootable 64-bit OS built from scratch:

- Custom BIOS bootloader → 64-bit long mode
- Monolithic kernel: IDT/PIC interrupts, PMM + heap, paging
- Cooperative scheduler with 64-bit context switching
- VyFS in-memory filesystem + persistent ATA disk
- `int 0x80` syscall ABI, ring-3 user mode (GDT + TSS), ELF64 loader
- Drivers: VESA framebuffer, PS/2 keyboard + mouse, PIT timer, RTC,
  PC speaker, PCI, ATA
- Networking stack (Ethernet/ARP/IPv4/ICMP + checksums), USB detection
- Window manager (draggable windows, Start menu, cursor)
- Multi-user security with SHA-256 password hashing
- vyropkg package manager with dependency resolution
- SMP core detection (ACPI MADT), ACPI power off/reboot
- Developer tools (dmesg, peek, prof), libvyro app framework, VyroBrowser
- UEFI boot path, CI/CD, full design document

---

## 🔭 v2.0 — making the detection real

| Area | v1.0 status | v2.0 goal |
|------|-------------|-----------|
| **Networking** | protocol structs + checksums | RTL8139 DMA TX/RX driver, real ping/DNS |
| **USB** | controller detected | xHCI ring buffers, device enumeration, USB kbd/mouse |
| **Scheduler** | cooperative | preemptive (timer-driven), priorities |
| **SMP** | cores detected | AP core bring-up (INIT-SIPI), per-core run queues |
| **Filesystem** | RAM + raw sectors | on-disk FAT32 / VyFS journaling, persistence across reboot |
| **Browser** | HTML/CSS renderer | CSS box model, real JS runtime, network fetch |
| **Graphics** | software compositor | double buffering, GPU acceleration prep, alpha blending |

---

## 🌐 v3.0 — portability & ecosystem

- ARM64 (AArch64) and RISC-V ports
- SMP-safe locking throughout the kernel
- Dynamic linking + shared libraries
- A real userland (coreutils compiled against libvyro)
- Self-hosting: build Vyro OS *on* Vyro OS
