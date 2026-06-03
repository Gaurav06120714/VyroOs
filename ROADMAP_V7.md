# Vyro OS — ROADMAP v7 (Tri-Path)

This roadmap supersedes the single-track v6 plan. Vyro OS is now developed
along three concurrent paths.

## Path A — Ubuntu Remix (target: 4 weeks)

Goal: ship a real bootable, installable `vyro-os-7.x-amd64.iso` based on
Ubuntu 24.04 LTS with full Vyro branding and a glassmorphism GNOME desktop.

| Tag | Phase | Deliverable |
|-----|-------|-------------|
| vA.7.0 | A0 | live-build skeleton, build.sh, PHASES.md, directory layout |
| vA.7.1 | A1 | Ubuntu 24.04 base config (`auto/config`, package selection) |
| vA.7.2 | A2 | Vyro GNOME shell theme (glassmorphism, dark+light variants) |
| vA.7.3 | A3 | GTK theme + icon theme |
| vA.7.4 | A4 | Plymouth boot splash |
| vA.7.5 | A5 | GDM login theme |
| vA.7.6 | A6 | Installer (Calamares) slideshow + branding |
| vA.7.7 | A7 | Default app set: VyroBrowser, Files, Terminal, TextEdit, Settings |
| vA.7.8 | A8 | First ISO build via GitHub Actions, published as Release |
| vA.7.9 | A9 | QEMU + real-hardware test pass, README screenshots |
| vA.7.10 | A10 | Public release announcement, docs polish |

Build artifact: `vyro-os-7.x-amd64.iso` (~3 GB).
Tested on: QEMU, VirtualBox, real x86_64 PCs.

## Path B — Linux Kernel + Vyro Userland (target: 4 months)

Goal: boot the Linux kernel into Vyro's compositor/window manager/apps
with no GNOME, no Wayland, no X11 — just DRM/KMS and libvyro.

| Tag | Phase | Deliverable |
|-----|-------|-------------|
| vB.0.0 | B0 | Buildroot skeleton, BR2_EXTERNAL tree, defconfig |
| vB.0.1 | B1 | Minimal Linux 6.x boot to a Vyro init shell over framebuffer |
| vB.0.2 | B2 | libvyro-linux: replace `int 0x80` syscalls with Linux glibc/musl calls |
| vB.0.3 | B3 | compositor-drm: open `/dev/dri/card0`, allocate dumb buffer, draw |
| vB.0.4 | B4 | Port window manager to compositor-drm; render desktop background |
| vB.0.5 | B5 | Port input (evdev `/dev/input/event*`) for mouse + keyboard |
| vB.0.6 | B6 | Port the first 4 desktop apps (Files, Terminal, TextEdit, Calculator) |
| vB.0.7 | B7 | Port the remaining 8 apps |
| vB.0.8 | B8 | Networking via Linux sockets (replace VyroTCP) |
| vB.0.9 | B9 | Audio via ALSA |
| vB.0.10 | B10 | First public `vyro-core-7.x.img` (Buildroot-built bootable disk image) |

Build artifact: `vyro-core-7.x.img` (~200 MB).
Tested on: QEMU first, real hardware second.

## Path C — Microkernel (research, continues)

The original from-scratch kernel work continues, but at lower priority
and with honest expectations.

Top-priority remaining items from ROADMAP_V6:

| Tag | Phase | Deliverable | Effort |
|-----|-------|-------------|--------|
| vC.6.1 | C1 | AHCI command list + FIS read/write (boot from real SATA) | ~2 PM |
| vC.6.2 | C2 | NVMe Admin Queue + Identify + I/O (boot from real NVMe) | ~2 PM |
| vC.6.3 | C3 | E1000 DMA TX/RX rings (real Ethernet) | ~1 PM |
| vC.6.4 | C4 | xHCI Address Device + HID enumeration (real USB keyboard) | ~2 PM |
| vC.6.5 | C5 | BIOS E820 / UEFI GetMemoryMap parsing (real RAM maps) | ~0.5 PM |
| vC.6.6 | C6 | True preemptive scheduler via timer IRQ | ~1 PM |

Beyond v6.6, see ROADMAP_V6.md for the remaining ~108 PM (Apple Silicon,
userspace, Chromium, security hardening).

## Cross-path coordination

- **Design system** lives in `docs/DESIGN.md` and is shared by all paths.
- **Branding assets** (logos, wallpapers) live in `branding/` and are
  consumed by Paths A, B, and C.
- **VyroBrowser** is built as both a Linux ELF (used by A and B) and a
  microkernel ELF (used by C). The source lives in `apps/vyrobrowser/`
  with `#ifdef __VYRO_MICROKERNEL__` for the syscall layer.

## Versioning and release cadence

- Path A: weekly tags, target monthly ISO releases
- Path B: monthly tags, target quarterly image releases
- Path C: when material progress lands, no fixed cadence
- Unified `v7.X` meta-tags: cut when all three paths hit a synchronized milestone
