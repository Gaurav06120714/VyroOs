# Vyro OS 7.0 — Tri-Path Release

> A real, installable, daily-drivable operating system — built three ways in parallel.
> **NASM + C · Ubuntu base · Linux kernel + Vyro userland · $0 budget.**

![status](https://img.shields.io/badge/version-7.0-blue)
![arch](https://img.shields.io/badge/arch-x86__64-green)
![license](https://img.shields.io/badge/license-MIT-yellow)
![paths](https://img.shields.io/badge/paths-A%20%7C%20B%20%7C%20C-purple)

---

## What changed in 7.0

After 80 tags and 6 major versions of from-scratch microkernel work, Vyro OS
is forking into three concurrent product paths. Each path serves a real
audience and ships a real artifact.

| Path | Identity | Base | Time-to-ship | Lives in |
|------|----------|------|--------------|----------|
| **A** | Vyro OS Desktop | Ubuntu 24.04 LTS remix | 2–4 weeks | `path-a-ubuntu-remix/` |
| **B** | Vyro OS Core | Linux kernel + Vyro userland | 2–4 months | `path-b-linux-core/` |
| **C** | Vyro Microkernel | from-scratch 64-bit kernel | ongoing research | repo root |

The original microkernel work (boot/, kernel/, drivers/, etc.) is preserved
unchanged at the repo root and continues as **Path C**. Paths A and B are
new top-level directories so the three product lines can ship independently.

---

## Path A — Vyro OS Desktop (Ubuntu remix)

**Goal:** ship a real installable ISO that boots on any PC, runs Chrome,
talks to WiFi, drives the GPU — within weeks, not years.

- **Base:** Ubuntu 24.04 LTS (kernel 6.8, GNOME 46)
- **Build tooling:** [`live-build`](https://salsa.debian.org/live-team/live-build)
- **Customization:** Vyro glassmorphism GNOME theme, Plymouth boot splash,
  GDM login theme, installer slideshow, default app set
- **Output:** `vyro-os-7.x-amd64.iso` published as a GitHub Release

```bash
cd path-a-ubuntu-remix/
./build.sh           # produces vyro-os-7.x-amd64.iso
```

Phase plan: [path-a-ubuntu-remix/PHASES.md](path-a-ubuntu-remix/PHASES.md).

---

## Path B — Vyro OS Core (Linux kernel, Vyro userland)

**Goal:** keep everything that makes Vyro OS *Vyro* — compositor, window
manager, libvyro, the 12 native apps, the visual identity — but drop them
on top of the Linux kernel so we get real hardware support for free.

- **Kernel:** Linux 6.x via Buildroot
- **Init:** custom Vyro init / systemd-lite
- **Graphics:** Vyro compositor reworked on DRM/KMS (no Wayland, no X11)
- **Userland:** libvyro reimplemented over Linux syscalls
- **Apps:** the 12 desktop apps recompiled as Linux ELFs

```bash
cd path-b-linux-core/
make                 # produces vyro-core-7.x.img
```

Phase plan: [path-b-linux-core/PHASES.md](path-b-linux-core/PHASES.md).

---

## Path C — Vyro Microkernel (legacy from-scratch kernel)

**Goal:** continue the original research kernel as an honest, scoped
research project — not pretending to be a daily driver, but a real
exploration of "what does it take to write an OS from a 512-byte boot
sector."

- All v1.0 → v6.0 work lives here untouched
- New work tagged as `vC.X.Y` to disambiguate from Path A (`vA.X.Y`) and Path B (`vB.X.Y`)
- Pending tracks from `ROADMAP_V6.md` (real hardware drivers, ARM64 port,
  userspace, security hardening) continue as time allows

```bash
make                 # builds the original from-scratch kernel
make usb             # produces a real 32MB bootable USB image
```

---

## Quick start (pick a path)

```bash
# Path A — Ubuntu remix ISO
cd path-a-ubuntu-remix && ./build.sh

# Path B — Linux + Vyro userland
cd path-b-linux-core && make

# Path C — original from-scratch kernel
make
```

---

## Installation & Running (Path C — the from-scratch desktop)

This is the bootable microkernel at the repo root. It builds to a disk image
and runs in **QEMU**. You need three tools: an `x86_64-elf` cross-compiler,
`nasm`, and `qemu`.

### macOS

1. **Install [Homebrew](https://brew.sh)** (skip if you already have it):
   ```bash
   /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
   ```
2. **Install the toolchain:**
   ```bash
   brew install x86_64-elf-gcc x86_64-elf-binutils nasm qemu make
   ```
3. **Clone and build + run:**
   ```bash
   git clone https://github.com/Gaurav06120714/VyroOs.git
   cd VyroOs
   make           # builds build/vyro.img
   make all       # builds and launches the desktop in QEMU
   ```

### Windows

The cross toolchain is easiest inside **WSL2** (Windows Subsystem for Linux).

1. **Install WSL2 + Ubuntu** (in an *Administrator* PowerShell, then reboot):
   ```powershell
   wsl --install -d Ubuntu
   ```
2. **Open the Ubuntu terminal and install the tools:**
   ```bash
   sudo apt update
   sudo apt install -y build-essential nasm qemu-system-x86 make git
   ```
3. **Install the `x86_64-elf` cross-compiler.** Ubuntu has no apt package for
   it, so either build it once with the helper below, or — because the WSL
   host is already x86_64 — use the native compiler as a drop-in:
   ```bash
   # Quick path: use the native toolchain (works on x86_64 Linux/WSL hosts)
   make CC=gcc LD=ld
   make CC=gcc LD=ld all
   ```
   If that errors, install a real cross-compiler following the
   [OSDev GCC Cross-Compiler guide](https://wiki.osdev.org/GCC_Cross-Compiler)
   (target `x86_64-elf`), then just run `make` / `make all`.
4. **Clone and build + run:**
   ```bash
   git clone https://github.com/Gaurav06120714/VyroOs.git
   cd VyroOs
   make           # or: make CC=gcc LD=ld
   make all       # launches the desktop in QEMU
   ```
   > A QEMU window needs an X server on WSL2. Windows 11 (WSLg) shows it
   > automatically. On Windows 10, install [VcXsrv](https://sourceforge.net/projects/vcxsrv/)
   > and `export DISPLAY=:0` before `make all`.

### Using the desktop

The guest uses a PS/2 mouse, so QEMU grabs the pointer on first click:

1. **Click once inside the Vyro OS window** to grab the mouse + keyboard.
2. Move the mouse and type — the cursor tracks and keys register.
3. Press **Ctrl + Alt + G** to release the mouse back to your host.

It boots straight to the desktop (no login). Click dock icons to open apps,
right-click the desktop for the context menu.

### Common make targets

| Command | What it does |
|---------|--------------|
| `make` | Build `build/vyro.img` |
| `make all` | Build and run in QEMU (graphical window) |
| `make usb` | Build a 32 MB bootable USB image (`build/vyro-usb.img`) |
| `make clean` | Remove all build artifacts |

---

## Why three paths instead of one?

Honest answer:

- **Path A** is what gets Vyro OS into people's hands this month. Real
  hardware, real apps, real WiFi. The kernel underneath is Linux but the
  product on screen is unmistakably Vyro.
- **Path B** is the long-term product — the Linux kernel handles the
  20-million-line problem of supporting modern hardware, and we spend our
  budget on the userland and design that actually differentiates Vyro.
- **Path C** is the from-scratch microkernel research project. It is not
  going to boot Chrome on Apple Silicon any time soon, and the v6.0
  `AUDIT.md` is honest about why (~116 person-months remaining). But it's
  real code, it boots, and it stays.

Every "indie OS" that actually shipped to users (elementary, Pop!_OS,
Zorin, Garuda, Mint, Endeavour) is an Ubuntu/Arch/Fedora derivative with
custom userland. Path A + B follows that template. Path C keeps the
microkernel dream alive without pretending it's ready for daily use.

---

## Versioning across paths

| Path | Tag prefix | Example |
|------|-----------|---------|
| Path A — Ubuntu remix | `vA.X.Y` | `vA.7.0` = first Ubuntu remix release |
| Path B — Linux+userland | `vB.X.Y` | `vB.0.1` = first Linux-kernel boot |
| Path C — microkernel | `vC.X.Y` | `vC.6.1` = continuation of v6.0 work |
| Unified meta-release | `vX.Y` | `v7.0` = "tri-path launch" |

The 80 historical tags `v0.1` … `v6.0` are kept as-is. New tags follow the
prefix scheme so the three product lines can move at independent cadence.

---

## Repository layout

```
.
├── README.md               # this file
├── RELEASE_NOTES.md        # per-release changelog
├── Makefile                # Path C build entry
├── link.ld                 # Path C linker script
│
├── path-a-ubuntu-remix/    # Path A — Ubuntu 24.04 LTS remix
│   ├── PHASES.md
│   ├── apps/               # in-tree .deb packages
│   ├── branding/           # Plymouth, GDM, Calamares, wallpapers, logos
│   ├── live-build/         # live-build configuration
│   ├── pages/              # gh-pages landing site
│   ├── scripts/            # build helpers
│   ├── theme/              # GNOME + GTK4 glassmorphism theme
│   └── build.sh
│
├── path-b-linux-core/      # Path B — Linux kernel + Vyro userland
│   ├── PHASES.md
│   ├── apps/               # native Vyro desktop apps
│   ├── buildroot/          # Buildroot config + Vyro packages
│   ├── compositor-drm/     # DRM/KMS display server
│   ├── init/               # vyro-init (PID 1)
│   ├── libvyro-linux/      # client library + IPC + font
│   └── Makefile
│
├── path-c-microkernel/     # Path C — from-scratch microkernel (pointer)
│   └── README.md           # points at root-level boot/, kernel/, drivers/...
│
├── boot/                   # Path C: bootloaders (BIOS + UEFI)
├── kernel/                 # Path C: 64-bit kernel + subsystems
├── drivers/                # Path C: hardware drivers
├── include/                # Path C: shared headers
├── user/                   # Path C: sample userland
│
├── docs/                   # shared documentation
│   ├── ARCHITECTURE.md
│   ├── CHANGELOG.md
│   ├── CROSS_PATH.md
│   ├── DESIGN.md / DESIGN-2.0.md
│   ├── PROJECT_STATE.md
│   ├── ROADMAP_V7.md
│   ├── USB_INSTALL.md
│   ├── projects_guide.md
│   └── path-c/             # Path C-specific docs
│       ├── AUDIT.md
│       ├── ROADMAP.md
│       └── ROADMAP_V6.md
│
└── .github/workflows/      # CI (ISO build, APT publish)
```

---

## Documentation

- [docs/ROADMAP_V7.md](docs/ROADMAP_V7.md) — full tri-path roadmap with phase-by-phase plan
- [docs/CROSS_PATH.md](docs/CROSS_PATH.md) — what's shared and what isn't across the three paths
- [path-a-ubuntu-remix/PHASES.md](path-a-ubuntu-remix/PHASES.md) — Path A delivery plan
- [path-b-linux-core/PHASES.md](path-b-linux-core/PHASES.md) — Path B delivery plan
- [path-c-microkernel/README.md](path-c-microkernel/README.md) — Path C pointer to root-level sources
- [docs/path-c/AUDIT.md](docs/path-c/AUDIT.md) — honest technical audit of the microkernel (Path C)
- [docs/path-c/ROADMAP_V6.md](docs/path-c/ROADMAP_V6.md) — Path C real-hardware roadmap (4 tracks, ~116 PM)
- [docs/USB_INSTALL.md](docs/USB_INSTALL.md) — installing the Path C microkernel from USB

## License

MIT.
