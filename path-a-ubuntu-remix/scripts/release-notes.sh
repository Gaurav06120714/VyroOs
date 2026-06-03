#!/usr/bin/env bash
# Vyro OS — Path A — release-notes generator
#
# Produces a human-readable Markdown release-notes file from the git log
# between the previous vA.* tag and the current one. Designed to be called
# both from CI (to populate the GitHub Release body) and locally.
#
# Usage:
#   release-notes.sh <CURRENT_TAG>            # to stdout
#   release-notes.sh <CURRENT_TAG> <OUT_FILE> # writes file too

set -euo pipefail

CUR="${1:-}"
OUT="${2:-}"

if [[ -z "${CUR}" ]]; then
    echo "usage: $0 <current_tag> [out_file]" >&2
    exit 2
fi

# Find the previous vA.* tag chronologically; ignores the current.
PREV="$(git tag --sort=-creatordate --list 'vA.*' \
        | awk -v cur="${CUR}" 'BEGIN{seen=0} $0==cur{seen=1; next} seen{print; exit}')"

if [[ -z "${PREV}" ]]; then
    RANGE_DESC="initial release"
    RANGE_ARG="${CUR}"
else
    RANGE_DESC="changes since ${PREV}"
    RANGE_ARG="${PREV}..${CUR}"
fi

# Bucket subjects by the conventional prefix Vyro uses
# (vA.X.Y / vB.X.Y / vC.X.Y / v7.X)
declare -A BUCKETS=(
    [path_a]=""
    [path_b]=""
    [path_c]=""
    [meta]=""
    [other]=""
)

while IFS= read -r line; do
    case "${line}" in
        vA.*) BUCKETS[path_a]+="- ${line}"$'\n' ;;
        vB.*) BUCKETS[path_b]+="- ${line}"$'\n' ;;
        vC.*) BUCKETS[path_c]+="- ${line}"$'\n' ;;
        v[0-9]*) BUCKETS[meta]+="- ${line}"$'\n' ;;
        *)    BUCKETS[other]+="- ${line}"$'\n' ;;
    esac
done < <(git log --no-merges --pretty=format:'%s' "${RANGE_ARG}")

emit() {
    cat <<EOF
# Vyro OS ${CUR#vA.}

*${RANGE_DESC}*

EOF
    if [[ -n "${BUCKETS[path_a]}" ]]; then
        echo "## Path A — Ubuntu Remix"
        echo
        echo "${BUCKETS[path_a]}"
    fi
    if [[ -n "${BUCKETS[path_b]}" ]]; then
        echo "## Path B — Linux + Vyro userland"
        echo
        echo "${BUCKETS[path_b]}"
    fi
    if [[ -n "${BUCKETS[path_c]}" ]]; then
        echo "## Path C — Microkernel"
        echo
        echo "${BUCKETS[path_c]}"
    fi
    if [[ -n "${BUCKETS[meta]}" ]]; then
        echo "## Cross-path"
        echo
        echo "${BUCKETS[meta]}"
    fi
    if [[ -n "${BUCKETS[other]}" ]]; then
        echo "## Other"
        echo
        echo "${BUCKETS[other]}"
    fi
}

if [[ -n "${OUT}" ]]; then
    emit | tee "${OUT}"
else
    emit
fi
