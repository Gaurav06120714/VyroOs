# Path C — Vyro Microkernel

This directory is the symmetric home for **Path C** in the tri-path layout,
matching `path-a-ubuntu-remix/` and `path-b-linux-core/`.

The actual kernel source tree lives at the **repository root** in:

```
../boot/        Bootloaders (BIOS + UEFI)
../kernel/      64-bit kernel + subsystems
../drivers/     Hardware drivers (PIC, timer, screen, keyboard, mouse,
                framebuffer, rtc, speaker, RTL8139, E1000)
../include/     Shared headers
../user/        Userland sample programs + libvyro framework
../Makefile     Build (also produces `make usb` bootable image)
../link.ld      Linker script
```

The kernel sources are *not* moved into this directory because doing so
would require rewriting every `#include "../include/types.h"` across
~50 source files, every relative path in the top-level `Makefile`, and
every `.ld` reference — for a purely cosmetic gain. The historical
v0.1 → v6.0 tags also point at the existing paths and we want
`git checkout vC.6.0` to still build cleanly.

## Build

From the repository root:

```bash
make            # build/vyro.img — 384 KB kernel image
make usb        # build/vyro-usb.img — 32 MB bootable USB image
```

## Docs

- [docs/path-c/AUDIT.md](../docs/path-c/AUDIT.md) — honest gap analysis
- [docs/path-c/ROADMAP.md](../docs/path-c/ROADMAP.md) — legacy roadmap
- [docs/path-c/ROADMAP_V6.md](../docs/path-c/ROADMAP_V6.md) — v6 real-hardware roadmap
- [docs/USB_INSTALL.md](../docs/USB_INSTALL.md) — install from USB

## Source-of-truth for new Path C work

New Path C work continues to land in `../kernel/`, `../drivers/`, etc.,
with tags prefixed `vC.X.Y`. This directory exists so the repo's
top-level `ls` shows three sibling paths and so future contributors
have a single place to look first.
