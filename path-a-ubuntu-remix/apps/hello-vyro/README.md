# hello-vyro

A real GTK4 application demonstrating the Vyro glassmorphism design tokens
on a stock GTK widget tree. Bundled with the Vyro OS Path A ISO as a
starting point for native app development.

## Build & run locally

```bash
sudo apt install meson libgtk-4-dev
meson setup builddir
meson compile -C builddir
./builddir/hello-vyro
```

## Build a .deb

```bash
sudo apt install debhelper devscripts
cd hello-vyro/
debuild -us -uc -b
# → ../hello-vyro_0.1.0-1_amd64.deb
```

## What it shows

- `.vyro-card` — the canonical glassmorphism surface (78% dark fill, 8%
  white border, 14px radius, 12px drop shadow, padding 32px)
- `.vyro-title` — Inter 22pt 600 in `#E8E9F1`
- `.vyro-subtitle` — Inter 11pt in `#A0A4B6`
- `suggested-action` button — picks up the Vyro accent from the system
  theme (vA.7.1), no app-side override needed
- `entry` — system-styled translucent input with focus-purple border

## How it ends up in the ISO

A live-build hook (planned vA.7.7) builds the .deb from this source tree
and includes it in `live-build/config/packages.chroot/`, where
`live-build` installs it automatically into the live system. The desktop
entry registers it in Activities under "Hello, Vyro".
