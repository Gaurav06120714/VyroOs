# Path A — Vyro OS Desktop (Ubuntu Remix)

Real, installable, daily-drivable. Ubuntu 24.04 LTS underneath, Vyro on top.

## Stack

- **Base:** Ubuntu 24.04 LTS (Noble Numbat), kernel 6.8, GNOME 46
- **Build tool:** `live-build` (Debian/Ubuntu official live-system builder)
- **Installer:** Ubiquity (default) or Calamares (richer branding)
- **Theme engine:** GNOME Shell extensions + GTK4 CSS
- **Boot:** GRUB 2 (BIOS + UEFI), Plymouth splash

## Output

```
build/vyro-os-7.x-amd64.iso       (~3 GB, bootable on any x86_64 PC)
build/vyro-os-7.x-amd64.iso.sha256
```

## Phases (each ships as `vA.7.X`)

| Phase | Tag | Deliverable | Verification |
|-------|-----|-------------|--------------|
| A0 | vA.7.0 | live-build skeleton, build.sh, this PHASES.md | `ls -la` shows scaffold |
| A1 | vA.7.1 | `auto/config` selects Ubuntu 24.04 + package set | `lb config` succeeds |
| A2 | vA.7.2 | GNOME shell glassmorphism theme (dark + light) | render in stock GNOME |
| A3 | vA.7.3 | GTK4 + GTK3 theme + Vyro icon theme | Files/Settings restyled |
| A4 | vA.7.4 | Plymouth splash (animated logo) | `plymouthd --debug` preview |
| A5 | vA.7.5 | GDM login theme (glass card, Vyro logo) | gdm-screenshot |
| A6 | vA.7.6 | Calamares installer with Vyro slides + branding | run installer in VM |
| A7 | vA.7.7 | Default app set — VyroBrowser, Files, Terminal, TextEdit, Settings, App Store | apps in Activities |
| A8 | vA.7.8 | GitHub Actions ISO build pipeline → Release | green CI, ISO downloadable |
| A9 | vA.7.9 | QEMU + VirtualBox + real-hardware test pass | boot to desktop on all three |
| A10 | vA.7.10 | Public announcement, screenshots in README | README has working ISO link |

## Build

```bash
# Requires: Ubuntu 24.04 host (or container), live-build, debootstrap
sudo apt install live-build debootstrap squashfs-tools xorriso

cd path-a-ubuntu-remix/
./build.sh
# → build/vyro-os-7.x-amd64.iso
```

## Why live-build

- Official Debian/Ubuntu tooling, maintained, well-documented
- Produces real hybrid ISOs (BIOS + UEFI from one image)
- Configuration is plain shell + apt package lists — easy to version-control
- Same tool elementary OS used for its first releases

## Honest deferrals

- ARM64 / Apple Silicon build is **out of scope for Path A**. Asahi Linux
  is the closest existing project; we may add an Asahi-based variant later.
- The bundled `VyroBrowser` in Path A is **Firefox or Chromium with Vyro
  branding**, not the from-scratch HTML renderer from Path C. The from-
  scratch renderer is preserved in Path C.
- No proprietary drivers shipped by default; user installs them via Software & Updates.
