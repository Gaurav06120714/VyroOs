# Vyro Theme (Path A)

Glassmorphism GNOME theme for the Ubuntu remix.

## Design tokens

| Token | Dark | Light |
|-------|------|-------|
| `--vyro-bg` | `#0B0B12` | `#F4F5FA` |
| `--vyro-surface` | `rgba(20, 22, 32, 0.55)` | `rgba(255, 255, 255, 0.55)` |
| `--vyro-surface-strong` | `rgba(20, 22, 32, 0.78)` | `rgba(255, 255, 255, 0.78)` |
| `--vyro-fg` | `#E8E9F1` | `#10131C` |
| `--vyro-fg-muted` | `#A0A4B6` | `#5B5F72` |
| `--vyro-accent` | `#B388FF` | `#7E5AE0` |
| `--vyro-accent-hover` | `#C7A6FF` | `#6A48CC` |
| `--vyro-border` | `rgba(255, 255, 255, 0.08)` | `rgba(0, 0, 0, 0.08)` |
| `--vyro-radius` | `14px` | `14px` |
| `--vyro-blur` | `24px` | `24px` |
| `--vyro-shadow` | `0 12px 40px rgba(0,0,0,0.45)` | `0 12px 40px rgba(0,0,0,0.10)` |

## Files

- `gnome-shell/gnome-shell.css` — top panel, calendar, overview, dash, OSD
- `gnome-shell/gnome-shell-dark.css` — dark variant overrides
- `gtk-4.0/gtk.css` — Files, Settings, Calculator, Text Editor
- `gtk-4.0/gtk-dark.css` — dark variant overrides

## Install (manual, for development)

```bash
sudo cp -r gnome-shell /usr/share/themes/Vyro/
sudo cp -r gtk-4.0     /usr/share/themes/Vyro/
gsettings set org.gnome.shell.extensions.user-theme name 'Vyro'
gsettings set org.gnome.desktop.interface gtk-theme 'Vyro'
gsettings set org.gnome.desktop.interface color-scheme 'prefer-dark'
```

## Install (in live-build)

This directory is copied into `/usr/share/themes/Vyro/` by a chroot hook
in `live-build/config/hooks/normal/0030-vyro-theme.hook.chroot` (Phase A2).
