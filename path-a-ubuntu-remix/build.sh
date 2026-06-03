#!/usr/bin/env bash
# Vyro OS — Path A — Ubuntu Remix builder
#
# Produces build/vyro-os-7.x-amd64.iso from the live-build configuration
# in ./live-build/. Requires an Ubuntu 24.04 host or container with
# live-build, debootstrap, squashfs-tools, and xorriso installed.

set -euo pipefail

VYRO_VERSION="${VYRO_VERSION:-7.0}"
VYRO_CODENAME="${VYRO_CODENAME:-Aurora}"
UBUNTU_SUITE="${UBUNTU_SUITE:-noble}"
ARCH="${ARCH:-amd64}"

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
LB_DIR="${ROOT_DIR}/live-build"
OUT_ISO="${BUILD_DIR}/vyro-os-${VYRO_VERSION}-${ARCH}.iso"

require() {
    command -v "$1" >/dev/null 2>&1 || {
        echo "error: missing required tool '$1'" >&2
        echo "       sudo apt install live-build debootstrap squashfs-tools xorriso" >&2
        exit 1
    }
}

main() {
    echo "==> Vyro OS ${VYRO_VERSION} (${VYRO_CODENAME}) — Path A — Ubuntu Remix"
    echo "    base: ubuntu ${UBUNTU_SUITE} ${ARCH}"
    echo "    out:  ${OUT_ISO}"
    echo

    require lb
    require debootstrap
    require mksquashfs
    require xorriso

    mkdir -p "${BUILD_DIR}"

    echo "==> prepare includes.chroot from theme/ and branding/"
    "${ROOT_DIR}/scripts/prepare-includes.sh"

    cd "${LB_DIR}"

    echo "==> lb clean"
    lb clean --purge

    echo "==> lb config"
    lb config

    echo "==> lb build (this can take 30-90 minutes)"
    lb build

    if [[ -f live-image-${ARCH}.hybrid.iso ]]; then
        mv "live-image-${ARCH}.hybrid.iso" "${OUT_ISO}"
        sha256sum "${OUT_ISO}" > "${OUT_ISO}.sha256"
        echo
        echo "==> OK: ${OUT_ISO}"
        echo "    $(du -h "${OUT_ISO}" | cut -f1) — sha256: $(cut -d' ' -f1 < "${OUT_ISO}.sha256")"
    else
        echo "error: live-build did not produce an ISO" >&2
        exit 1
    fi
}

main "$@"
