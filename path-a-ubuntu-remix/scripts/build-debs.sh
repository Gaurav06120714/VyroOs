#!/usr/bin/env bash
# Vyro OS — Path A — build in-tree .deb packages
#
# Builds every app under apps/* into a .deb and drops the result into
# live-build/config/packages.chroot/, where lb_build installs them into
# the live system automatically.
#
# Each app must follow the same layout as apps/hello-vyro:
#   apps/<name>/
#     meson.build         (or other supported buildsystem)
#     debian/control etc. (debian dir with dh sequencer)

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
APPS_DIR="${ROOT_DIR}/apps"
OUT_DIR="${ROOT_DIR}/live-build/config/packages.chroot"
WORK_DIR="${ROOT_DIR}/build/debs"

mkdir -p "${OUT_DIR}" "${WORK_DIR}"

if [[ ! -d "${APPS_DIR}" ]]; then
    echo "==> no apps/ directory, skipping .deb build"
    exit 0
fi

echo "==> building in-tree .debs from ${APPS_DIR}"

shopt -s nullglob
for app_src in "${APPS_DIR}"/*/; do
    name="$(basename "${app_src}")"
    if [[ ! -d "${app_src}/debian" ]]; then
        echo "    skip ${name} — no debian/ directory"
        continue
    fi

    echo "==> ${name}"
    work="${WORK_DIR}/${name}"
    rm -rf "${work}"
    cp -r "${app_src}" "${work}"

    (
        cd "${work}"
        # Build a binary .deb without source-package signing
        debuild -us -uc -b
    )

    # Move the produced .deb(s) into packages.chroot
    parent="$(dirname "${work}")"
    found=0
    for deb in "${parent}/${name}"_*_*.deb; do
        if [[ -f "${deb}" ]]; then
            mv "${deb}" "${OUT_DIR}/"
            echo "    -> $(basename "${deb}") -> packages.chroot/"
            found=1
        fi
    done
    if [[ ${found} -eq 0 ]]; then
        echo "    warn: no .deb produced for ${name}" >&2
    fi
done

echo "==> all in-tree .debs built; $(ls -1 "${OUT_DIR}"/*.deb 2>/dev/null | wc -l) total in packages.chroot/"
