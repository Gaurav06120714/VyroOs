#!/usr/bin/env bash
# Vyro OS — Path A — build a flat GitHub-Pages-ready Debian repo from
# every .deb in live-build/config/packages.chroot/ plus any extras the
# caller hands us.
#
# Output layout (under $OUT_DIR, default ./build/apt):
#
#   ./apt/
#       dists/noble/main/binary-amd64/Packages   (+ Packages.gz)
#       dists/noble/main/binary-arm64/Packages   (+ Packages.gz)
#       dists/noble/InRelease                    (signed if VYRO_GPG_KEY set)
#       dists/noble/Release
#       pool/main/v/vyro-compositor/vyro-compositor_0.1.0-1_amd64.deb
#       ...
#       vyro-keyring.gpg                         (if VYRO_GPG_KEY set)
#
# This directory can be served as-is from GitHub Pages and APT will be
# happy. Calling this script does NOT publish — that's a separate step
# (the GH Actions workflow will rsync it into a pages branch).

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
DEBS_DIR="${DEBS_DIR:-${ROOT_DIR}/live-build/config/packages.chroot}"
OUT_DIR="${OUT_DIR:-${ROOT_DIR}/build/apt}"
SUITE="${SUITE:-noble}"
COMP="${COMP:-main}"

if ! command -v dpkg-scanpackages >/dev/null 2>&1; then
    echo "error: dpkg-scanpackages not found (apt-get install dpkg-dev)" >&2
    exit 1
fi

mkdir -p "${OUT_DIR}/dists/${SUITE}/${COMP}/binary-amd64"
mkdir -p "${OUT_DIR}/dists/${SUITE}/${COMP}/binary-arm64"
mkdir -p "${OUT_DIR}/pool/${COMP}"

# Sort .debs into pool/main/<first-letter>/<pkg>/
shopt -s nullglob
for deb in "${DEBS_DIR}"/*.deb; do
    base=$(basename "${deb}")
    # name from "name_version_arch.deb"
    pkg="${base%%_*}"
    first="${pkg:0:1}"
    dest="${OUT_DIR}/pool/${COMP}/${first}/${pkg}"
    mkdir -p "${dest}"
    cp "${deb}" "${dest}/"
done

# Generate Packages indexes per arch
for arch in amd64 arm64; do
    bin_dir="${OUT_DIR}/dists/${SUITE}/${COMP}/binary-${arch}"
    (
        cd "${OUT_DIR}"
        dpkg-scanpackages --arch "${arch}" pool/ > "${bin_dir}/Packages" 2>/dev/null || true
    )
    gzip -9c "${bin_dir}/Packages" > "${bin_dir}/Packages.gz"
done

# Release file
cat > "${OUT_DIR}/dists/${SUITE}/Release" <<EOF
Origin: Vyro OS
Label: Vyro OS APT
Suite: ${SUITE}
Codename: ${SUITE}
Architectures: amd64 arm64
Components: ${COMP}
Date: $(date -u '+%a, %d %b %Y %H:%M:%S +0000')
Description: Vyro-native Debian packages
EOF

# Optional GPG sign if VYRO_GPG_KEY is set
if [[ -n "${VYRO_GPG_KEY:-}" ]]; then
    gpg --batch --yes --default-key "${VYRO_GPG_KEY}" \
        --output "${OUT_DIR}/dists/${SUITE}/Release.gpg" \
        --detach-sign "${OUT_DIR}/dists/${SUITE}/Release"
    gpg --batch --yes --default-key "${VYRO_GPG_KEY}" \
        --output "${OUT_DIR}/dists/${SUITE}/InRelease" \
        --clear-sign "${OUT_DIR}/dists/${SUITE}/Release"
    gpg --batch --yes --export "${VYRO_GPG_KEY}" > "${OUT_DIR}/vyro-keyring.gpg"
fi

echo "==> APT repo built at ${OUT_DIR}"
find "${OUT_DIR}" -type f | head -20
