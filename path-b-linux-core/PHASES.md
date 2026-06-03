# Path B — Vyro OS Core (Linux kernel + Vyro userland)

The long-term product. Linux 6.x handles the 20-million-line hardware
problem; we own everything above it.

## Stack

- **Kernel:** Linux 6.x (LTS, via Buildroot)
- **Bootloader:** GRUB 2 / systemd-boot
- **Init:** custom Vyro init (PID 1, no systemd dependency at first)
- **libc:** musl
- **Graphics:** DRM/KMS direct — no Wayland, no X11
- **Compositor:** Vyro compositor reworked on `/dev/dri/card0`
- **Input:** evdev `/dev/input/event*`
- **Audio:** ALSA → PipeWire later
- **Apps:** the 12 Vyro desktop apps recompiled as static Linux ELFs

## Output

```
build/vyro-core-7.x.img          (~200 MB raw bootable disk image)
build/vyro-core-7.x-rootfs.tar   (rootfs tarball for chroot testing)
```

## Phases (each ships as `vB.0.X`)

| Phase | Tag | Deliverable | Verification |
|-------|-----|-------------|--------------|
| B0 | vB.0.0 | Buildroot skeleton, BR2_EXTERNAL tree, defconfig | `make menuconfig` works |
| B1 | vB.0.1 | Linux 6.x boots to Vyro init shell over framebuffer | login prompt in QEMU |
| B2 | vB.0.2 | libvyro-linux: syscall wrappers over musl | hello.c links + runs |
| B3 | vB.0.3 | compositor-drm: open card0, dumb buffer, clear screen | colored screen on tty |
| B4 | vB.0.4 | Window manager renders desktop background + a window | desktop visible |
| B5 | vB.0.5 | Input via evdev — mouse + keyboard | cursor moves, keys type |
| B6 | vB.0.6 | First 4 apps ported (Files, Terminal, TextEdit, Calculator) | apps launchable |
| B7 | vB.0.7 | Remaining 8 apps ported | full app suite |
| B8 | vB.0.8 | Networking via Linux sockets | curl works |
| B9 | vB.0.9 | Audio via ALSA | aplay tone works |
| B10 | vB.0.10 | First public `vyro-core-7.x.img` | boots on real hardware |

## Build

```bash
# Requires: Linux host with build-essential, ncurses-dev, git
cd path-b-linux-core/
make                              # full build (45-90 minutes first time)
make qemu                         # boot the image in QEMU
```

## Architecture

```
+---------------------------------------------------+
|   Vyro Desktop Apps (Files, Terminal, ...)       |   ELF binaries
+---------------------------------------------------+
|   Vyro Window Manager + Compositor (DRM/KMS)     |   single process
+---------------------------------------------------+
|   libvyro-linux  (window, draw, input, IPC)      |   shared lib
+---------------------------------------------------+
|   musl libc                                       |
+---------------------------------------------------+
|   Vyro init  (PID 1, service supervisor)         |
+---------------------------------------------------+
|   Linux kernel 6.x  (drivers, DRM, evdev, ALSA)  |
+---------------------------------------------------+
```

The key design choice: **no display server**. The Vyro compositor is the
display server. It opens DRM/KMS directly, allocates GBM/dumb buffers, and
flips pages. This is the same approach as Weston in headless mode, but
without the Wayland protocol layer.

## Honest deferrals

- No GPU acceleration in B0-B5. Mesa/EGL integration is post-B5.
- Vyro init is **not** a systemd replacement for general Linux use; it's
  just enough to bring up the compositor and a few services. We may
  switch to systemd or s6 later if practical.
- No package manager in the first 10 phases. The image is static.
