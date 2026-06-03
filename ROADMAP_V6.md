# Vyro OS — v6 Production Roadmap

> Companion to `AUDIT.md`. Numbers below are realistic person-month estimates assuming
> one full-time experienced kernel engineer. Times multiply when each phase has
> hardware-acquisition delays, vendor-blob unknowns, or reverse-engineering work.

## Track A — Real x86_64 PC hardware (closest to current capability)

| Phase | Title | Effort | What it unlocks |
|---|---|---|---|
| **v6.0** | UEFI boot path solid + USB image | 1 mo | Boots on any UEFI PC from a USB stick |
| **v6.1** | E820 / UEFI memory map | 2 wk | Real available RAM detection |
| **v6.2** | CPUID + feature detection | 1 wk | Knows whether AES-NI, SHA, AVX exist |
| **v6.3** | AHCI SATA driver | 3 wk | Boots from real SATA SSD |
| **v6.4** | NVMe driver | 4 wk | Boots from real NVMe SSD |
| **v6.5** | Intel E1000 / E1000e NIC | 2 wk | Wired Ethernet on real Intel PCs |
| **v6.6** | RTL8169 / 8125 NIC | 2 wk | Wired Ethernet on real Realtek PCs |
| **v6.7** | xHCI device enumeration | 4 wk | Real USB keyboards / mice |
| **v6.8** | HD Audio (Intel HDA) | 4 wk | Speakers on real PCs |
| **v6.9** | ACPI AML interpreter (minimal) | 8 wk | Real battery, fan, thermals on PCs |
| **v6.10** | Intel iGPU mode-setting | 12 wk | Native HD resolution on Intel Macs |

**Track A total:** ~10 person-months. Gets Vyro running on a real PC with Ethernet, USB keyboard, native graphics, real battery.

## Track B — Apple Silicon (the big one)

| Phase | Title | Effort | Notes |
|---|---|---|---|
| **v7.0** | ARM64 architecture port + HAL | 2 mo | Boots in QEMU virt |
| **v7.1** | Apple Silicon boot chain (m1n1 fork) | 3 mo | Can be launched from macOS Recovery |
| **v7.2** | Apple FDT (device tree) parser | 1 mo | Knows what's on the SoC |
| **v7.3** | AIC interrupt controller | 1 mo | Receives interrupts |
| **v7.4** | DART IOMMU | 2 mo | Mandatory before any DMA |
| **v7.5** | PCIe controller | 2 mo | Talks to the SoC's PCIe |
| **v7.6** | Apple ANS2 NVMe | 3 mo | Reads internal SSD |
| **v7.7** | USB-C controllers (Type-C ATC) | 2 mo | External peripherals |
| **v7.8** | Apple GPU (AGX) | **18+ mo** | The hardest single subsystem |
| **v7.9** | Display Coprocessor (DCP) | 6 mo | Native panel output |
| **v7.10** | PMGR + thermal | 2 mo | Real battery + power |
| **v7.11** | Broadcom WiFi firmware load | 3 mo | Wireless |
| **v7.12** | Audio (CS42L84 / TAS5770) | 3 mo | Speakers |

**Track B total:** ~48 person-months / ~4 years. Matches Asahi's actual timeline.

## Track C — Userspace + applications

| Phase | Title | Effort |
|---|---|---|
| **v8.0** | Per-process virtual memory | 2 mo |
| **v8.1** | Linux syscall ABI emulation (top 50) | 4 mo |
| **v8.2** | musl libc port | 3 mo |
| **v8.3** | Dynamic ELF linker | 2 mo |
| **v8.4** | POSIX shell (Vyro `vsh`) | 1 mo |
| **v8.5** | Coreutils | 1 mo |
| **v8.6** | Package manager + repo | 3 mo |
| **v8.7** | X11 / Wayland minimal compositor | 6 mo |
| **v8.8** | Mesa Gallium driver port | 6 mo |
| **v8.9** | GCC port hosted-on-Vyro | 6 mo |
| **v8.10** | Chromium build (after libc + X11 + Mesa) | 12 mo |

**Track C total:** ~46 person-months. Gets a real desktop with Chromium.

## Track D — Security + production hardening

| Phase | Title | Effort |
|---|---|---|
| **v9.0** | KASLR + stack canaries | 1 mo |
| **v9.1** | Per-process page tables | 2 mo |
| **v9.2** | Apple Secure Boot signing pipeline | 2 mo |
| **v9.3** | Full-disk encryption | 3 mo |
| **v9.4** | seccomp / sandbox | 3 mo |
| **v9.5** | Microcode update path | 1 mo |

**Track D total:** ~12 person-months.

## Grand total

| Track | Person-months |
|---|---|
| A (real PCs) | 10 |
| B (Apple Silicon) | 48 |
| C (userspace + Chromium) | 46 |
| D (security) | 12 |
| **Total** | **~116 PM ≈ 10 engineer-years** |

## What this session can deliver toward this roadmap

Honest scope: **v5.1 → v6.0** in this session covers the audit, this roadmap, and the most achievable foundational phases from Track A: ARM64 HAL skeleton, ACPI table walker, CPUID parsing, E820 memory map, AHCI scaffolding, NIC abstraction, and a real bootable USB image. Anything beyond that is multi-month work that doesn't fit a chat.

## Per-phase deliverable template (used by every shipped phase below)

```
v6.X — <title>
  - Objective:             single sentence
  - Prerequisites:          earlier phases or external dependencies
  - Code size (LOC):        rough number
  - Files added/modified:   list
  - Build commands:         exact lines
  - QEMU test:              exact qemu-system-x86_64 invocation
  - Real-hardware test:     what you'd do on real PC / Mac
  - Success criteria:       observable behavior
  - Risks:                  what could go wrong
  - Git workflow:           commit message format + tag + push
```
