#!/usr/bin/env bash
# Vyro OS — Path A — prepare includes.chroot
#
# Mirrors the editable source trees (theme/, branding/) into the live-build
# includes.chroot tree so the chroot hooks find their assets at runtime.
#
# Layout produced under live-build/config/includes.chroot/:
#   usr/share/themes/Vyro/                 <- theme/{gnome-shell,gtk-4.0,index.theme}
#   usr/share/plymouth/themes/vyro/        <- branding/plymouth/{vyro.plymouth,vyro.script}
#   usr/share/vyro-staging/logos/          <- branding/logos/*.svg (for rsvg-convert at hook time)
#   usr/share/vyro-staging/wallpapers/     <- branding/wallpapers/*.svg + *.xml
#   usr/share/vyro-staging/gdm/            <- branding/gdm/vyro-gdm.css
#   usr/share/vyro-staging/installer/      <- branding/installer/{branding.desc,show.qml}
#   etc/calamares/branding/vyro/           <- (populated at hook time from staging)

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
INC="${ROOT_DIR}/live-build/config/includes.chroot"

echo "==> mirroring sources into includes.chroot"

# Theme tree
install -d "${INC}/usr/share/themes/Vyro"
cp -r "${ROOT_DIR}/theme/gnome-shell" "${INC}/usr/share/themes/Vyro/"
cp -r "${ROOT_DIR}/theme/gtk-4.0"     "${INC}/usr/share/themes/Vyro/"
cp    "${ROOT_DIR}/theme/index.theme" "${INC}/usr/share/themes/Vyro/"

# Plymouth theme (PNG assets generated at hook time)
install -d "${INC}/usr/share/plymouth/themes/vyro"
cp "${ROOT_DIR}/branding/plymouth/vyro.plymouth" "${INC}/usr/share/plymouth/themes/vyro/"
cp "${ROOT_DIR}/branding/plymouth/vyro.script"   "${INC}/usr/share/plymouth/themes/vyro/"

# Staging tree consumed by hooks
install -d "${INC}/usr/share/vyro-staging/logos"
install -d "${INC}/usr/share/vyro-staging/wallpapers"
install -d "${INC}/usr/share/vyro-staging/gdm"
install -d "${INC}/usr/share/vyro-staging/installer"
install -d "${INC}/usr/share/vyro-staging/scripts"

cp "${ROOT_DIR}/branding/logos/"*.svg          "${INC}/usr/share/vyro-staging/logos/"
cp "${ROOT_DIR}/branding/wallpapers/"*.svg     "${INC}/usr/share/vyro-staging/wallpapers/"
cp "${ROOT_DIR}/branding/wallpapers/"*.xml     "${INC}/usr/share/vyro-staging/wallpapers/"
cp "${ROOT_DIR}/branding/gdm/vyro-gdm.css"     "${INC}/usr/share/vyro-staging/gdm/"
cp "${ROOT_DIR}/branding/installer/"*.desc     "${INC}/usr/share/vyro-staging/installer/"
cp "${ROOT_DIR}/branding/installer/"*.qml      "${INC}/usr/share/vyro-staging/installer/"
# vA.7.10: branding scripts (halo + dot generators etc.)
install -m 0755 "${ROOT_DIR}/branding/scripts/"*.sh "${INC}/usr/share/vyro-staging/scripts/"

echo "==> includes.chroot ready: $(find "${INC}" -type f | wc -l) files"
