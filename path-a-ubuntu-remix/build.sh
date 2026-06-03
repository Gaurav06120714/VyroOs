#!/usr/bin/env bash
# Vyro OS — Path A — Ubuntu Remix builder
#
# Produces build/vyro-os-7.x-amd64.iso from the live-build configuration
# in ./live-build/. Requires an Ubuntu 24.04 host or container with
# live-build, debootstrap, squashfs-tools, and xorriso installed.

set -euo pipefail

VYRO_VERSION="${VYRO_VERSION:-7.1}"
VYRO_CODENAME="${VYRO_CODENAME:-Aurora}"
UBUNTU_SUITE="${UBUNTU_SUITE:-noble}"
ARCH="${ARCH:-amd64}"

# Resolve symlinks too so sudo -E works even when invoked via a path that
# crosses symlink boundaries (the GH Actions checkout under /home/runner
# is sometimes a symlink to /__w/...).
ROOT_DIR="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
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

# Some apt-installed binaries don't end up on the PATH `sudo` resets to
# (notably anything under /sbin). Always include the system paths.
export PATH="/usr/sbin:/sbin:/usr/local/sbin:${PATH}"

main() {
    echo "==> Vyro OS ${VYRO_VERSION} (${VYRO_CODENAME}) — Path A — Ubuntu Remix"
    echo "    base: ubuntu ${UBUNTU_SUITE} ${ARCH}"
    echo "    root: ${ROOT_DIR}"
    echo "    out:  ${OUT_ISO}"
    echo

    require lb
    require debootstrap
    require mksquashfs
    require xorriso

    if [[ "$(id -u)" -ne 0 ]]; then
        echo "error: live-build requires root (re-run under sudo)" >&2
        exit 1
    fi

    mkdir -p "${BUILD_DIR}"

    echo "==> prepare includes.chroot from theme/ and branding/"
    "${ROOT_DIR}/scripts/prepare-includes.sh"

    echo "==> build in-tree .deb packages from apps/"
    if ! "${ROOT_DIR}/scripts/build-debs.sh"; then
        echo "warn: in-tree .deb build failed, continuing without bundled apps" >&2
    fi

    cd "${LB_DIR}"

    echo "==> lb clean"
    lb clean --purge || true

    echo "==> lb config"
    ARCH="${ARCH}" lb config

    echo "==> lb build (this can take 30-90 minutes)"
    lb build

    # live-build output name varies by binary-image type. Try the
    # common shapes in order, plus a generic fallback that picks up
    # whatever .iso landed in the live-build dir.
    SRC_ISO=""
    for cand in "live-image-${ARCH}.hybrid.iso" "live-image-${ARCH}.iso" \
                "vyro-os-${VYRO_VERSION}-${ARCH}.iso"; do
        if [[ -f "${cand}" ]]; then SRC_ISO="${cand}"; break; fi
    done
    if [[ -z "${SRC_ISO}" ]]; then
        # Glob fallback — first .iso file in the working dir.
        shopt -s nullglob
        candidates=( ./*.iso )
        shopt -u nullglob
        if [[ ${#candidates[@]} -gt 0 ]]; then SRC_ISO="${candidates[0]}"; fi
    fi

    if [[ -n "${SRC_ISO}" && -f "${SRC_ISO}" ]]; then
        mv "${SRC_ISO}" "${OUT_ISO}"
        sha256sum "${OUT_ISO}" > "${OUT_ISO}.sha256"
        echo
        echo "==> OK: ${OUT_ISO}"
        echo "    $(du -h "${OUT_ISO}" | cut -f1) — sha256: $(cut -d' ' -f1 < "${OUT_ISO}.sha256")"
    else
        echo "error: live-build did not produce an ISO" >&2
        echo "       contents of $(pwd):" >&2
        ls -la >&2 || true
        exit 1
    fi
}

main "$@"
