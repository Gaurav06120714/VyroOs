# Cross-path Sharing Model

How the three Vyro OS paths relate to one another in practice.

## Two things are shared. Two things are not.

### Shared #1 — Design system

Every Path uses the same design tokens. Colors, radii, blur amounts,
font choices, traffic-light palette, accent purple — all live in
`docs/DESIGN.md` and are reflected wherever the design surfaces:

| Surface | Where the tokens land |
|---------|-----------------------|
| Path A GNOME Shell theme | `path-a-ubuntu-remix/theme/gnome-shell/gnome-shell.css` |
| Path A GTK4 theme        | `path-a-ubuntu-remix/theme/gtk-4.0/gtk.css` |
| Path A Plymouth splash   | `path-a-ubuntu-remix/branding/plymouth/vyro.script` |
| Path A GDM theme         | `path-a-ubuntu-remix/branding/gdm/vyro-gdm.css` |
| Path A Calamares branding| `path-a-ubuntu-remix/branding/installer/branding.desc` |
| Path B compositor chrome | `path-b-linux-core/compositor-drm/src/chrome.c` |
| Path B apps              | `path-b-linux-core/apps/*/src/main.c` |
| Path C apps              | `kernel/apps/*` |

Result: a screenshot taken on Path A, Path B, or Path C is recognisable
as Vyro OS. The colors and shapes are not negotiable.

### Shared #2 — App logic

Apps follow a "split logic from rendering" rule. The pure arithmetic +
state-machine portion of each app is identical across paths. Only the
input + draw layer changes:

| App        | Logic source-of-truth      | Path A binding | Path B binding | Path C binding |
|------------|----------------------------|----------------|----------------|----------------|
| Calculator | `apply_op`, `key_*` state machine | GTK4 widgets (planned) | `apps/calculator/src/main.c` over libvyro IPC | `kernel/apps/calculator` over int 0x80 |
| Files      | tree walk + entry sort     | Nautilus (rebrand) | `apps/files/src/main.c` over POSIX readdir | `kernel/apps/files` over vyfs |
| TextEdit   | buffer + cursor + undo     | gedit/Text Editor (rebrand) | (planned vB.0.10) | `kernel/apps/textedit` |

Result: porting an app between paths is mostly a rendering swap, not a
ground-up rewrite.

## Two things are not shared.

### Not shared #1 — Display server

| Path | Display server |
|------|----------------|
| A | Mutter (GNOME's compositor on Wayland), exactly as Ubuntu ships it |
| B | `vyro-compositor` — DRM/KMS direct, no X, no Wayland |
| C | Microkernel framebuffer compositor |

These are not interchangeable. A Path B app cannot run under Path A
without `vyro-compositor` also being installed and `/run/vyro/compositor.sock`
being live — and that path is **experimental** under Path A (see below).

### Not shared #2 — Kernel

| Path | Kernel |
|------|--------|
| A | Linux 6.x as shipped by Ubuntu |
| B | Linux 6.x compiled by Buildroot |
| C | Vyro microkernel (from-scratch) |

Path A and Path B share kernel family, but Path A has every desktop and
server module Ubuntu enables, Path B has the minimum Buildroot config
needs.

## Running Path B's compositor under Path A (experimental)

It is possible to install the Path B compositor (`vyro-compositor`,
`libvyro-linux`, `vyro-files`, `vyro-calculator`, `vyro-init`'s session
parts minus the PID 1) as `.deb` packages on top of a Path A install, and
offer a "Vyro Compositor (experimental)" session at the GDM login screen
alongside the default GNOME session.

This is not yet wired up automatically. The scaffolding is:

- `path-a-ubuntu-remix/live-build/config/includes.chroot/usr/share/xsessions/vyro.desktop`
  registers the session with GDM
- A future phase (vA.7.13+) will package the Path B compositor and apps
  as `.deb`s and drop them into `packages.chroot/`

For now, the `.desktop` file is shipped but the binaries it references
are not yet built into the Path A ISO. Choosing the Vyro session will
fall back to the next available session.

## Versioning across paths

| Tag prefix | Path | Cadence |
|-----------|------|---------|
| `vA.X.Y`  | A    | Weekly target |
| `vB.X.Y`  | B    | Monthly target |
| `vC.X.Y`  | C    | When material progress lands |
| `vX.Y`    | Meta | Quarterly rollup when all three paths hit a milestone |

The 80 historical `v0.1`–`v6.0` tags are preserved. New tags use the
prefix scheme so the three product lines move at independent cadence.
