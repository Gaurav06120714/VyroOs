#!/usr/bin/env bash
# Launch Vyro OS in QEMU with the mouse-grab disabled.
#
# vC.6.12.4: the default QEMU pointer is a PS/2 mouse with RELATIVE
# motion — QEMU has to grab your host cursor to deliver those events,
# and on macOS Cocoa releasing the grab requires Ctrl+Option+G which
# is unreliable on some keyboard layouts and leaves users staring at
# a stuck cursor. Adding -usb -device usb-tablet switches to an
# absolute-position pointer; QEMU never has to grab the host mouse,
# you can move freely in and out of the window, and clicking inside
# the window still delivers keystrokes to the guest.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMG="${ROOT}/build/vyro.img"

if [[ ! -f "${IMG}" ]]; then
    echo "error: ${IMG} not found — run 'make build' first" >&2
    exit 1
fi

# Make sure no other Vyro QEMU is already grabbing input.
pkill -f "qemu-system-x86_64.*vyro.img" 2>/dev/null || true
sleep 0.5

exec qemu-system-x86_64 \
    -drive file="${IMG}",format=raw,if=ide,index=0,media=disk \
    -m 256M \
    -smp 1 \
    -cpu max \
    -vga std \
    -name "Vyro OS" \
    -display cocoa,show-cursor=on \
    -usb \
    -device usb-tablet \
    -no-reboot \
    "$@"
