#!/usr/bin/env bash
# Vyro OS — Path A — generate Plymouth runtime assets
#
# vyro.script references three PNGs:
#   vyro-mark.png   — rasterized from logos/vyro-mark.svg (handled by branding hook)
#   vyro-halo.png   — soft accent glow behind the mark         (generated here)
#   dot.png         — small progress indicator dot              (generated here)
#
# We synthesize the halo + dot procedurally with ImageMagick (`convert`)
# so they pick up the exact Vyro accent #B388FF without a designer in the
# loop. ImageMagick is in the live-build chroot via librsvg/imagemagick
# pulled in by Plymouth's own deps.

set -euo pipefail

OUT_DIR="${1:?usage: $0 <output-dir>}"
ACCENT="#B388FF"
ACCENT_DARK="#6A48CC"

mkdir -p "${OUT_DIR}"

# --- vyro-halo.png — 384x384 radial gradient, transparent edge ---
# A gradient from accent at center to transparent. We get there by
# generating a radial gradient (white→black) then colorizing the white
# part to accent and using the value as alpha.
if command -v convert >/dev/null 2>&1; then
    convert -size 384x384 \
        radial-gradient:"${ACCENT}-none" \
        -channel A -evaluate Multiply 0.85 +channel \
        "${OUT_DIR}/vyro-halo.png"

    # --- dot.png — 12x12 filled circle, accent color, transparent bg ---
    convert -size 12x12 xc:none \
        -fill "${ACCENT}" -draw "circle 6,6 6,1" \
        "${OUT_DIR}/dot.png"

    echo "make-plymouth-assets: wrote vyro-halo.png + dot.png to ${OUT_DIR}"
else
    # Fallback: 1x1 transparent PNG stubs so Plymouth still loads even
    # without ImageMagick (degrades to no-halo + no-dots cosmetically).
    PNG_STUB='\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR\x00\x00\x00\x01\x00\x00\x00\x01\x08\x06\x00\x00\x00\x1f\x15\xc4\x89\x00\x00\x00\rIDATx\x9cc\xfc\xff\xff?\x03\x00\x05\xfe\x02\xfe\xdc\xcc\x59\xe7\x00\x00\x00\x00IEND\xaeB`\x82'
    printf "${PNG_STUB}" > "${OUT_DIR}/vyro-halo.png"
    printf "${PNG_STUB}" > "${OUT_DIR}/dot.png"
    echo "make-plymouth-assets: warning — convert not found, wrote 1x1 PNG stubs"
fi
